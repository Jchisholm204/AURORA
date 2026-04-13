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
    if (!pHndl || !pAMR) {
        log_error("NULL parameter");
        return eARM_ERR_NULL;
    }

    if (!pAMR->pActive_memory || !pAMR->pShadow_memory) {
        log_error("NULL parameter");
        return eARM_ERR_NULL;
    }

    if (!pAMR->free) {
        log_error("NULL parameter");
        return eARM_ERR_NULL;
    }

    amr_hndl *pInst_AMR = _arl_add(&pHndl->local_rgns);

    if (!pInst_AMR) {
        log_error("Failed to create AMR");
        return eARM_ERR_NULL;
    }

    (void) memcpy(pInst_AMR, pAMR, sizeof(amr_hndl));

    void *rkey_buffer = NULL;
    size_t rkey_size = 0;

    { // BEGIN Map the shadow region
        ucp_mem_map_params_t mparam;
        mparam.field_mask =
            UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
        mparam.address = (void *) pInst_AMR->pShadow_memory;
        mparam.length = pInst_AMR->rgn_size;
        ucs_status_t ucs_status = UCS_OK;
        ucs_status =
            aci_mem_map(pHndl->pACI, &mparam, &pInst_AMR->shadow_mem_hndl);
        if (ucs_status != UCS_OK) {
            log_error("UCS Failure");
            (void) arm_remove(pHndl, pInst_AMR);
            return eARM_ERR_UCS;
        }
        // Pack the shadow rkey into the global buffer
        ucs_status = aci_rkey_pack(pHndl->pACI, pInst_AMR->shadow_mem_hndl,
                                   &rkey_buffer, &rkey_size);
        if (ucs_status != UCS_OK) {
            log_error("UCS Failure");
            (void) arm_remove(pHndl, pInst_AMR);
            return eARM_ERR_UCS;
        }
    } // END Map the shadow region

    { // BEGIN Map the active region
        ucp_mem_map_params_t mparam;
        // Same mask as shadow region
        mparam.field_mask =
            UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
        mparam.address = (void *) pInst_AMR->pActive_memory;
        mparam.length = pInst_AMR->rgn_size;
        ucs_status_t ucs_status = UCS_OK;
        ucs_status =
            aci_mem_map(pHndl->pACI, &mparam, &pInst_AMR->active_mem_hndl);
        if (ucs_status != UCS_OK) {
            log_error("UCS Failure");
            (void) arm_remove(pHndl, pInst_AMR);
            return eARM_ERR_UCS;
        }
        void *active_rkey_buf = NULL;
        size_t active_rkey_size = 0;
        ucs_status = aci_rkey_pack(pHndl->pACI, pInst_AMR->active_mem_hndl,
                                   &active_rkey_buf, &active_rkey_size);
        if (ucs_status != UCS_OK) {
            log_error("UCS Failure");
            // Release the rkey buffer
            if (rkey_buffer) {
                ucp_rkey_buffer_release(rkey_buffer);
                rkey_size = 0;
                rkey_buffer = NULL;
            }
            (void) arm_remove(pHndl, pInst_AMR);
            return eARM_ERR_UCS;
        }

        // Pack the rkeys
        uint8_t *global_rkey_buf =
            malloc(active_rkey_size + rkey_size + sizeof(size_t));
        if (!global_rkey_buf) {
            log_error("Bad Alloc");
            (void) arm_remove(pHndl, pInst_AMR);
            return eARM_ERR_NULL;
        }

        *((size_t *) global_rkey_buf) = rkey_size;

        // Copy rkeys into the buffer
        (void) memcpy(global_rkey_buf + sizeof(size_t), rkey_buffer, rkey_size);
        (void) memcpy(global_rkey_buf + sizeof(size_t) + rkey_size,
                      active_rkey_buf, rkey_size);

        // Free UCP buffers
        (void) ucp_rkey_buffer_release(rkey_buffer);
        rkey_buffer = NULL;
        (void) ucp_rkey_buffer_release(active_rkey_buf);
        active_rkey_buf = NULL;

        // Finalize rkey buffer in super context
        rkey_size = active_rkey_size + rkey_size + sizeof(size_t);
        rkey_buffer = global_rkey_buf;

    } // END Map the active region

    ucp_request_param_t rparam = {0};
    ucs_status_t ucs_status = UCS_OK;
    ucs_status_ptr_t pStatus =
        aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_ADD, pInst_AMR,
                        sizeof(amr_hndl), rkey_buffer, rkey_size, &rparam);
    if (UCS_PTR_IS_ERR(pStatus)) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        (void) arm_remove(pHndl, pInst_AMR);
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pStatus)) {
        do {
            ucs_status_t aci_status = aci_poll(pHndl->pACI);
            if (aci_status != UCS_OK) {
                log_error("UCS Error %s", ucs_status_string(aci_status));
                (void) arm_remove(pHndl, pInst_AMR);
                return eARM_ERR_UCS;
            }
            ucs_status = ucp_request_check_status(pStatus);

        } while (ucs_status == UCS_INPROGRESS);
        ucp_request_release(pStatus);
    }

    free(rkey_buffer);

    if (ucs_status != UCS_OK) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        (void) arm_remove(pHndl, pInst_AMR);
        return eARM_ERR_UCS;
    }

    log_debug("Added new remote region. %d %s", pInst_AMR->id, pInst_AMR->name);
    log_debug("0x%lx 0x%lx", pInst_AMR->pActive_memory,
              pInst_AMR->pShadow_memory);
    log_debug("0x%lx 0x%lx", pInst_AMR->active_remote_key,
              pInst_AMR->shadow_remote_key);

    return eARM_OK;
}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if (!pHndl || !pAMR) {
        return eARM_ERR_NULL;
    }

    amr_hndl *pInst_AMR = (amr_hndl *) pAMR;
    size_t inst_idx = 0;
    eARM_error arm_status;
    arm_status = _arm_find(&pHndl->local_rgns, &pInst_AMR, &inst_idx);
    if (arm_status != eARM_OK || !pInst_AMR) {
        log_error("ARM Error %d", arm_status);
        return arm_status;
    }

    log_trace("removing region %d, %d left", pInst_AMR->id,
              pHndl->local_rgns.size - 1);

    ucp_request_param_t rparam = {0};
    rparam.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    rparam.flags = UCP_AM_SEND_FLAG_COPY_HEADER;

    ucs_status_t ucs_status = UCS_OK;
    ucs_status_ptr_t pStatus =
        aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_RM, pInst_AMR, sizeof(amr_hndl),
                        NULL, 0, &rparam);
    if (UCS_PTR_IS_ERR(pStatus)) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
        // Not much we can do about an error here..
        // This function may be used when UCP is already in an error state
    } else if (UCS_PTR_IS_PTR(pStatus)) {
        do {
            (void) aci_poll(pHndl->pACI);
            ucs_status = ucp_request_check_status(pStatus);

        } while (ucs_status == UCS_INPROGRESS);
        ucp_request_release(pStatus);
    }

    if (ucs_status != UCS_OK) {
        log_error("UCS Error %s", ucs_status_string(UCS_PTR_STATUS(pStatus)));
    }

    if (pInst_AMR->active_remote_key) {
        ucp_rkey_destroy(pInst_AMR->active_remote_key);
        pInst_AMR->active_remote_key = NULL;
    }
    if (pInst_AMR->active_mem_hndl) {
        aci_mem_unmap(pHndl->pACI, pInst_AMR->active_mem_hndl);
        pInst_AMR->active_mem_hndl = NULL;
    }
    if (pInst_AMR->shadow_remote_key) {
        ucp_rkey_destroy(pInst_AMR->shadow_remote_key);
        pInst_AMR->shadow_remote_key = NULL;
    }
    if (pInst_AMR->shadow_mem_hndl) {
        aci_mem_unmap(pHndl->pACI, pInst_AMR->shadow_mem_hndl);
        pInst_AMR->shadow_mem_hndl = NULL;
    }

    // Attempt to use the provided free
    if (pAMR->free &&
        (pInst_AMR->pActive_memory || pInst_AMR->pShadow_memory)) {
        // Call the provided free function
        (void) pAMR->free(pInst_AMR);
        // Ensure memory is zeroed out
        *((uint64_t *) (&pInst_AMR->pActive_memory)) = 0;
        *((uint64_t *) (&pInst_AMR->pShadow_memory)) = 0;
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
