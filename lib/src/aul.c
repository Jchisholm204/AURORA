/**
 * @file aul.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#define AUL_INTERNAL

#include "aul.h"

#include "ads/ads.h"
#include "aul_internal.h"
#include "limits.h"
#include "log.h"

// Compile time error check name lengths
#if AUL_NAME_LEN != ACN_NAME_LEN
#error "AUL Name Length Must match ACN name length"
#endif

struct aurora_user_library_context _aul_ctx = {
    .log_file = NULL,
    .pACI = NULL,
    .pACN = NULL,
    .pARM = NULL,
    .pConfig = NULL,
};

void _arm_free(const amr_hndl *const pAMR) {
    if (pAMR) {
        if (pAMR->pShadow_memory) {
            free((void *) pAMR->pShadow_memory);
        }
    }
}

int AUL_Init(const aul_configuration_t *pCFG) {
    if (!pCFG) {
        log_error("No Configuration Provided");
        return -1;
    }

    // Setup the log file
    if (pCFG->opt_log_file) {
        _aul_ctx.log_file = fopen(pCFG->opt_log_file, "a");
        if (_aul_ctx.log_file) {
            log_add_fp(_aul_ctx.log_file, LOG_TRACE);
        }
    }

    // Configuration Trace
    log_trace("Initializing AUL:");
    log_trace("\tSAVE: %s", pCFG->persistent_path);
    log_trace("\tEC: %d", pCFG->use_error_correction);
    log_trace("\tCon Mode: %d", pCFG->connection_mode);
    log_trace("\tOpt IP: %s", pCFG->opt_ip);
    log_trace("\tLog File: %s", pCFG->opt_log_file);
    log_trace("\tRank: %d", pCFG->rank);
    log_trace("\tOpt GID: %d", pCFG->opt_group_id);
    log_trace("\tOpt GSize: %d", pCFG->opt_group_size);

    _aul_ctx.pConfig = aoc_alloc(pCFG->persistent_path);
    if (!_aul_ctx.pConfig) {
        log_error("Configuration Failed");
        if (_aul_ctx.log_file) {
            fclose(_aul_ctx.log_file);
            _aul_ctx.log_file = NULL;
        }
        return -2;
    }

    _aul_ctx.pConfig->rank = pCFG->rank;
    _aul_ctx.pConfig->group.id = pCFG->opt_group_id;
    _aul_ctx.pConfig->group.size = pCFG->opt_group_size;
    _aul_ctx.pConfig->group.size = pCFG->opt_group_size;
    _aul_ctx.pConfig->chkpt_opts.use_error_correction =
        pCFG->use_error_correction;

    ads_exchange_data_t ads_data_tx = {0};

    // Pack Configuration Data
    ads_data_tx.config.data = _aul_ctx.pConfig;
    ads_data_tx.config.size = aoc_size(_aul_ctx.pConfig);
    if (!ads_data_tx.config.data) {
        log_fatal("Bad Alloc??");
        AUL_Finalize();
        return -1;
    }

    // Initialize ACI
    _aul_ctx.pACI = aci_create_instance(&ads_data_tx.comm);
    if (!_aul_ctx.pACI) {
        log_fatal("Could not create the ACI instance");
        AUL_Finalize();
        return -1;
    }
    // Initialize ACN
    _aul_ctx.pACN = acn_create_instance(_aul_ctx.pACI, &ads_data_tx.notif);
    if (!_aul_ctx.pACN) {
        log_fatal("Could not create the ACN instance");
        AUL_Finalize();
        return -1;
    }

    ads_exchange_data_t *ads_data_rx = NULL;

    // Use ADS to finalize ACI
    ads_conf_t ads_conf = {
        .timeout_ms = 1000,
        .opt_server_ip = pCFG->opt_ip,
    };
    switch (pCFG->connection_mode) {
    default:
        /* fallthrough */
    case eAULCModeAuto:
        log_trace("Automatic Connection Enabled");
        /* fallthrough */
    case eAULCModeBF:
        ads_conf.opt_server_ip = "192.168.100.2";
        ads_data_rx = ads_request_exchange(&ads_conf, &ads_data_tx);
        if (ads_data_rx || pCFG->connection_mode == eAULCModeBF) {
            break;
        }
        log_warn("Connection to BF Internal Failed");
        /* fallthrough */
    case eAULCModeHost:
        ads_conf.opt_server_ip = "127.0.0.1";
        ads_data_rx = ads_request_exchange(&ads_conf, &ads_data_tx);
        if (ads_data_rx || pCFG->connection_mode == eAULCModeHost) {
            break;
        }
        log_warn("Connection to Host Server Failed");
        /* fallthrough */
    case eAULCModeTarget:
        ads_conf.opt_server_ip = pCFG->opt_ip;
        ads_data_rx = ads_request_exchange(&ads_conf, &ads_data_tx);
        if (ads_data_rx || pCFG->connection_mode == eAULCModeTarget) {
            break;
        }
        log_warn("Connection to Target Server Failed");
    }

    if (!ads_data_rx) {
        log_fatal("All connection methods have failed");
        (void) AUL_Finalize();
        return -1;
    }

    int status = 0;

    // Finalize the ACI with the ADS exchange data
    if ((status = aci_connect_instance(_aul_ctx.pACI, &ads_data_tx.comm,
                                       &ads_data_rx->comm)) != 0) {
        log_fatal("ACI Connection Failed");
        (void) AUL_Finalize();
        return -1;
    }

    // Setup the ACN (Completion Notifications)
    if ((status = acn_connect_instance(_aul_ctx.pACN, &ads_data_tx.notif,
                                       &ads_data_rx->notif)) != 0) {
        log_fatal("ACN Connection Failed");
        (void) AUL_Finalize();
        return -1;
    }

    free(ads_data_rx);

    // Setup the ARM (Region Manager)
    _aul_ctx.pARM = arm_create_instance(_aul_ctx.pACI);

    if (!_aul_ctx.pARM) {
        log_fatal("Failed to create ARM");
        (void) AUL_Finalize();
        return -1;
    }

    return 0;
}

