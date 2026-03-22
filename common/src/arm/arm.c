/**
 * @file arm.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager
 * @version 0.2
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-21
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL
#include "arm/arm.h"

#include "aci/aci_internal.h"
#include "arm/arm_am.h"
#include "log.h"
#include "string.h"

arm_hndl *arm_create_instance(aci_hndl *pACI) {
    if (!pACI) {
        log_error("ACI was NULL. Failed to create ARM.");
        return NULL;
    }
    arm_hndl *pHndl = malloc(sizeof(arm_hndl));
    if (!pHndl) {
        log_error("Failed to create ARM Handle.");
        return NULL;
    }

    pHndl->pACI = pACI;
    _arl_init(&pHndl->local_rgns);
    _arl_init(&pHndl->remote_rgns);

    ucs_status_t ucs_status = UCS_OK;
    ucp_am_handler_param_t am_params;
    am_params.field_mask =
        UCP_AM_HANDLER_PARAM_FIELD_ID | UCP_AM_HANDLER_PARAM_FIELD_CB |
        UCP_AM_HANDLER_PARAM_FIELD_FLAGS | UCP_AM_HANDLER_PARAM_FIELD_ARG;
    am_params.id = ARM_UCX_ID_ADD;
    am_params.cb = _arm_add_rgn_cb;
    am_params.arg = pHndl;
    ucs_status = aci_set_am_recv_handler(pACI, &am_params);

    if (ucs_status != UCS_OK) {
        log_error("Failed to register am handler");
        (void) arm_destroy_instance(&pHndl);
        return NULL;
    }

    am_params.id = ARM_UCX_ID_RM;
    am_params.cb = _arm_rm_rgn_cb;
    am_params.arg = pHndl;
    ucs_status = aci_set_am_recv_handler(pACI, &am_params);

    if (ucs_status != UCS_OK) {
        log_error("Failed to register am handler");
        (void) arm_destroy_instance(&pHndl);
        return NULL;
    }

    return pHndl;
}

eARM_error arm_destroy_instance(arm_hndl **ppHndl) {
    // Assumed that if this is called, the worker is already dead
    if (!ppHndl) {
        log_error("Cannot clean up a NULL handle");
        return eARM_ERR_NULL;
    }
    arm_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        log_error("Cannot clean up a NULL handle");
        return eARM_ERR_NULL;
    }

    // Clean up the memory regions
    _arl_free_local(&pHndl->local_rgns, pHndl->pACI);
    _arl_free_remote(&pHndl->remote_rgns, pHndl->pACI);

    ucs_status_t ucs_status = UCS_OK;
    ucp_am_handler_param_t am_params;
    am_params.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                           UCP_AM_HANDLER_PARAM_FIELD_FLAGS |
                           UCP_AM_HANDLER_PARAM_FIELD_ARG;
    am_params.id = ARM_UCX_ID_ADD;
    am_params.cb = NULL;
    am_params.arg = NULL;
    ucs_status = aci_set_am_recv_handler(pHndl->pACI, &am_params);

    (void) ucs_status;

    // Free the handle itself
    free(pHndl);
    *ppHndl = NULL;

    return eARM_OK;
}

eARM_error _arl_init(struct aurora_region_list *pList) {
    if (!pList) {
        log_debug("NULL argument");
        return eARM_ERR_NULL;
    }
    pList->capacity = ARM_INIT_INSTANCES;
    pList->size = 0;
    pList->data = malloc(ARM_INIT_INSTANCES * sizeof(amr_hndl));
    if (!pList->data) {
        log_error("Failed to allocate ARL");
        return eARM_ERR_NULL;
    }
    (void) memset(pList->data, 0, ARM_INIT_INSTANCES * sizeof(amr_hndl));
    return eARM_OK;
}

eARM_error _arl_free_local(struct aurora_region_list *pList, aci_hndl *pACI) {
    if (!pList || !pACI) {
        log_debug("NULL argument");
        return eARM_ERR_NULL;
    }

    log_debug("Deleting %d local regions.", pList->size);

    for (size_t i = pList->size; i > 0; i--) {
        amr_hndl *pAMR = &pList->data[i];
        if (pAMR->remote_key) {
            ucp_rkey_destroy(pAMR->remote_key);
            pAMR->remote_key = NULL;
        }
        if (pAMR->mem_hndl) {
            aci_mem_unmap(pACI, pAMR->mem_hndl);
            pAMR->mem_hndl = NULL;
        }
        if (pAMR->pActive_memory) {
            free((void *) pAMR->pActive_memory);
            pAMR->pActive_memory = 0;
        }
        if (pAMR->pShadow_memory) {
            free((void *) pAMR->pShadow_memory);
            pAMR->pShadow_memory = 0;
        }
    }

    return eARM_OK;
}

eARM_error _arl_free_remote(struct aurora_region_list *pList, aci_hndl *pACI) {
    if (!pList || !pACI) {
        log_debug("NULL argument");
        return eARM_ERR_NULL;
    }

    log_debug("Deleting %d remote regions.", pList->size);

    for (size_t i = pList->size; i > 0; i--) {
        amr_hndl *pAMR = &pList->data[i];
        if (pAMR->remote_key) {
            ucp_rkey_destroy(pAMR->remote_key);
            pAMR->remote_key = NULL;
        }
        if (pAMR->mem_hndl) {
            log_debug("mem hndl in remote?");
            aci_mem_unmap(pACI, pAMR->mem_hndl);
            pAMR->mem_hndl = NULL;
        }
    }

    return eARM_OK;
}

amr_hndl *_arl_add(struct aurora_region_list *pList) {
    if (!pList) {
        log_error("ARM Null");
        return NULL;
    }

    // Reallocate the array if over size
    if (pList->size >= pList->capacity) {
        pList->capacity *= 2;
        pList->data =
            reallocarray(pList->data, pList->capacity, sizeof(amr_hndl));
    }

    // Grab the last region and inc the counter
    amr_hndl *pRgn = &pList->data[pList->size++];

    return pRgn;
}

eARM_error _arl_remove(struct aurora_region_list *pList,
                       const size_t amr_index) {
    if (!pList) {
        log_debug("ARM NULL");
        return eARM_ERR_NULL;
    }

    if (amr_index > pList->size) {
        log_error("Index out of range.");
        return eARM_ERR_MATCH_NOT_FOUND;
    }

    // Free/Remove the Region from ARM
    amr_hndl *pAMR = &pList->data[amr_index];

    // Zero out the rgn
    memset(pAMR, 0, sizeof(amr_hndl));

    // Shuffle the array down
    memmove(pAMR, pAMR + 1, (pList->size - amr_index - 1) * sizeof(amr_hndl));

    pList->size--;

    // Reduce internal storage size
    if (pList->capacity > (pList->size * 4) &&
        pList->capacity > ARM_INIT_INSTANCES) {
        pList->capacity /= 4;
        pList->data =
            reallocarray(pList->data, sizeof(amr_hndl), pList->capacity);
    }

    return eARM_OK;
}
