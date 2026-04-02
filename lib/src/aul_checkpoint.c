/**
 * @file aul_checkpoint.c
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

#include <limits.h>

int AUL_Checkpoint(const int version, const char name[static AUL_NAME_LEN]) {
    TIME_REGION("Checkpoint Total") {
        TIME_REGION("Wait for checkpoint") {
            // Wait for previous checkpoint to complete
            if (acn_await(_aul_ctx.pACN, eACN_checkpoint) != 0) {
                log_fatal("Server disconnected");
                return INT_MIN;
            }
        }

        TIME_REGION("Memory Copy") {
            size_t n_regions = arm_get_n_local_regions(_aul_ctx.pARM);
            const amr_hndl *region_list = arm_get_local_regions(_aul_ctx.pARM);
            if (!region_list) {
                return -2;
            }
            for (size_t i = 0; i < n_regions; i++) {
                log_trace("Chpt Rgn %d, id=%d", i, region_list[i].id);
                const void *pActive = (void *) region_list[i].pActive_memory;
                void *pShadow = (void *) region_list[i].pShadow_memory;
                if (pActive && pShadow) {
                    memcpy(pShadow, pActive, region_list[i].rgn_size);
                } else {
                    log_error("NULL Error");
                }
            }
        }

        // Setup the checkpoint name and version
        acn_set_name(_aul_ctx.pACN, name);
        acn_set(_aul_ctx.pACN, eACN_version, version);
        // Trigger for next checkpoint (client side tick)
        acn_tick(_aul_ctx.pACN, eACN_checkpoint);
    }
    return -1;
}
