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

eACN_error _acr_cmd_checkpoint_init(aim_entry_t *pInstance);
afv_metadata_t *_acr_cmd_checkpoint_gen_metadata(aim_entry_t *pInstance);

void *acr_cmd_checkpoint(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // All variables in CHECKPOINT_FAIL must be declared here
    aim_entry_t *pInstance = pCtx->pInstance;
    afv_file_hndl *pCkpt_file = NULL;
    afv_metadata_t *pMetadata = NULL;

    // Wait for outstanding operations to complete
    eACN_error acn_status = _acr_cmd_checkpoint_init(pInstance);
    if (acn_status != eACN_OK) {
        log_error("ACN Error %d", acn_status);
        goto CHECKPOINT_FAIL;
    }

    // Read Only Pointer -> Do not Free
    const amr_hndl *const arm_regions = arm_get_remote_regions(pInstance->pARM);
    if (!arm_regions) {
        log_error("Bad Alloc??");
        goto CHECKPOINT_FAIL;
    }

    // Setup Metadata
    pMetadata = _acr_cmd_checkpoint_gen_metadata(pInstance);
    if (!pMetadata) {
        log_error("Error Generating Metadata");
        goto CHECKPOINT_FAIL;
    }

    // Setup checkpoint file
    pCkpt_file = afv_file_open_w(pInstance->pAFV, pMetadata->version,
                                 pMetadata->chkpt_name);

    if (!pCkpt_file) {
        log_error("Bad Alloc??");
        goto CHECKPOINT_FAIL;
    }

    // Setup Copy Regions
    const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
    uint8_t *pRgn_A = pCtx->pScratch;
    uint8_t *pRgn_B = pCtx->pScratch + cpy_rgn_size;

    // Complete the checkpoint
    for (size_t i = 0; i < pMetadata->n_regions; i++) { // BEGIN Region Loop

        const amr_hndl *pAMR = &arm_regions[i];

        // Setup Region Metadata
        pMetadata->region_ids[i] = pAMR->id;
        pMetadata->region_sizes[i] = pAMR->rgn_size;
        memcpy(pMetadata->region_names[i], pAMR->name, ARM_NAME_LEN);

        log_trace("rgn: %d -> rgnid: %d (%d)", i, pAMR->id, pAMR->rgn_size);

        size_t rgn_write_remaining = pAMR->rgn_size;
        size_t rgn_read_remaining = pAMR->rgn_size;

        // Active ARM operation struct
        arm_op arm_operation = {0};
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
            log_debug("Reading %ld", read_cpy_size);
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

            log_debug("Writing %ld", cpy_rgn_size);
            // Write recvd RDMA request B=region[x]
            eAFV_file_error write_status = eAFV_FILE_OK;
            write_status = afv_file_write(pCkpt_file, pRgn_B, cpy_rgn_size);
            if (write_status != eAFV_FILE_OK) {
                log_error("AFV Error: %d", write_status);
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
            eAFV_file_error write_status = eAFV_FILE_OK;
            write_status =
                afv_file_write(pCkpt_file, pRgn_A, rgn_write_remaining);
            if (write_status != eAFV_FILE_OK) {
                log_error("AFV Error: %d", write_status);
                goto CHECKPOINT_FAIL;
            }
        }

    } // END Region Loop

    eAFV_file_error file_close_status = eAFV_FILE_OK;
    file_close_status = afv_file_close(&pCkpt_file);
    if (file_close_status != eAFV_FILE_OK) {
        log_error("FS Error: 0x%x", file_close_status);
        goto CHECKPOINT_FAIL;
    }

    log_info("Completion %d: %.*s %d", pMetadata->rank, ACN_NAME_LEN,
             pMetadata->chkpt_name, pMetadata->version);

    eAFV_verif metadata_status = eAFV_VERIF_OK;
    // Takes ownership of the metadata file (on success)
    metadata_status = afv_write_metadata(pInstance->pAFV, pMetadata);
    if (metadata_status != eAFV_VERIF_OK) {
        log_error("FS Error: 0x%x", metadata_status);
        goto CHECKPOINT_FAIL;
    } else {
        // AFV module takes ownership
        pMetadata = NULL;
    }

    // NOP
    acn_tick(pInstance->pACN, eACN_checkpoint);

CHECKPOINT_FAIL:
    if (pCkpt_file) {
        log_error("Checkpoint Failed");
        afv_file_close(&pCkpt_file);
    }
    if (pMetadata) {
        log_error("Checkpoint Failed");
        afv_destroy_metadata(&pMetadata);
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

eACN_error _acr_cmd_checkpoint_init(aim_entry_t *pInstance) {
    if (!pInstance) {
        log_warn("NULL Parameter");
        return eACN_ERR_NULL;
    }
    eACN_error acn_status = eACN_OK;
    do {
        acn_status = acn_await(pInstance->pACN, eACN_memory);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    return acn_status;
}

afv_metadata_t *_acr_cmd_checkpoint_gen_metadata(aim_entry_t *pInstance) {
    if (!pInstance) {
        log_warn("NULL Parameter");
        return NULL;
    }
    size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

    afv_metadata_t *pMetadata = afv_create_metadata(arm_n_regions);

    if (!pMetadata) {
        log_error("Bad Alloc??");
        return NULL;
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
        return NULL;
    }
    do {
        acn_status = acn_get_name(pInstance->pACN, pMetadata->chkpt_name);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        afv_destroy_metadata(&pMetadata);
        log_error("ACN Error 0x%lx", acn_status);
        return NULL;
    }
    pMetadata->n_regions = arm_n_regions;
    pMetadata->rank = afv_get_rank(pInstance->pAFV);

    return pMetadata;
}
