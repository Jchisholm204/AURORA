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
#define AFV_METADATA_INC_MATCH
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

#if ACN_NAME_LEN != AFV_CKPT_NAME_LEN
#error "ACN Name Length Must Match AFV Checkpoint Name Length"
#endif

void *acr_cmd_restart(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;
    aim_entry_t *pInstance = pCtx->pInstance;
    const afv_metadata_t *pMetadata = NULL;

    { // BEGIN Wait for outstanding memory operations to complete
        eACN_error acn_status = eACN_OK;
        acn_status = acn_await(pInstance->pACN, eACN_memory);
        if(acn_status == eACN_ERR_TIMEOUT){
            // Silent Fail for timeouts
            goto RESTART_FAIL;
        }
        if (acn_status != eACN_OK) {
            log_error("ACN Error %d", acn_status);
            goto RESTART_FAIL;
        }
    } // END Wait for outstanding memory operations to complete


    { // BEGIN setup metadata
        eACN_error acn_status = eACN_OK;

        int64_t cli_req_version;
        acn_status = acn_get(pInstance->pACN, eACN_version,
                             (uint64_t *) &cli_req_version);
        if (acn_status != eACN_OK) {
            log_error("ACN Error 0x%lx", acn_status);
            goto RESTART_FAIL;
        }
        char cli_req_name[AFV_CKPT_NAME_LEN];
        acn_get_name(pInstance->pACN, cli_req_name);
        if (acn_status != eACN_OK) {
            log_error("ACN Error 0x%lx", acn_status);
            goto RESTART_FAIL;
        }

        pMetadata = afv_get_metadata_versioned(pInstance->pAFV, cli_req_version,
                                               cli_req_name);

        if (!pMetadata) {
            log_warn("Bad Alloc??");
            pMetadata =
                afv_get_metadata_versioned(pInstance->pAFV, -1, cli_req_name);
        }
        if (!pMetadata) {
            log_warn("Bad Alloc??");
            pMetadata = afv_get_metadata_versioned(pInstance->pAFV, -1, NULL);
        }
        if (!pMetadata) {
            log_error("Bad Alloc??");
            goto RESTART_FAIL;
        }

        log_debug("Restore Triggered -> %d: %.*s (%d)", pMetadata->rank,
                  ACN_NAME_LEN, pMetadata->chkpt_name, pMetadata->version);

    } // END setup metadata

    size_t *meta_cli_hash_map = malloc(sizeof(size_t) * pMetadata->n_regions);
    if (!meta_cli_hash_map) {
        log_warn("Bad Alloc??");
        meta_cli_hash_map = malloc(sizeof(size_t) * pMetadata->n_regions);
    }

    if (!meta_cli_hash_map) {
        log_error("Bad Alloc??");
        goto RESTART_FAIL;
    }

    { // BEGIN Verify State
        // PRE:
        //  Metadata Loaded
        //  meta_cli_hash_map memory allocated (uninitialized)
        //  pInstance

        // Read Only Pointer
        // Do not Free
        const amr_hndl *const cli_arm_regions =
            arm_get_remote_regions(pInstance->pARM);
        if (!cli_arm_regions) {
            log_error("ARM Error");
            afv_destroy_metadata((afv_metadata_t **) &pMetadata);
            free(meta_cli_hash_map);
            goto RESTART_FAIL;
        }

        const size_t cli_arm_n_regions =
            arm_get_n_remote_regions(pInstance->pARM);

        eAFV_verif afv_status = eAFV_VERIF_OK;
        afv_status = afv_metadata_match(pMetadata, cli_arm_regions,
                                        cli_arm_n_regions, meta_cli_hash_map);

        if (afv_status != eAFV_VERIF_OK) {
            log_error("Verification 0x%lx", afv_status);
            afv_destroy_metadata((afv_metadata_t **) &pMetadata);
            free(meta_cli_hash_map);
            goto RESTART_FAIL;
        }

        // POST:
        //  meta_cli_hash_map: [metadata idx] -> [arm idx]
    } // END Verify State

    { // BEGIN Restore
        // Setup checkpoint file
        afv_file_hndl *pCkpt_file = afv_file_open_r(pInstance->pAFV, pMetadata);

        if (!pCkpt_file) {
            afv_destroy_metadata((afv_metadata_t **) &pMetadata);
            log_error("Bad Alloc??");
            goto RESTART_FAIL;
        }

        // Read Only Pointer
        // Do not Free
        const amr_hndl *const arm_regions =
            arm_get_remote_regions(pInstance->pARM);
        if (!arm_regions) {
            log_error("ARM Error");
            goto RESTART_FAIL;
        }

        // Setup Copy Regions
        const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
        uint8_t *const pRgn_A = pCtx->pScratch;
        uint8_t *const pRgn_B = pCtx->pScratch + cpy_rgn_size;

        // Complete the checkpoint
        for (size_t i = 0; i < pMetadata->n_regions; i++) { // BEGIN Region Loop

            // Index into the ARM through the hash map
            const size_t cli_amr_index = meta_cli_hash_map[i];
            const amr_hndl *pAMR = &arm_regions[cli_amr_index];
            size_t rgn_size = pAMR->rgn_size;
            const size_t rgn_id = pAMR->id;

            // Setup Region Metadata
            pMetadata->region_ids[i] = rgn_id;
            pMetadata->region_sizes[i] = rgn_size;
            memcpy(pMetadata->region_names[i], pAMR->name, ARM_NAME_LEN);

            log_debug("rgn: %d -> rgnid: %d (%d)", i, pAMR->id, pAMR->rgn_size);

            while (rgn_size > cpy_rgn_size) { // BEGIN Block Copies
                eAFV_file_error write_status = eAFV_FILE_OK;
                write_status = afv_file_read(pCkpt_file, pRgn_A, cpy_rgn_size);
                if (write_status != eAFV_FILE_OK) {
                    log_error("FS Error: 0x%x", write_status);
                    // Retry
                    continue;
                }
                const size_t bytes_read = pAMR->rgn_size - rgn_size;
                eARM_error arm_status =
                    arm_write(pInstance->pARM, pAMR,
                              pAMR->pActive_memory + bytes_read, pRgn_A,
                              cpy_rgn_size);
                if (arm_status != eARM_OK) {
                    log_error("ARM Err %d", arm_status);
                    // Retry
                    continue;
                }
                // Successfull Write
                rgn_size -= cpy_rgn_size;
            } // END Block Copies

            { // BEGIN  Write Final Block
                eAFV_file_error write_status = eAFV_FILE_OK;
                write_status = afv_file_read(pCkpt_file, pRgn_A, rgn_size);
                if (write_status != eAFV_FILE_OK) {
                    log_error("FS Error: 0x%x", write_status);
                    // Retry
                    continue;
                }
                const size_t bytes_read = pAMR->rgn_size - rgn_size;
                eARM_error arm_status =
                    arm_write(pInstance->pARM, pAMR,
                              pAMR->pActive_memory + bytes_read, pRgn_A,
                              pAMR->rgn_size);
                if (arm_status != eARM_OK) {
                    log_error("ARM Err %d", arm_status);
                    // Retry
                    continue;
                }
            } // END Write Final Block

        } // END Region Loop

        eAFV_file_error file_close_status = eAFV_FILE_OK;
        file_close_status = afv_file_close(&pCkpt_file);
        if (file_close_status != eAFV_FILE_OK) {
            log_error("FS Error: 0x%x", file_close_status);
            afv_destroy_metadata((afv_metadata_t **) &pMetadata);
            free(meta_cli_hash_map);
            goto RESTART_FAIL;
        }

        free(meta_cli_hash_map);
        meta_cli_hash_map = NULL;

    } // END Restore

    { // BEGIN Notify Client of Completion
        int acn_status = 0;
        acn_status = acn_set(pInstance->pACN, eACN_version, pMetadata->version);
        if (acn_status != 0) {
            log_warn("ACN Abnormal %d", acn_status);
        }
        acn_status = acn_set_name(pInstance->pACN, pMetadata->chkpt_name);
        if (acn_status != 0) {
            log_warn("ACN Abnormal %d", acn_status);
        }

        log_trace("Set Version");

        acn_status = acn_tick(pInstance->pACN, eACN_restore);
        if (acn_status != 0) {
            log_warn("ACN Abnormal %d", acn_status);
        }

        log_trace("Ticked Restore");
    } // END Notify Client of Completion

RESTART_FAIL:

    afv_destroy_metadata((afv_metadata_t **) &pMetadata);

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
