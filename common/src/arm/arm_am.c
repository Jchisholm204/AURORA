/**
 * @file arm_am.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager - Active Messages Layer
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL

#include "arm/arm_am.h"

#include "aci/aci_internal.h"
#include "acn/acn.h"
#include "arm/arm.h"
#include "log.h"

#include <string.h>

#define SET_CONST(var, type) (*((type *) &var))

ucs_status_t _arm_add_rgn_cb(void *arg, const void *header, size_t header_len,
                             void *data, size_t data_len,
                             const ucp_am_recv_param_t *pParam) {
    arm_hndl *pHndl = (arm_hndl *) arg;
    amr_hndl *pAMR = (amr_hndl *) header;
    // Unused, SHould be NULL
    (void) pParam;

    acn_tick(pHndl->pACN, eACN_memory);

    if (!data) {
        log_error("UCS Error");
        // Return OK to avoid killing UCS
        return UCS_OK;
    }

    if (header_len != sizeof(amr_hndl)) {
        log_error("UCS Error");
        // Return OK to avoid killing UCS
        return UCS_OK;
    }
    if (!pHndl || !pAMR) {
        log_error("UCS Error");
        // Return OK to avoid killing UCS
        return UCS_OK;
    }

    // Grab the local handle
    amr_hndl *pInst_AMR = _arl_add(&pHndl->remote_rgns);

    if (!pInst_AMR) {
        log_error("Failed to create AMR");
        return UCS_OK;
    }

    // Copy RX data into the local handle
    (void) memcpy(pInst_AMR, pAMR, sizeof(amr_hndl));

    // Zero out all non-local fields (even const members)
    *((aurora_memory_free_cb_t *) &pInst_AMR->free) = NULL;
    SET_CONST(pInst_AMR->free, aurora_memory_free_cb_t) = NULL;
    pInst_AMR->remote_key = NULL;
    pInst_AMR->mem_hndl = NULL;

    { // Unpack Rkey/Setup Region
        ucs_status_t ucs_status =
            aci_rkey_unpack(pHndl->pACI, data, &pInst_AMR->remote_key);

        if (ucs_status != UCS_OK || !pInst_AMR->remote_key) {
            log_error("UCS Error");
            (void) _arl_remove(&pHndl->remote_rgns, pHndl->remote_rgns.size);
            // Return OK to avoid killing UCS
            return UCS_OK;
        }
    }

    log_trace("Add: %d %s 0x%lx", pInst_AMR->id, pInst_AMR->name,
              pInst_AMR->pRegion);

    return UCS_OK;
}

ucs_status_t _arm_rm_rgn_cb(void *arg, const void *header, size_t header_len,
                            void *data, size_t data_len,
                            const ucp_am_recv_param_t *pParam) {
    arm_hndl *pHndl = (arm_hndl *) arg;
    amr_hndl *pRemote_AMR = (amr_hndl *) header;
    // Unused, Should be NULL
    (void) data;
    (void) data_len;
    (void) pParam;

    acn_tick(pHndl->pACN, eACN_memory);

    if (header_len != sizeof(amr_hndl)) {
        log_error("UCS Error");
        // Return OK to avoid killing UCS
        return UCS_OK;
    }
    if (!pHndl || !pRemote_AMR) {
        log_error("UCS Error");
        // Return OK to avoid killing UCS
        return UCS_OK;
    }

    amr_hndl *pInst_AMR = (amr_hndl *) pRemote_AMR;
    size_t inst_idx = 0;
    eARM_error arm_status = eARM_OK;

    arm_status = _arm_find(&pHndl->remote_rgns, &pInst_AMR, &inst_idx);
    if (arm_status != eARM_OK || !pInst_AMR) {
        log_error("ARM Error");
        return UCS_OK;
    }

    log_trace("Remove: %d %s 0x%lx", pInst_AMR->id, pInst_AMR->name,
              pInst_AMR->pRegion);

    if (pInst_AMR->remote_key) {
        log_trace("destroy rkey 0x%lx", pInst_AMR->remote_key);
        ucp_rkey_destroy(pInst_AMR->remote_key);
        pInst_AMR->remote_key = NULL;
    }

    (void) _arl_remove(&pHndl->remote_rgns, inst_idx);

    return UCS_OK;
}
