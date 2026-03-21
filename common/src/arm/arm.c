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
    pHndl->n_regions = 0;
    pHndl->rgn_capacity = ARM_INIT_INSTANCES;
    pHndl->regions = (amr_hndl *) malloc(sizeof(amr_hndl) * ARM_INIT_INSTANCES);

    if (!pHndl->regions) {
        log_error("Failed to create ARM instance list");
        free(pHndl);
        return NULL;
    }

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

    // Clean up the memory regions (start from last to avoid reshuffle)
    for (size_t i = pHndl->n_regions; i > 0; i--) {
        amr_hndl *pAMR = &pHndl->regions[i];
        if (pAMR->remote_key) {
            ucp_rkey_destroy(pAMR->remote_key);
            pAMR->remote_key = NULL;
        }
        if (pAMR->mem_hndl) {
            aci_mem_unmap(pHndl->pACI, pAMR->mem_hndl);
            pAMR->mem_hndl = NULL;
        }
    }

    if (pHndl->regions) {
        free(pHndl->regions);
        pHndl->n_regions = 0;
        pHndl->rgn_capacity = 0;
        pHndl->regions = NULL;
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

// Function to check if an AMR belongs to this handle
eARM_error _arm_check_amr_hndl(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if (!pHndl || !pAMR) {
        return eARM_ERR_NULL;
    }
    const amr_hndl *base_addr = pHndl->regions;
    const amr_hndl *max_addr = pHndl->regions + pHndl->n_regions;
    if (pAMR < base_addr || pAMR > max_addr) {
        return eARM_ERR_MATCH_NOT_FOUND;
    }
    return eARM_OK;
}

eARM_error _arm_add(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if (!pHndl || !pAMR) {
        log_error("ARM Null");
        return eARM_ERR_NULL;
    }

    // Check if the handle already exists in this AMR
    if (_arm_check_amr_hndl(pHndl, pAMR) == eARM_OK) {
        log_debug("Attempted add of region that already exists.");
        return eARM_OK;
    }

    // Reallocate the array if over size
    if (pHndl->n_regions >= pHndl->rgn_capacity) {
        pHndl->rgn_capacity *= 2;
        pHndl->regions =
            reallocarray(pHndl->regions, pHndl->rgn_capacity, sizeof(amr_hndl));
    }

    // Grab the last region and inc the counter
    amr_hndl *pRgn = &pHndl->regions[pHndl->n_regions++];

    // Copy region data into the handles array
    (void) memcpy(pRgn, pAMR, sizeof(amr_hndl));

    return eARM_OK;
}

eARM_error _arm_remove(arm_hndl *pHndl, const size_t amr_index) {
    if (!pHndl) {
        log_debug("ARM NULL");
        return eARM_ERR_NULL;
    }

    if (amr_index > pHndl->n_regions) {
        log_error("Index out of range.");
        return eARM_ERR_MATCH_NOT_FOUND;
    }

    // Free/Remove the Region from ARM
    amr_hndl *pAMR = &pHndl->regions[amr_index];

    // Zero out the rgn
    memset(pAMR, 0, sizeof(amr_hndl));

    // Shuffle the array down
    memmove(pAMR, pAMR + 1,
            (pHndl->n_regions - amr_index - 1) * sizeof(amr_hndl));

    pHndl->n_regions--;

    return eARM_OK;
}
