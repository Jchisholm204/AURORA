/**
 * @file arm.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL
#include "arm/arm.h"

#include "aci/aci_internal.h"
#include "arm/arm_am.h"
#include "log.h"

arm_hndl *arm_create_instance(aci_hndl *pACI) {
    if (!pACI) {
        log_error("ACI was NULL. Failed to create ARM.");
        return NULL;
    }
    arm_hndl *pHndl = malloc(sizeof(arm_hndl));
    if (pHndl) {
        log_error("Failed to create ARM Handle.");
        return NULL;
    }

    pHndl->pACI = pACI;
    pHndl->n_regions = ARM_INIT_INSTANCES;
    pHndl->regions = (amr_hndl *) malloc(sizeof(amr_hndl) * ARM_INIT_INSTANCES);

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
}

eARM_error arm_add(arm_hndl *pHndl, const amr_hndl *pAMR) {
}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
}

size_t arm_get_n_regions(arm_hndl *pHndl) {
}

const amr_hndl *arm_get_regions(arm_hndl *pHndl) {
}
