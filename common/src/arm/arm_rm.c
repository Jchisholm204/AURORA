/**
 * @file arm_rm.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AUORA Region Manager - Region Management
 * @version 0.1
 * @date Created: 2026-03-21
 * @modified Last Modified: 2026-03-21
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL
#include "aci/aci_internal.h"
#include "arm/arm.h"
#include "arm/arm_am.h"
#include "log.h"

#include <string.h>

eARM_error arm_add(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if (!pHndl || pAMR) {
        log_error("NULL parameter");
        return eARM_ERR_NULL;
    }

    if (!pAMR->pActive_memory) {
        log_error("NULL parameter");
        return eARM_ERR_NULL;
    }

    void *shadow_memory = malloc(pAMR->rgn_size);

    if (!shadow_memory) {
        log_error("Bad Malloc");
        return eARM_ERR_NULL;
    }

    amr_hndl *pInt_AMR = _arl_add(&pHndl->local_rgns);

    if (!pInt_AMR) {
        log_error("Failed to create AMR");
        free(shadow_memory);
        return eARM_ERR_NULL;
    }

    (void) memcpy(pInt_AMR, pAMR, sizeof(amr_hndl));

    pInt_AMR->pShadow_memory = (uint64_t) shadow_memory;

    ucp_mem_map_params_t mparam;
    mparam.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mparam.address = (void *) pInt_AMR->pShadow_memory;
    mparam.length = pInt_AMR->rgn_size;
    ucs_status_t ucs_status = UCS_OK;
    ucs_status = aci_mem_map(pHndl->pACI, &mparam, &pInt_AMR->mem_hndl);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        (void) arm_remove(pHndl, pInt_AMR);
        return eARM_ERR_UCS;
    }

    void *rkey_buffer = NULL;
    size_t rkey_size = 0;
    ucs_status = aci_rkey_pack(pHndl->pACI, pInt_AMR->mem_hndl, &rkey_buffer,
                               &rkey_size);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        (void) arm_remove(pHndl, pInt_AMR);
        return eARM_ERR_UCS;
    }

    ucp_request_param_t rparam;

    ucs_status_ptr_t pStatus =
        aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_ADD, pInt_AMR, sizeof(amr_hndl),
                        rkey_buffer, rkey_size, &rparam);
    if (UCS_PTR_IS_ERR(pStatus)) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        (void) arm_remove(pHndl, pInt_AMR);
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pStatus)) {
        do {
            aci_poll(pHndl->pACI);
            ucs_status = ucp_request_check_status(pStatus);

        } while (ucs_status == UCS_INPROGRESS);
    }

    ucp_rkey_buffer_release(rkey_buffer);

    if (ucs_status != UCS_OK) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        (void) arm_remove(pHndl, pInt_AMR);
        return eARM_ERR_UCS;
    }

    return eARM_OK;
}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
}

size_t arm_get_n_regions(arm_hndl *pHndl) {
}

const amr_hndl *arm_get_regions(arm_hndl *pHndl) {
}
