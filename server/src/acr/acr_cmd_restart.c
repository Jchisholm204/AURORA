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

#if ACN_NAME_LEN != AFV_CKPT_NAME_LEN
#error "ACN Name Length Must Match AFV Checkpoint Name Length"
#endif
eACN_error _acr_cmd_restart_init(aim_entry_t *pInstance);
const afv_metadata_t *_acr_cmd_restore_gen_metadata(aim_entry_t *pInstance);
size_t *_acr_cmd_restart_gen_mapping(aim_entry_t *pInstance,
                                     const afv_metadata_t *pMetadata);

void *acr_cmd_restart(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // All variables in RESTART_FAIL must be declared here
    aim_entry_t *pInstance = pCtx->pInstance;
    const afv_metadata_t *pMetadata = NULL;
    afv_file_hndl *pCkpt_file = NULL;
    size_t *meta_cli_hash_map = NULL;

    eACN_error acn_status = _acr_cmd_restart_init(pInstance);
    if (acn_status != eACN_OK) {
        log_error("ACN Error %d", acn_status);
        goto RESTART_FAIL;
    }

    pMetadata = _acr_cmd_restore_gen_metadata(pInstance);
    if (!pMetadata) {
        log_error("Metadata Failure");
        goto RESTART_FAIL;
    }

    log_debug("Restore Triggered -> %d: %.*s (%d)", pMetadata->rank,
              ACN_NAME_LEN, pMetadata->chkpt_name, pMetadata->version);

    meta_cli_hash_map = _acr_cmd_restart_gen_mapping(pInstance, pMetadata);
    if (!meta_cli_hash_map) {
        log_error("Metadata Verification Failure");
        goto RESTART_FAIL;
    }

    // Setup checkpoint file
    // Call can fail due to FS lock errors
    pCkpt_file = afv_file_open_r(pInstance->pAFV, pMetadata);
    if (!pCkpt_file) {
        log_warn("Bad Alloc??");
        // Retry the FS lock
        pCkpt_file = afv_file_open_r(pInstance->pAFV, pMetadata);
        if (!pCkpt_file) {
            // Repeated Failure indicates hard failure
            log_error("Bad Alloc??");
            goto RESTART_FAIL;
        }
    }

    // Read Only Pointer
    // Do not Free
    const amr_hndl *const arm_regions = arm_get_remote_regions(pInstance->pARM);
    if (!arm_regions) {
        log_error("ARM Error");
        (void) afv_file_close(&pCkpt_file);
        goto RESTART_FAIL;
    }

    // Setup Copy Regions
    const size_t cpy_rgn_size = (ACR_CMD_CTX_SCRATCH_SIZE >> 1);
    uint8_t *pRgn_A = pCtx->pScratch;
    uint8_t *pRgn_B = pCtx->pScratch + cpy_rgn_size;

    // Complete the checkpoint
    for (size_t i = 0; i < pMetadata->n_regions; i++) { // BEGIN Region Loop
        // Index into the ARM through the hash map
        const size_t cli_amr_index = meta_cli_hash_map[i];
        const amr_hndl *pAMR = &arm_regions[cli_amr_index];

        // Setup Region Metadata
        pMetadata->region_ids[i] = pAMR->id;
        pMetadata->region_sizes[i] = pAMR->rgn_size;
        memcpy(pMetadata->region_names[i], pAMR->name, ARM_NAME_LEN);

        log_trace("rgn: %d -> rgnid: %d (%d) (%d)", i, pAMR->id, pAMR->rgn_size,
                  pMetadata->rank);

        size_t rgn_write_remaining = pAMR->rgn_size;
        size_t rgn_read_remaining = pAMR->rgn_size;

        arm_op arm_operation = {0};
        { // File Read for A=region[x]
            const size_t read_cpy_size = rgn_read_remaining >= cpy_rgn_size
                                             ? cpy_rgn_size
                                             : rgn_read_remaining;
            eAFV_file_error afv_status =
                afv_file_read(pCkpt_file, pRgn_A, read_cpy_size);
            if (afv_status != eAFV_FILE_OK) {
                log_error("AFV Error: %d", afv_status);
                goto RESTART_FAIL;
            }
            rgn_read_remaining -= read_cpy_size;
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
            eAFV_file_error afv_status =
                afv_file_read(pCkpt_file, pRgn_A, read_cpy_size);
            if (afv_status != eAFV_FILE_OK) {
                log_error("AFV Error: %d", afv_status);
                goto RESTART_FAIL;
            }
            rgn_read_remaining -= read_cpy_size;

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

    eAFV_file_error file_close_status = eAFV_FILE_OK;
    file_close_status = afv_file_close(&pCkpt_file);
    if (file_close_status != eAFV_FILE_OK) {
        log_error("FS Error: 0x%x", file_close_status);
        goto RESTART_FAIL;
    }

    { // BEGIN Notify Client of Completion
        eACN_error acn_status = eACN_OK;
        acn_status = acn_set(pInstance->pACN, eACN_version, pMetadata->version);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }
        acn_status = acn_set_name(pInstance->pACN, pMetadata->chkpt_name);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }

        log_trace("Set Version");

        acn_status = acn_tick(pInstance->pACN, eACN_restore);
        if (acn_status != eACN_OK) {
            log_warn("ACN Abnormal 0x%x", acn_status);
        }

        log_info("Completion %d: %.*s %d", pMetadata->rank, ACN_NAME_LEN,
                 pMetadata->chkpt_name, pMetadata->version);
    } // END Notify Client of Completion

RESTART_FAIL:
    if (meta_cli_hash_map) {
        free(meta_cli_hash_map);
        meta_cli_hash_map = NULL;
    }

    afv_destroy_metadata((afv_metadata_t **) &pMetadata);

    if(pCkpt_file){
        afv_file_close(&pCkpt_file);
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

eACN_error _acr_cmd_restart_init(aim_entry_t *pInstance) {
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

const afv_metadata_t *_acr_cmd_restore_gen_metadata(aim_entry_t *pInstance) {
    if (!pInstance) {
        log_warn("NULL Parameter");
        return NULL;
    }
    eACN_error acn_status = eACN_OK;
    int64_t cli_req_version = -1;
    char cli_req_name[AFV_CKPT_NAME_LEN];

    do {
        acn_status = acn_get(pInstance->pACN, eACN_version,
                             (uint64_t *) &cli_req_version);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        log_error("ACN Error 0x%lx", acn_status);
        return NULL;
    }

    do {
        acn_status = acn_get_name(pInstance->pACN, cli_req_name);
        usleep(1000);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        log_error("ACN Error 0x%lx", acn_status);
        return NULL;
    }

    // Can fail due to transient FS errors and/or lock errors
    // Catch with a NULL retry mechanim
    const afv_metadata_t *pMetadata =
        afv_get_metadata_versioned(pInstance->pAFV, cli_req_version,
                                   cli_req_name);
    if (!pMetadata) {
        log_warn("Bad Alloc??");
        // Retry intermitent failure
        pMetadata = afv_get_metadata_versioned(pInstance->pAFV, cli_req_version,
                                               cli_req_name);
        // Repeated fail indicates hard fault
        if (!pMetadata) {
            log_error("Bad Metadata Lookup %s %d", cli_req_name,
                      cli_req_version);
            return NULL;
        }
    }

    return pMetadata;
}

size_t *_acr_cmd_restart_gen_mapping(aim_entry_t *pInstance,
                                     const afv_metadata_t *pMetadata) {
    if (!pInstance || !pMetadata) {
        log_warn("NULL Parameter");
        return NULL;
    }

    // Read only PTR -> DO NOT FREE
    const amr_hndl *const cli_arm_regions =
        arm_get_remote_regions(pInstance->pARM);
    if (!cli_arm_regions) {
        log_error("Bad Alloc??");
        return NULL;
    }

    size_t *meta_cli_hash_map = malloc(sizeof(size_t) * pMetadata->n_regions);
    if (!meta_cli_hash_map) {
        log_error("Bad Alloc??");
        return NULL;
    }

    const size_t cli_arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);

    eAFV_verif afv_status = eAFV_VERIF_OK;
    afv_status = afv_metadata_match(pMetadata, cli_arm_regions,
                                    cli_arm_n_regions, meta_cli_hash_map);
    if (afv_status != eAFV_VERIF_OK) {
        log_error("Verification 0x%lx", afv_status);
        free(meta_cli_hash_map);
        meta_cli_hash_map = NULL;
        return NULL;
    }

    return meta_cli_hash_map;
}
