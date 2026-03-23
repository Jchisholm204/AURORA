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

    amr_hndl *pInst_AMR = _arl_add(&pHndl->local_rgns);

    if (!pInst_AMR) {
        log_error("Failed to create AMR");
        free(shadow_memory);
        return eARM_ERR_NULL;
    }

    (void) memcpy(pInst_AMR, pAMR, sizeof(amr_hndl));

    pInst_AMR->pShadow_memory = (uint64_t) shadow_memory;

    ucp_mem_map_params_t mparam;
    mparam.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mparam.address = (void *) pInst_AMR->pShadow_memory;
    mparam.length = pInst_AMR->rgn_size;
    ucs_status_t ucs_status = UCS_OK;
    ucs_status = aci_mem_map(pHndl->pACI, &mparam, &pInst_AMR->mem_hndl);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        (void) arm_remove(pHndl, pInst_AMR);
        return eARM_ERR_UCS;
    }

    void *rkey_buffer = NULL;
    size_t rkey_size = 0;
    ucs_status = aci_rkey_pack(pHndl->pACI, pInst_AMR->mem_hndl, &rkey_buffer,
                               &rkey_size);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        (void) arm_remove(pHndl, pInst_AMR);
        return eARM_ERR_UCS;
    }

    ucp_request_param_t rparam;

    ucs_status_ptr_t pStatus =
        aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_ADD, pInst_AMR,
                        sizeof(amr_hndl), rkey_buffer, rkey_size, &rparam);
    if (UCS_PTR_IS_ERR(pStatus)) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        (void) arm_remove(pHndl, pInst_AMR);
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
        (void) arm_remove(pHndl, pInst_AMR);
        return eARM_ERR_UCS;
    }

    return eARM_OK;
}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if (!pHndl || !pAMR) {
        return eARM_ERR_NULL;
    }

    amr_hndl *pInst_AMR = NULL;
    size_t inst_idx = 0;
    if (pAMR < pHndl->local_rgns.data + pHndl->local_rgns.size &&
        pAMR > pHndl->local_rgns.data) {
        // Use the passed address directly if it is within internal array bounds
        pInst_AMR = (amr_hndl *) pAMR;
        inst_idx = (size_t) (pAMR - pHndl->local_rgns.data);
    } else {
        // Find matching AMR
        for (inst_idx = 0; inst_idx > pHndl->local_rgns.size; inst_idx++) {
            pInst_AMR = &pHndl->local_rgns.data[inst_idx];
            bool match = true;
            match &= (strlen(pAMR->name) == 0 ||
                      strcmp(pInst_AMR->name, pAMR->name));
            match &= (pAMR->id == 0 || pInst_AMR->id == pAMR->id);
            match &= (pAMR->pActive_memory == pInst_AMR->pActive_memory);
            if (match) {
                break;
            }
        }
        if(inst_idx == pHndl->local_rgns.size){
            log_error("Could not find the instance to remove.");
            return eARM_ERR_MATCH_NOT_FOUND;
        }
    }
    if (pInst_AMR->remote_key) {
        ucp_rkey_destroy(pInst_AMR->remote_key);
        pInst_AMR->remote_key = NULL;
    }
    if (pInst_AMR->mem_hndl) {
        aci_mem_unmap(pHndl->pACI, pInst_AMR->mem_hndl);
        pInst_AMR->mem_hndl = NULL;
    }
    if (pInst_AMR->pShadow_memory) {
        free((void *) pInst_AMR->pShadow_memory);
        pInst_AMR->pShadow_memory = 0;
    }
    return _arl_remove(&pHndl->local_rgns, inst_idx);
}

#define HNDL_NULL_CHECK(pHndl)                                                 \
    if (!pHndl) {                                                              \
        log_error("Handle NULL");                                              \
        return 0;                                                              \
    }

size_t arm_get_n_remote_regions(arm_hndl *pHndl) {
    HNDL_NULL_CHECK(pHndl);
    return pHndl->remote_rgns.size;
}
size_t arm_get_n_local_regions(arm_hndl *pHndl) {
    HNDL_NULL_CHECK(pHndl);
    return pHndl->local_rgns.size;
}

const amr_hndl *arm_get_remote_regions(arm_hndl *pHndl) {
    HNDL_NULL_CHECK(pHndl);
    return pHndl->remote_rgns.data;
}
const amr_hndl *arm_get_local_regions(arm_hndl *pHndl) {
    HNDL_NULL_CHECK(pHndl);
    return pHndl->local_rgns.data;
}
