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
#include <unistd.h>

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
        eACN_error acn_status = eACN_OK;
        do {
            acn_status = acn_await(pInstance->pACN, eACN_memory);
            usleep(1000);
        } while (acn_status == eACN_ERR_TIMEOUT);
        if (acn_status != eACN_OK) {
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
        do {
            acn_status = acn_get(pInstance->pACN, eACN_version,
                                 (uint64_t *) &pMetadata->version);
            usleep(1000);
        } while (acn_status == eACN_ERR_TIMEOUT);
        if (acn_status != eACN_OK) {
            afv_destroy_metadata(&pMetadata);
            log_error("ACN Error 0x%lx", acn_status);
            goto CHECKPOINT_FAIL;
        }
        do {
            acn_status = acn_get_name(pInstance->pACN, pMetadata->chkpt_name);
            usleep(1000);
        } while (acn_status == eACN_ERR_TIMEOUT);
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

        // Setup Copy Regions
        const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
        uint8_t *const pRgn_A = pCtx->pScratch;
        uint8_t *const pRgn_B = pCtx->pScratch + cpy_rgn_size;

        // Complete the checkpoint
        for (size_t i = 0; i < pMetadata->n_regions; i++) { // BEGIN Region Loop

            const amr_hndl *pAMR = &arm_regions[i];
            size_t rgn_size = pAMR->rgn_size;
            const size_t rgn_id = pAMR->id;

            // Setup Region Metadata
            pMetadata->region_ids[i] = rgn_id;
            pMetadata->region_sizes[i] = rgn_size;
            memcpy(pMetadata->region_names[i], pAMR->name, ARM_NAME_LEN);

            log_trace("rgn: %d -> rgnid: %d (%d)", i, pAMR->id, pAMR->rgn_size);

            while (rgn_size > cpy_rgn_size) { // BEGIN Block Copies
                const size_t bytes_read = pAMR->rgn_size - rgn_size;
                eAFV_file_error write_status = eAFV_FILE_OK;
                size_t retry_count = 0;
                do {
                    usleep(1000);
                    retry_count++;
                    if (write_status != eAFV_FILE_OK) {
                        log_error("FS Error: 0x%x", write_status);
                    }
                    eARM_error arm_status =
                        arm_read(pInstance->pARM, pAMR,
                                 pAMR->pShadow_memory + bytes_read, pRgn_A,
                                 cpy_rgn_size);
                    if (arm_status != eARM_OK) {
                        log_error("ARM Err %d", arm_status);
                        // Hard Fail
                        if (arm_status == eARM_ERR_FATAL) {
                            (void) afv_file_close(&pCkpt_file);
                            afv_destroy_metadata(&pMetadata);
                            goto CHECKPOINT_FAIL;
                        }
                        // Retry
                        continue;
                    }
                    write_status =
                        afv_file_write(pCkpt_file, pRgn_A, cpy_rgn_size);
                } while (write_status != eAFV_FILE_OK &&
                         retry_count <= ACR_RW_MAX_RETRIES);
                if (retry_count > ACR_RW_MAX_RETRIES) {
                    log_fatal("Retry Count %d exceeded %d", retry_count,
                              ACR_RW_MAX_RETRIES);
                    (void) afv_file_close(&pCkpt_file);
                    afv_destroy_metadata(&pMetadata);
                    goto CHECKPOINT_FAIL;
                }
                // Successfull Write
                usleep(1000);
                rgn_size -= cpy_rgn_size;
            } // END Block Copies

            // BEGIN  Write Final Block
            eAFV_file_error write_status = eAFV_FILE_OK;
            size_t retry_count = 0;
            do {
                usleep(1000);
                retry_count++;
                if (write_status != eAFV_FILE_OK) {
                    log_error("FS Error: 0x%x", write_status);
                }

                const size_t bytes_read = pAMR->rgn_size - rgn_size;
                eARM_error arm_status =
                    arm_read(pInstance->pARM, pAMR,
                             pAMR->pShadow_memory + bytes_read, pRgn_A,
                             rgn_size);
                if (arm_status != eARM_OK) {
                    log_error("ARM Err %d", arm_status);
                    // Hard Fail
                    if (arm_status == eARM_ERR_FATAL) {
                        (void) afv_file_close(&pCkpt_file);
                        afv_destroy_metadata(&pMetadata);
                        goto CHECKPOINT_FAIL;
                    }
                    // Retry
                    continue;
                }
                write_status = afv_file_write(pCkpt_file, pRgn_A, rgn_size);
                // END Write Final Block
            } while (write_status != eAFV_FILE_OK &&
                     retry_count <= ACR_RW_MAX_RETRIES);
            if (retry_count > ACR_RW_MAX_RETRIES) {
                log_fatal("Retry Count %d exceeded %d", retry_count,
                          ACR_RW_MAX_RETRIES);
                (void) afv_file_close(&pCkpt_file);
                afv_destroy_metadata(&pMetadata);
                goto CHECKPOINT_FAIL;
            }

        } // END Region Loop

        eAFV_file_error file_close_status = eAFV_FILE_OK;
        file_close_status = afv_file_close(&pCkpt_file);
        if (file_close_status != eAFV_FILE_OK) {
            log_error("FS Error: 0x%x", file_close_status);
            afv_destroy_metadata(&pMetadata);
            goto CHECKPOINT_FAIL;
        }

    } // END Checkpoint

    log_info("Completion %d: %.*s %d", pMetadata->rank, ACN_NAME_LEN,
             pMetadata->chkpt_name, pMetadata->version);

    eAFV_verif metadata_status = eAFV_VERIF_OK;
    metadata_status = afv_write_metadata(pInstance->pAFV, pMetadata);
    if (metadata_status != eAFV_VERIF_OK) {
        log_error("FS Error: 0x%x", metadata_status);
        afv_destroy_metadata(&pMetadata);
    }

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
