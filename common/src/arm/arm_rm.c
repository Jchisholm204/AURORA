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
#include "arm/arm_am.h"
#include "arm/arm.h"

eARM_error arm_add(arm_hndl *pHndl, const amr_hndl *pAMR) {
    if(!pHndl || pAMR){
        return eARM_ERR_NULL;
    }

    _arl_add(&pHndl->local_rgns);


    aci_am_send_nbx(pHndl->pACI, ARM_UCX_ID_ADD, NULL, 0, NULL, 0, NULL);


}

eARM_error arm_remove(arm_hndl *pHndl, const amr_hndl *pAMR) {
}

size_t arm_get_n_regions(arm_hndl *pHndl) {
}

const amr_hndl *arm_get_regions(arm_hndl *pHndl) {
}
