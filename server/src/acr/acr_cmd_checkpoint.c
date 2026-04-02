/**
 * @file acr_cmd_checkpoint
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Checkpoint Command for AURORA Command Runner
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

void *acr_cmd_checkpoint(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;
    aim_entry_t *pInstance = pCtx->pInstance;

    int acn_status = 0;
    acn_status = acn_await(pInstance->pACN, eACN_memory);
    if (acn_status != 0) {
        log_error("ACN Error");
    }

    // Setup the checkpoint
    char name[ACN_NAME_LEN];
    int64_t version;
    acn_get(pInstance->pACN, eACN_version, (uint64_t *) &version);
    acn_get_name(pCtx->pInstance->pACN, name);
    size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);
    const amr_hndl *arm_regions = arm_get_remote_regions(pInstance->pARM);
    const afv_metadata_t *pMetadata_old = afv_get_metadata(pInstance->pAFV);

    // Setup Metadata
    afv_metadata_t *pMetadata =
        afv_metadata_ptr_init(malloc(afv_metadata_size(arm_n_regions)));
    if (!pMetadata) {
        log_error("Bad Alloc?");
        if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
            log_fatal("Could Not Enqueue! A thread lost a connection");
        }
        return NULL;
    }

    // log_info("Rank %d checkpointing %d rngs to %s", pMetadata_old->rank,
    //          arm_n_regions, afv_get_filename(pInstance->pAFV, version, name));

    // Complete the checkpoint
    for (size_t i = 0; i < arm_n_regions; i++) {
        log_trace("rgn: %d -> rgnid: %d", i, arm_regions[i].id);
        pMetadata->region_ids[i] = arm_regions[i].id;
        const amr_hndl *pAMR = &arm_regions[i];
        uint64_t test_data[4] = {0x0000, 0x1111, 0x2222, 0x3333};
        eARM_error arm_status =
            arm_read(pInstance->pARM, pAMR, pAMR->pShadow_memory, test_data,
                     pAMR->rgn_size);
        if (arm_status != eARM_OK) {
            log_error("ARM Err %d", arm_status);
        }
        for (int i = 0; i < 4; i++)
            log_trace("TD %d 0x%lx", i, test_data[i]);
    }

    // Finalize the Checkpoint
    pMetadata->rank = pMetadata_old->rank;
    pMetadata->version = version;
    memcpy(pMetadata->chkpt_name, name, ACN_NAME_LEN);

    log_debug("checkpoint: %d %.*s", version, ACN_NAME_LEN, name);

    // NOP
    acn_tick(pInstance->pACN, eACN_checkpoint);

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;

    // Release the thread context
    (void) _acr_ctx_release_retry(pCtx, 2);

    return NULL;
}
