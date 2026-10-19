/**
 * @file acr_cmd_checkpoint
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Checkpoint Command for AURORA Command Runner
 * @version 0.3
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-07-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#define AFV_NO_COMPAT
#include "acr/acr.h"
#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "afv/afv_metadata.h"
#include "aim.h"
#include "log.h"

#include <memory.h>
#include <unistd.h>

// Leave modules seperate while ensuring nothing faults
#if ARM_NAME_LEN != AFV_RGN_NAME_LEN
#error "ARM Name Length Must Match AFV RNG Name Length"
#endif

afv_checkpoint_t *_acr_cmd_checkpoint_init(aim_entry_t *pInstance);
afv_metadata_t *_acr_cmd_checkpoint_gen_metadata(aim_entry_t *pInstance);

void *acr_cmd_checkpoint(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // Initialize Checkpoint
    aim_entry_t *pInstance = pCtx->pInstance;
    afv_checkpoint_t *pCheckpoint = _acr_cmd_checkpoint_init(pInstance);
    if (!pCheckpoint) {
        log_error("Checkpoint Creation Failed");
        goto CHECKPOINT_FAIL;
    }

    // Read Only Pointer (do not free)
    const amr_hndl *const arm_regions = arm_get_remote_regions(pInstance->pARM);
    const size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

    // Setup Copy Regions
    const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
    uint8_t *pRgn_A = pCtx->pScratch;
    uint8_t *pRgn_B = pCtx->pScratch + cpy_rgn_size;

    // Complete the checkpoint
    for (size_t i = 0; i < arm_n_regions; i++) { // BEGIN Region Loop

        const amr_hndl *pAMR = &arm_regions[i];

        // Setup Region Metadata
        eAFV_verif region_status =
            afv_checkpoint_add(pCheckpoint, pAMR->id, pAMR->rgn_size,
                               pAMR->name);
        if (region_status != eAFV_VERIF_OK) {
            log_error("Checkpoint Region %d Failed 0x%x", i, region_status);
            goto CHECKPOINT_FAIL;
        }

        log_trace("rgn: %d -> rgnid: %d (%d)", i, pAMR->id, pAMR->rgn_size);

        size_t rgn_write_remaining = pAMR->rgn_size;
        size_t rgn_read_remaining = pAMR->rgn_size;

        // Active ARM operation struct
        arm_op arm_operation = {0};

        // START Region Read Loop
        { // Queue RDMA Read for A=region[x]
            const size_t read_cpy_size = rgn_read_remaining >= cpy_rgn_size
                                             ? cpy_rgn_size
                                             : rgn_read_remaining;
            eARM_error arm_status =
                arm_read_async(pInstance->pARM, &arm_operation, pAMR,
                               pAMR->pShadow_memory, pRgn_A, read_cpy_size);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto CHECKPOINT_FAIL;
            }
            rgn_read_remaining -= read_cpy_size;
        }

        log_debug("Region Read Remaining %d", rgn_read_remaining);
        while (rgn_read_remaining > 0) { // BEGIN Block Copies

            // Wait for previous RDMA read to finish
            eARM_error arm_status =
                arm_async_check(pInstance->pARM, &arm_operation, true);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto CHECKPOINT_FAIL;
            }

            // Swap: A=region[x+1], B=A
            {
                void *const C = pRgn_A;
                pRgn_A = pRgn_B;
                pRgn_B = C;
            }

            // Send next RDMA Request A=region[x+1]
            const size_t read_cpy_size = rgn_read_remaining >= cpy_rgn_size
                                             ? cpy_rgn_size
                                             : rgn_read_remaining;
            // Read from base + (size - remaining) = (base + read)
            arm_status = arm_read_async(pInstance->pARM, &arm_operation, pAMR,
                                        pAMR->pShadow_memory + pAMR->rgn_size -
                                            rgn_read_remaining,
                                        pRgn_A, read_cpy_size);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto CHECKPOINT_FAIL;
            }
            rgn_read_remaining -= read_cpy_size;

            // Write recvd RDMA request B=region[x]
            size_t written_bytes = afv_checkpoint_write(pCheckpoint, pAMR->id,
                                                        pRgn_B, cpy_rgn_size);
            if (written_bytes != cpy_rgn_size) {
                log_error("AFV Error");
                goto CHECKPOINT_FAIL;
            }
            rgn_write_remaining -= cpy_rgn_size;
        } // END Block Copies

        // START Final Block Write

        { // Wait for previous RDMA read to finish
            // Wait for previous RDMA read to finish
            eARM_error arm_status =
                arm_async_check(pInstance->pARM, &arm_operation, true);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto CHECKPOINT_FAIL;
            }
        }
        { // Finish final write
            log_debug("Writing %ld", rgn_write_remaining);
            size_t written_bytes =
                afv_checkpoint_write(pCheckpoint, pAMR->id, pRgn_A,
                                     rgn_write_remaining);
            if (written_bytes != rgn_write_remaining) {
                log_error("AFV Error");
                goto CHECKPOINT_FAIL;
            }
        }

    } // END Region Loop

    eAFV_verif checkpoint_verification = afv_checkpoint_verify(pCheckpoint);
    if (checkpoint_verification != eAFV_VERIF_OK) {
        log_error("Checkpoint Verification Failed 0x%x",
                  checkpoint_verification);
        goto CHECKPOINT_FAIL;
    }
    checkpoint_verification = afv_checkpoint_close(&pCheckpoint);
    if (checkpoint_verification != eAFV_VERIF_OK) {
        log_error("Checkpoint Verification Failed 0x%x",
                  checkpoint_verification);
        goto CHECKPOINT_FAIL;
    }

    // NOP
    acn_tick(pInstance->pACN, eACN_checkpoint);

CHECKPOINT_FAIL:
    if (pCheckpoint) {
        log_error("Checkpoint Failed");
        afv_checkpoint_destroy(&pCheckpoint);
    }
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

// Loads metadata from the ACN to prep the checkpoint
afv_checkpoint_t *_acr_cmd_checkpoint_init(aim_entry_t *pInstance) {
    if (!pInstance) {
        log_warn("NULL Parameter");
        return NULL;
    }

    // Wait for memory operations to complete
    eACN_error acn_status = eACN_OK;
    do {
        acn_status = acn_await(pInstance->pACN, eACN_memory);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);

    if (acn_status != eACN_OK) {
        log_error("ACN Memory Error 0x%x", acn_status);
        return NULL;
    }

    // Get the number of operations
    size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

    int64_t checkpoint_version = -1;
    char checkpoint_name[AFV_CKPT_NAME_LEN] = "NULL\0";

    do {
        acn_status = acn_get(pInstance->pACN, eACN_version,
                             (uint64_t *) &checkpoint_version);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);

    if (acn_status != eACN_OK) {
        log_error("ACN Error 0x%lx", acn_status);
        return NULL;
    }

    do {
        acn_status = acn_get_name(pInstance->pACN, checkpoint_name);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        log_error("ACN Error 0x%lx", acn_status);
        return NULL;
    }

    afv_checkpoint_t *pCheckpoint =
        afv_checkpoint_create(pInstance->pAFV, checkpoint_version,
                              checkpoint_name, arm_n_regions);

    if (!pCheckpoint) {
        log_error("Checkpoint Creation Failed");
        return NULL;
    }

    return pCheckpoint;
}
