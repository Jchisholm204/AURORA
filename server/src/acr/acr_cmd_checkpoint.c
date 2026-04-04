/**
 * @file acr_cmd_checkpoint
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Checkpoint Command for AURORA Command Runner
 * @version 0.2
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#include "acr/acr.h"
#include "afv/afv.h"
#include "afv/afv_file.h"
#include "afv/afv_metadata.h"
#include "aim.h"
#include "log.h"

#include <memory.h>

// Leave modules seperate while ensuring nothing faults
#if ARM_NAME_LEN != AFV_RGN_NAME_LEN
#error "ARM Name Length Must Match AFV RNG Name Length"
#endif

void *acr_cmd_checkpoint(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;
    aim_entry_t *pInstance = pCtx->pInstance;

    { // BEGIN Wait for outstanding memory operations to complete
        int acn_status = 0;
        acn_status = acn_await(pInstance->pACN, eACN_memory);
        if (acn_status != 0) {
            log_error("ACN Error");
            goto CHECKPOINT_FAIL;
        }
    } // END Wait for outstanding memory operations to complete

    afv_metadata_t *pMetadata = NULL;

    { // BEGIN setup metadata
        size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

        pMetadata = afv_create_metadata(arm_n_regions);

        if (!pMetadata) {
            log_error("Bad Alloc??");
            goto CHECKPOINT_FAIL;
        }

        eACN_error acn_status = eACN_OK;
        acn_status = acn_get(pInstance->pACN, eACN_version,
                             (uint64_t *) &pMetadata->version);
        if (acn_status != eACN_OK) {
            afv_destroy_metadata(&pMetadata);
            log_error("ACN Error 0x%lx", acn_status);
            goto CHECKPOINT_FAIL;
        }
        acn_get_name(pCtx->pInstance->pACN, pMetadata->chkpt_name);
        if (acn_status != eACN_OK) {
            afv_destroy_metadata(&pMetadata);
            log_error("ACN Error 0x%lx", acn_status);
            goto CHECKPOINT_FAIL;
        }
        pMetadata->n_regions = arm_n_regions;
        pMetadata->rank = afv_get_rank(pInstance->pAFV);

    } // END setup metadata

    { // BEGIN Checkpoint
        // Setup checkpoint file
        afv_file_hndl *pCkpt_file =
            afv_file_open_w(pInstance->pAFV, pMetadata->version,
                            pMetadata->chkpt_name);

        if (!pCkpt_file) {
            afv_destroy_metadata(&pMetadata);
            log_error("Bad Alloc??");
            goto CHECKPOINT_FAIL;
        }

        // Read Only Pointer
        // Do not Free
        const amr_hndl *const arm_regions =
            arm_get_remote_regions(pInstance->pARM);
        if (!arm_regions) {
            log_error("ARM Error");
            goto CHECKPOINT_FAIL;
        }

        // Complete the checkpoint
        for (size_t i = 0; i < pMetadata->n_regions; i++) {
            const amr_hndl *pAMR = &arm_regions[i];

            // Setup Region Metadata
            pMetadata->region_ids[i] = pAMR->id;
            pMetadata->region_sizes[i] = pAMR->rgn_size;
            memcpy(pMetadata->region_names[i], pAMR->name, ARM_NAME_LEN);

            log_debug("rgn: %d -> rgnid: %d (%d)", i, pAMR->id, pAMR->rgn_size);

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

        afv_file_close(&pCkpt_file);

    } // END Checkpoint

    // log_debug("checkpoint: %d %.*s", version, ACN_NAME_LEN, name);

    afv_write_metadata(pInstance->pAFV, pMetadata);

    // NOP
    acn_tick(pInstance->pACN, eACN_checkpoint);

CHECKPOINT_FAIL:

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
