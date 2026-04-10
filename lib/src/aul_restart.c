/**
 * @file aul_restart.c
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
#include "limits.h"
#include "log.h"

int AUL_Test(const int version, const char name[static AUL_NAME_LEN]) {
}

int AUL_Restart(const int version, const char name[static AUL_NAME_LEN]) {
    // Wait for all pending operations to finish
    if (acn_await(_aul_ctx.pACN,
                  eACN_systick | eACN_checkpoint | eACN_restore) != 0) {
        // Connection Failure
        log_fatal("Server disconnected");
        return INT_MIN;
    }

    // Setup the version and name we want
    acn_set_name(_aul_ctx.pACN, name);
    acn_set(_aul_ctx.pACN, eACN_version, ((int64_t) version));
    acn_tick(_aul_ctx.pACN, eACN_restore);

    // Wait for restore

    if (acn_await(_aul_ctx.pACN, eACN_restore) != 0) {
        // Connection Failure
        log_fatal("Server disconnected");
        return INT_MIN;
    }

    // Check the restored version
    int64_t vplaced = 0;
    if (acn_get(_aul_ctx.pACN, eACN_version, (uint64_t *) &vplaced) != 0) {
        // Server failure
        log_fatal("Server disconnected");
        return INT_MIN;
    }
    return vplaced;
}