int AUL_Finalize(void) {
    if (_aul_ctx.log_file) {
        fclose(_aul_ctx.log_file);
        _aul_ctx.log_file = NULL;
    }
    // All handlers placed here must be able to handle a hardfault
    aoc_free(&_aul_ctx.pConfig);
    arm_destroy_instance(&_aul_ctx.pARM);
    acn_destroy_instance(&_aul_ctx.pACN);
    aci_destroy_instance(&_aul_ctx.pACI);
    return 0;
}

int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                    const size_t size) {
    // Advance the client side memory tick (memory ops pending)
    int acn_status = 0;
    acn_status = acn_tick(_aul_ctx.pACN, eACN_memory);
    if (acn_status != 0) {
        log_error("ACN Error");
        return acn_status;
    }

    amr_hndl amr = {
        .pActive_memory = (uint64_t) ptr,
        .pShadow_memory = (uint64_t) malloc(size),
        .rgn_size = size,
        .id = mem_id,
        .free = _arm_free,
        .name = "",
        .__reserved = {0ULL},
    };
    eARM_error arm_status = eARM_OK;
    arm_status = arm_add(_aul_ctx.pARM, &amr);
    if (arm_status != eARM_OK) {
        log_error("%d", arm_status);
        return -arm_status;
    }
    return 0;
}

int AUL_Mem_unprotect(const uint64_t mem_id) {
    // Advance the client side memory tick (memory ops pending)
    int acn_status = 0;
    acn_status = acn_await(_aul_ctx.pACN, eACN_checkpoint);
    if (acn_status != 0) {
        log_error("ACN Error");
        return acn_status;
    }

    acn_status = acn_tick(_aul_ctx.pACN, eACN_memory);
    if (acn_status != 0) {
        log_error("ACN Error");
        return acn_status;
    }

    amr_hndl amr = {
        .pActive_memory = 0ULL,
        .rgn_size = 0ULL,
        .id = mem_id,
        .free = NULL,
        .name = "",
        .__reserved = {0ULL},
    };
    eARM_error arm_status = eARM_OK;
    arm_status = arm_remove(_aul_ctx.pARM, &amr);
    if (arm_status != eARM_OK) {
        return -arm_status;
    }
    return 0;
}

int AUL_Checkpoint(const int version, const char name[static AUL_NAME_LEN]) {
    // Wait for previous checkpoint to complete
    if (acn_await(_aul_ctx.pACN, eACN_checkpoint) != 0) {
        log_fatal("Server disconnected");
        return INT_MIN;
    }

    // Do Checkpoint

    // Setup the checkpoint name and version
    acn_set_name(_aul_ctx.pACN, name);
    acn_set(_aul_ctx.pACN, eACN_version, version);
    // Trigger for next checkpoint (client side tick)
    acn_tick(_aul_ctx.pACN, eACN_checkpoint);
    return -1;
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
