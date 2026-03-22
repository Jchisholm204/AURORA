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

    (void) memcpy(pInt_AMR, pAMR, sizeof(amr_hndl));

    pInt_AMR->pShadow_memory = (uint64_t) shadow_memory;

    ucp_mem_map_params_t mparam;
    mparam.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mparam.address = (void *) pInt_AMR->pActive_memory;
    mparam.length = pInt_AMR->rgn_size;
    ucs_status_t ucs_status = UCS_OK;
    ucs_status = aci_mem_map(pHndl->pACI, &mparam, &pInt_AMR->mem_hndl);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        _arl_remove(&pHndl->local_rgns, pHndl->local_rgns.size);
    }
    mparam.address = (void *) pInt_AMR->pActive_memory;
    mparam.length = pInt_AMR->rgn_size;
    ucs_status = aci_mem_map(pHndl->pACI, &mparam, &pInt_AMR->mem_hndl);
    if (ucs_status != UCS_OK) {
        log_error("UCS Failure");
        _arl_remove(&pHndl->local_rgns, pHndl->local_rgns.size);
    }

    aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_ADD, NULL, 0, NULL, 0, NULL);
}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
}

size_t arm_get_n_regions(arm_hndl *pHndl) {
}

const amr_hndl *arm_get_regions(arm_hndl *pHndl) {
}
