/**
 * @file acr_cmd_restart.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Restart Command for AURORA Command Runner
 * @version 0.2
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-07-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#define AFV_METADATA_INC_MATCH
#include "acr/acr.h"
#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "aim.h"
#include "log.h"

#include <assert.h>
#include <memory.h>
#include <unistd.h>

// Leave modules seperate while ensuring nothing faults
#if ARM_NAME_LEN != AFV_RGN_NAME_LEN
#error "ARM Name Length Must Match AFV RNG Name Length"
#endif

#if ACN_NAME_LEN != AFV_CKPT_NAME_LEN
#error "ACN Name Length Must Match AFV Checkpoint Name Length"
#endif

#define ASSERT_INSTANCE(pInstance) assert(pInstance != NULL);

afv_checkpoint_t *_acr_cmd_restart_init(aim_entry_t *pInstance);

void *acr_cmd_restart(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // All variables in RESTART_FAIL must be declared here
    aim_entry_t *pInstance = pCtx->pInstance;
    ASSERT_INSTANCE(pInstance);
    afv_checkpoint_t *pCheckpoint = NULL;
    size_t *amr_restore_order = NULL;
    bool restart_fail = true;

    // Load the checkpoint from disk
    pCheckpoint = _acr_cmd_restart_init(pInstance);
    if (!pCheckpoint) {
        log_error("Checkpoint Load Failure");
        goto RESTART_FAIL;
    }

    const size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

    // Read only pointer, do not free
    amr_hndl const *const arm_region_list =
        arm_get_remote_regions(pInstance->pARM);

    if (arm_n_regions == 0 || arm_region_list == NULL) {
        log_fatal("Attempted Restore with no active regions");
        goto RESTART_FAIL;
    }

    const ssize_t checkpoint_n_regions = afv_checkpoint_n_regions(pCheckpoint);
    if (checkpoint_n_regions < 0) {
        log_error("Failed to get checkpoint region list 0x%x",
                  -checkpoint_n_regions);
        goto RESTART_FAIL;
    }

    if ((size_t) checkpoint_n_regions != arm_n_regions) {
        log_warn("checkpoint_regions (%ld) != (%ld) active memory regions",
                 checkpoint_n_regions, arm_n_regions);
        goto RESTART_FAIL;
    }

    amr_restore_order = calloc(arm_n_regions, sizeof(size_t));
    if (!amr_restore_order) {
        log_error("Bad Alloc?");
        goto RESTART_FAIL;
    }
    for (size_t i = 0; i < arm_n_regions; i++) {
        amr_hndl const *const pAMR = &arm_region_list[i];
        size_t index = 0;
        eAFV_verif status =
            afv_checkpoint_find_index(pCheckpoint, pAMR->id, pAMR->rgn_size,
                                      pAMR->name, &index);
        if (status != eAFV_VERIF_OK) {
            log_error("No Such Region: id=%d status=0x%x", pAMR->id, status);
            goto RESTART_FAIL;
        } else {
            log_debug("Region: id=%ld size=%ld order=%d", pAMR->id,
                      pAMR->rgn_size, index);
        }
        amr_restore_order[index] = i;
    }

    // Setup Copy Regions
    const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
    uint8_t *pRgn_A = pCtx->pScratch;
    uint8_t *pRgn_B = pCtx->pScratch + cpy_rgn_size;

    // Complete the Restore
    for (size_t i = 0; i < (size_t) checkpoint_n_regions;
         i++) { // BEGIN Region Loop
        // Index into the ARM through the hash map
        const size_t amr_index = amr_restore_order[i];
        const amr_hndl *const pAMR = &arm_region_list[amr_index];

        size_t rgn_read_remaining = pAMR->rgn_size;
        size_t rgn_write_remaining = pAMR->rgn_size;

        arm_op arm_operation = {0};
        { // File Read for A=region[x]
            const size_t read_cpy_size = rgn_read_remaining >= cpy_rgn_size
                                             ? cpy_rgn_size
                                             : rgn_read_remaining;
            const ssize_t bytes_read =
                afv_checkpoint_read(pCheckpoint, pAMR->id,
                                    pAMR->rgn_size - rgn_read_remaining, pRgn_A,
                                    read_cpy_size);
            if (bytes_read < 0) {
                log_error("Checkpoint Read Error: %d", -bytes_read);
                goto RESTART_FAIL;
            }
            rgn_read_remaining -= bytes_read;
        }

        while (rgn_read_remaining > 0) { // BEGIN Block Copies

            // Queue RDMA Write for A=region[x]
            eARM_error arm_status =
                arm_write_async(pInstance->pARM, &arm_operation, pAMR,
                                pAMR->pActive_memory + pAMR->rgn_size -
                                    rgn_write_remaining,
                                pRgn_A, cpy_rgn_size);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto RESTART_FAIL;
            }

            // Swap: A=region[x+1], B=A=region[x]
            {
                void *const C = pRgn_A;
                pRgn_A = pRgn_B;
                pRgn_B = C;
            }

            // Read next block from file
            const size_t read_cpy_size = rgn_read_remaining >= cpy_rgn_size
                                             ? cpy_rgn_size
                                             : rgn_read_remaining;
            const ssize_t bytes_read =
                afv_checkpoint_read(pCheckpoint, pAMR->id,
                                    pAMR->rgn_size - rgn_read_remaining, pRgn_A,
                                    read_cpy_size);
            if (bytes_read < 0) {
                log_error("Checkpoint Read Error: %d", -bytes_read);
                goto RESTART_FAIL;
            }
            rgn_read_remaining -= bytes_read;

            // Wait for RDMA copy to finish
            { // Wait for previous RDMA read to finish
                // Wait for previous RDMA read to finish
                eARM_error arm_status =
                    arm_async_check(pInstance->pARM, &arm_operation, true);
                if (arm_status != eARM_OK) {
                    log_error("ARM Error: %d", arm_status);
                    goto RESTART_FAIL;
                }
            }
            rgn_write_remaining -= cpy_rgn_size;

        } // END Block Copies

        { // Finish final RDMA Write
            eARM_error arm_status =
                arm_write(pInstance->pARM, pAMR,
                          pAMR->pActive_memory + pAMR->rgn_size -
                              rgn_write_remaining,
                          pRgn_A, rgn_write_remaining);
            if (arm_status != eARM_OK) {
                log_error("ARM Error: %d", arm_status);
                goto RESTART_FAIL;
            }
        }

    } // END Region Loop

    { // BEGIN Notify Client of Completion
        // Nofree
        const char *checkpoint_name = afv_checkpoint_name(pCheckpoint);
        char _checkpoint_name[AFV_CKPT_NAME_LEN] = "\0";
        if (!checkpoint_name) {
            // catch nofree with stack
            checkpoint_name = _checkpoint_name;
        }
        int64_t checkpoint_version = afv_checkpoint_version(pCheckpoint);
        eACN_error acn_status = eACN_OK;
        acn_status = acn_set(pInstance->pACN, eACN_version, checkpoint_version);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }
        acn_status = acn_set_name(pInstance->pACN, checkpoint_name);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }

        log_trace("Set Version");

        acn_status = acn_tick(pInstance->pACN, eACN_restore);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }

        log_info("Completion %d: %.*s %d", afv_get_rank(pInstance->pAFV),
                 ACN_NAME_LEN, checkpoint_name, checkpoint_version);
    } // END Notify Client of Completion

    restart_fail = false;
RESTART_FAIL:

    if (amr_restore_order) {
        free(amr_restore_order);
        amr_restore_order = NULL;
    }

    afv_checkpoint_destroy(&pCheckpoint);

    // Log Failure in the instance counter
    if (restart_fail) {
        pInstance->error_counter++;
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

afv_checkpoint_t *_acr_cmd_restart_init(aim_entry_t *pInstance) {
    if (!pInstance) {
        log_warn("NULL Parameter");
        return NULL;
    }

    ASSERT_INSTANCE(pInstance);

    // Wait for memory operations to complete
    eACN_error acn_status = eACN_OK;
    do {
        acn_status = acn_await(pInstance->pACN, eACN_memory);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        log_error("ACN Memory Error 0x%lx", acn_status);
        return NULL;
    }

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
        afv_checkpoint_open(pInstance->pAFV, checkpoint_version,
                            checkpoint_name);

    if (!pCheckpoint) {
        log_error("Failed to open Checkpoint name='%s' version=%d",
                  checkpoint_name, checkpoint_version);
        return NULL;
    }

    return pCheckpoint;
}
