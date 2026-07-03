/**
 * @file arm_rw.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager - R/W operations
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL
#include "aci/aci_internal.h"
#include "arm/arm.h"
#include "log.h"

#include <assert.h>
#include <unistd.h>

ucp_rkey_h _arm_get_rkey(const amr_hndl *pAMR, uint64_t addr, size_t size) {
    if (!pAMR) {
        return NULL;
    }
    if (!pAMR->shadow_remote_key) {
        log_fatal("NULL Parameter");
        return NULL;
    }
    if (!pAMR->active_remote_key) {
        log_fatal("NULL Parameter");
        return NULL;
    }
    uint64_t shadow_min_addr = pAMR->pShadow_memory;
    uint64_t shadow_max_addr = pAMR->pShadow_memory + pAMR->rgn_size;
    uint64_t active_min_addr = pAMR->pActive_memory;
    uint64_t active_max_addr = pAMR->pActive_memory + pAMR->rgn_size;

    // Wrap Check
    if (addr + size < addr) {
        log_error("Overflow");
        return NULL;
    }

    if ((addr + size) <= shadow_max_addr && addr >= shadow_min_addr) {
        return pAMR->shadow_remote_key;
    }
    if ((addr + size) <= active_max_addr && addr >= active_min_addr) {
        return pAMR->active_remote_key;
    }
    log_error("OOB Access");
    return NULL;
}

eARM_error arm_write(arm_hndl *pHndl, const amr_hndl *pAMR,
                     const uint64_t remote_addr, const void *data,
                     size_t size) {
    arm_op operation = {0};
    eARM_error arm_status =
        arm_write_async(pHndl, &operation, pAMR, remote_addr, data, size);
    if (arm_status != eARM_OK) {
        return arm_status;
    }
    return arm_async_check(pHndl, &operation, true);
}

eARM_error arm_write_async(arm_hndl *pHndl, arm_op *pOperation,
                           const amr_hndl *pAMR, const uint64_t remote_addr,
                           const void *data, size_t size) {
    if (!pHndl || !pAMR || !data || !pOperation) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    if (pOperation->ucs_pStatus) {
        return eARM_ERR_INPROGRESS;
    }
    ucp_rkey_h ucp_remote_key = _arm_get_rkey(pAMR, remote_addr, size);
    if (!ucp_remote_key) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }

    ucp_request_param_t ucp_rparams = {0};
    pOperation->ucs_pStatus = aci_put(pHndl->pACI, data, size, remote_addr,
                                      ucp_remote_key, &ucp_rparams);
    if (UCS_PTR_IS_ERR(pOperation->ucs_pStatus)) {
        log_error("Remote Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(pOperation->ucs_pStatus)));
        pOperation->ucs_pStatus = NULL;
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pOperation->ucs_pStatus)) {
        int aci_status = 0;
        aci_status = aci_poll(pHndl->pACI);
        if (aci_status != UCS_OK) {
            return eARM_ERR_FATAL;
        }
    }
    return eARM_OK;
}

eARM_error arm_read(arm_hndl *pHndl, const amr_hndl *pAMR,
                    const uint64_t remote_addr, void *data, size_t size) {
    arm_op operation = {0};
    eARM_error arm_status =
        arm_read_async(pHndl, &operation, pAMR, remote_addr, data, size);
    if (arm_status != eARM_OK) {
        return arm_status;
    }
    return arm_async_check(pHndl, &operation, true);
}

eARM_error arm_read_async(arm_hndl *pHndl, arm_op *pOperation,
                          const amr_hndl *pAMR, const uint64_t remote_addr,
                          void *data, size_t size) {
    if (!pHndl || !pAMR || !data) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    if (!pAMR->shadow_remote_key) {
        log_fatal("NULL Parameter");
        return eARM_ERR_FATAL;
    }
    if (!pAMR->active_remote_key) {
        log_fatal("NULL Parameter");
        return eARM_ERR_FATAL;
    }
    if (!pOperation) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    if (pOperation->ucs_pStatus) {
        return eARM_ERR_INPROGRESS;
    }
    pOperation->status = eARM_OK;
    pOperation->ucs_pStatus = NULL;

    ucp_rkey_h ucp_remote_key = _arm_get_rkey(pAMR, remote_addr, size);
    if (!ucp_remote_key) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }

    ucp_request_param_t ucp_rparams = {0};
    pOperation->ucs_pStatus = aci_get(pHndl->pACI, data, size, remote_addr,
                                      ucp_remote_key, &ucp_rparams);
    if (UCS_PTR_IS_ERR(pOperation->ucs_pStatus)) {
        log_error("Remote Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(pOperation->ucs_pStatus)));
        pOperation->ucs_pStatus = NULL;
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pOperation->ucs_pStatus)) {
        int aci_status = 0;
        aci_status = aci_poll(pHndl->pACI);
        if (aci_status != UCS_OK) {
            return eARM_ERR_FATAL;
        }
    }

    return eARM_OK;
}

eARM_error arm_async_check(arm_hndl *pHndl, arm_op *pOperation, bool wait) {
    if (!pOperation) {
        return eARM_ERR_NULL;
    }
    if (pOperation->status != eARM_OK) {
        return pOperation->status;
    }

    ucs_status_t ucs_status = UCS_OK;
    if (UCS_PTR_IS_ERR(pOperation->ucs_pStatus)) {
        log_error("Remote Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(pOperation->ucs_pStatus)));
        pOperation->ucs_pStatus = NULL;
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pOperation->ucs_pStatus)) {
        do {
            ucs_status = ucp_request_check_status(pOperation->ucs_pStatus);
            int aci_status = 0;
            aci_status = aci_poll(pHndl->pACI);
            if (aci_status != 0) {
                return eARM_ERR_FATAL;
            }
        } while (ucs_status == UCS_INPROGRESS && wait);
        if (ucs_status != UCS_INPROGRESS) {
            ucp_request_free(pOperation->ucs_pStatus);
            pOperation->ucs_pStatus = NULL;
            pOperation->status = eARM_OK;
        }
    }

    if (ucs_status != UCS_OK) {
        log_error("Failed remote read: %s", ucs_status_string(ucs_status));
        return eARM_ERR_UCS;
    }
    return eARM_OK;
}
