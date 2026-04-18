/**
 * @file aul_mem.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AUL_INTERNAL

#include "aul.h"
#include "aul_internal.h"
#include "log.h"

void _arm_free(const amr_hndl *const pAMR) {
    if (pAMR) {
        if (pAMR->pShadow_memory) {
            free((void *) pAMR->pShadow_memory);
        }
    }
}

int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                    const size_t size) {
    // Advance the client side memory tick (memory ops pending)
    int acn_status = 0;
    acn_status = acn_tick(_aul_ctx.pACN, eACN_memory);
    if (acn_status != 0) {
        log_error("ACN Error: %d", acn_status);
        return acn_status;
    }

    amr_hndl amr = {
        .pActive_memory = (uint64_t) ptr,
        .pShadow_memory = (uint64_t) malloc(size),
        .rgn_size = size,
        .id = mem_id,
        .free = _arm_free,
        .name = "",
        .__reserved = {0ULL},
    };
    eARM_error arm_status = eARM_OK;
    arm_status = arm_add(_aul_ctx.pARM, &amr);
    if (arm_status != eARM_OK) {
        log_error("%d", arm_status);
        return -arm_status;
    }
    return 0;
}

int AUL_Mem_unprotect(const uint64_t mem_id) {
    // Advance the client side memory tick (memory ops pending)
    eACN_error acn_status = eACN_OK;
    TIME_REGION("Wait") {
        do {
            acn_status = acn_await(_aul_ctx.pACN, eACN_checkpoint);
        } while (acn_status == eACN_ERR_TIMEOUT);
        if (acn_status != eACN_OK) {
            log_error("ACN Error: %d", acn_status);
            return acn_status;
        }
    }

    acn_status = acn_tick(_aul_ctx.pACN, eACN_memory);
    if (acn_status != eACN_OK) {
        log_error("ACN Error: %d", acn_status);
        return acn_status;
    }

    amr_hndl amr = {
        .pActive_memory = 0ULL,
        .rgn_size = 0ULL,
        .id = mem_id,
        .free = NULL,
        .name = "",
        .__reserved = {0ULL},
    };
    eARM_error arm_status = eARM_OK;
    arm_status = arm_remove(_aul_ctx.pARM, &amr);
    if (arm_status != eARM_OK) {
        return -arm_status;
    }
    return 0;
}
