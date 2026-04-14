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
        return NULL;
    }

    if ((addr + size) <= shadow_max_addr && addr >= shadow_min_addr) {
        return pAMR->shadow_remote_key;
    }
    if ((addr + size) <= active_max_addr && addr >= active_min_addr) {
        return pAMR->active_remote_key;
    }
    return NULL;
}

eARM_error arm_write(arm_hndl *pHndl, const amr_hndl *pAMR,
                     const uint64_t remote_addr, const void *data,
                     size_t size) {
    if (!pHndl || !pAMR || !data) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    ucp_rkey_h ucp_remote_key = _arm_get_rkey(pAMR, remote_addr, size);
    if (!ucp_remote_key) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    ucs_status_t ucs_status = UCS_OK;
    ucp_request_param_t ucp_rparams = {0};
    ucs_status_ptr_t ucs_pStatus = aci_put(pHndl->pACI, data, size, remote_addr,
                                           ucp_remote_key, &ucp_rparams);
    if (UCS_PTR_IS_ERR(ucs_pStatus)) {
        log_error("Remote Read Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(ucs_pStatus)));
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(ucs_pStatus)) {
        do {
            int aci_status = 0;
            aci_status = aci_wait(pHndl->pACI);
            if (aci_status != UCS_OK && aci_status != UCS_INPROGRESS) {
                log_fatal("UCS_ERROR: %d", aci_status);
                ucp_request_free(ucs_pStatus);
                return eARM_ERR_UCS;
            }
            ucs_status = ucp_request_check_status(ucs_pStatus);
        } while (ucs_status == UCS_INPROGRESS);
    }

    if (ucs_pStatus) {
        ucp_request_free(ucs_pStatus);
        ucs_pStatus = NULL;
    }

    if (ucs_status != UCS_OK) {
        log_error("Failed remote read: %s", ucs_status_string(ucs_status));
        return eARM_ERR_UCS;
    }
    usleep(100);
    return eARM_OK;
}

eARM_error arm_read(arm_hndl *pHndl, const amr_hndl *pAMR,
                    const uint64_t remote_addr, void *data, size_t size) {
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
    ucp_rkey_h ucp_remote_key = _arm_get_rkey(pAMR, remote_addr, size);
    if (!ucp_remote_key) {
        log_error("NULL Parameter");
        return eARM_ERR_NULL;
    }
    ucs_status_t ucs_status = UCS_OK;
    ucp_request_param_t ucp_rparams = {0};
    ucs_status_ptr_t ucs_pStatus = aci_get(pHndl->pACI, data, size, remote_addr,
                                           ucp_remote_key, &ucp_rparams);
    if (UCS_PTR_IS_ERR(ucs_pStatus)) {
        log_error("Remote Read Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(ucs_pStatus)));
        return eARM_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(ucs_pStatus)) {
        do {
            ucs_status = ucp_request_check_status(ucs_pStatus);
            int aci_status = 0;
            aci_status = aci_poll(pHndl->pACI);
            if (aci_status != 0) {
                return eARM_ERR_FATAL;
            }
        } while (ucs_status == UCS_INPROGRESS);
        ucp_request_free(ucs_pStatus);
    }

    if (ucs_status != UCS_OK) {
        log_error("Failed remote read: %s", ucs_status_string(ucs_status));
        return eARM_ERR_UCS;
    }
    return eARM_OK;
}
