/**
 * @file acr_cmd_restart.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Restart Command for AURORA Command Runner
 * @version 0.1
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#include "acr/acr.h"
#include "aim.h"
#include "log.h"

#include <memory.h>

void *acr_cmd_restart(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;
    aim_entry_t *pInstance = pCtx->pInstance;

    // Do the command stuff

    int64_t restore_version = 0;
    acn_get(pInstance->pACN, eACN_version, (uint64_t *) &restore_version);

    char restore_name[ACN_NAME_LEN] = "NULL";
    acn_get_name(pInstance->pACN, restore_name);

    log_debug("Restore Triggered -> %d: %.*s (%d)", restore_version,
              ACN_NAME_LEN, restore_name, 0);

    size_t inst_n_rgns = arm_get_n_remote_regions(pInstance->pARM);
    log_trace("Remote has %lu regions", inst_n_rgns);

    const amr_hndl *AMR_list = arm_get_remote_regions(pInstance->pARM);

    uint64_t test_data[4] = {0x0000, 0x1111, 0x2222, 0x3333};

    for (size_t i = 0; i < inst_n_rgns; i++) {
        const amr_hndl *pAMR = &AMR_list[i];
        log_trace("RGN: %d: %s", pAMR->id, pAMR->name);
        eARM_error arm_status =
            arm_write(pInstance->pARM, pAMR, pAMR->pActive_memory, test_data,
                      pAMR->rgn_size);
        if (arm_status != eARM_OK) {
            log_error("ARM Err %d", arm_status);
        }
    }

    acn_set(pInstance->pACN, eACN_version, 7);

    log_trace("Set Version");

    acn_tick(pInstance->pACN, eACN_restore);

    log_trace("Ticked Restore");

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    log_debug("Restore Finished -> %d: %.*s (%d)", restore_version,
              ACN_NAME_LEN, restore_name, 0);
    int release = 0;
    int attempts = 0;
    do {
        attempts++;
        release = _acr_ctx_release(pCtx);
    } while (release != 0 && attempts < 10);
    if (release != 0) {
        log_warn("Failed to release");
    }
    else{
        log_trace("Released");
    }
    return NULL;
}
