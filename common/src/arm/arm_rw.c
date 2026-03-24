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
#include "arm/arm.h"

#warning "Not Implemented"

eARM_error arm_write(arm_hndl *pHndl, const amr_hndl *pAMR,
                     const uint64_t remote_addr, const void *data,
                     size_t size) {
    (void)pHndl;
    (void)pAMR;
    (void)remote_addr;
    (void)data;
    (void)size;
    return eARM_OK;
}

eARM_error arm_read(arm_hndl *pHndl, const amr_hndl *pAMR,
                    const uint64_t remote_addr, void *data, size_t size) {
    (void)pHndl;
    (void)pAMR;
    (void)remote_addr;
    (void)data;
    (void)size;
    return eARM_OK;
}
