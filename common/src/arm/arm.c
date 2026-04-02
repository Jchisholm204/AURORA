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

arm_hndl *arm_create_instance(aci_hndl *pACI, acn_hndl *pACN) {
    if (!pACI) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!pACN) {
        log_error("NULL Parameter");
        return NULL;
    }
    arm_hndl *pHndl = malloc(sizeof(arm_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->pACI = pACI;
    pHndl->pACN = pACN;
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

    log_debug("Destroying Instance");

    // Clean up the memory regions

    if (pHndl->remote_rgns.data) {
        for (size_t i = 0; i < pHndl->remote_rgns.size; i++) {
            amr_hndl *pAMR = &pHndl->remote_rgns.data[i];
            if (pAMR->active_remote_key) {
                log_trace("destroy rkey 0x%lx", pAMR->active_remote_key);
                ucp_rkey_destroy(pAMR->active_remote_key);
                pAMR->active_remote_key = NULL;
            }
            if (pAMR->active_mem_hndl) {
                log_debug("mem hndl in remote?");
                (void) aci_mem_unmap(pHndl->pACI, pAMR->active_mem_hndl);
                pAMR->active_mem_hndl = NULL;
            }
            if (pAMR->shadow_remote_key) {
                log_debug("destroy rkey 0x%lx", pAMR->shadow_remote_key);
                ucp_rkey_destroy(pAMR->shadow_remote_key);
                pAMR->shadow_remote_key = NULL;
            }
            if (pAMR->shadow_mem_hndl) {
                log_debug("mem hndl in remote?");
                (void) aci_mem_unmap(pHndl->pACI, pAMR->shadow_mem_hndl);
                pAMR->shadow_mem_hndl = NULL;
            }
        }
        free(pHndl->remote_rgns.data);
        pHndl->remote_rgns = (struct aurora_region_list) {0};
    }

    if (pHndl->local_rgns.data) {
        while (pHndl->local_rgns.size > 0) {
            amr_hndl *pAMR = &pHndl->local_rgns.data[0];
            (void) arm_remove(pHndl, pAMR);
        }
        free(pHndl->local_rgns.data);
        pHndl->local_rgns = (struct aurora_region_list) {0};
    }

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
            reallocarray(pList->data, pList->capacity, sizeof(amr_hndl));
    }

    return eARM_OK;
}

eARM_error _arm_find(struct aurora_region_list *pList, amr_hndl **ppAMR,
                     size_t *pInst_idx) {
    if (!pList || !ppAMR) {
        return eARM_ERR_NULL;
    }
    if (!*ppAMR) {
        return eARM_ERR_NULL;
    }

    amr_hndl *pInst_AMR = NULL;
    if (*ppAMR <= pList->data + pList->size && *ppAMR > pList->data) {
        // Use the passed address directly if it is within internal array bounds
        // -> Leave pAMR alone
        *pInst_idx = (size_t) (*ppAMR - pList->data);
        log_trace("Local Instance %d id=%d", *pInst_idx, (*ppAMR)->id);
        return eARM_OK;
    } else {
        // Find matching AMR
        for (*pInst_idx = 0; *pInst_idx < pList->size; (*pInst_idx)++) {
            pInst_AMR = &pList->data[*pInst_idx];
            bool match = true;
            // Compare the name variable
            match &= (strlen((*ppAMR)->name) == 0 ||
                      strcmp(pInst_AMR->name, (*ppAMR)->name));
            // Compare the ID
            match &= (pInst_AMR->id == (*ppAMR)->id);
            // Compare the AM address if its not NULL
            match &= ((*ppAMR)->pActive_memory == pInst_AMR->pActive_memory) ||
                     (!(*ppAMR)->pActive_memory);
            // Do NOT compare the shadow memory (cannot be set by user)
            if (match) {
                *ppAMR = pInst_AMR;
                break;
            }
        }
        if ((*pInst_idx) == pList->size) {
            log_error("Could not find instance");
            *ppAMR = NULL;
            return eARM_ERR_MATCH_NOT_FOUND;
        }
    }
    return eARM_OK;
}
