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

struct aurora_user_library_context _aul_ctx = {
    .log_file = NULL,
    .pACI = NULL,
    .pACN = NULL,
};

int AUL_Init(const aul_configuration_t *pCFG, const uint64_t proc_id) {
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
    log_trace("\tPID: %d", proc_id);

    ads_exchange_data_t ads_data_tx = {0};

    // Initialize ACI
    _aul_ctx.pACI = aci_create_instance(&ads_data_tx.comm);
    if (!_aul_ctx.pACI) {
        log_fatal("Could not create the ACI instance");
        return -1;
    }
    // Initialize ACN
    _aul_ctx.pACN = acn_create_instance(_aul_ctx.pACI, &ads_data_tx.notif);
    if (!_aul_ctx.pACN) {
        log_fatal("Could not create the ACN instance");
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

    return 0;
}

int AUL_Finalize(void) {
    if (_aul_ctx.log_file) {
        fclose(_aul_ctx.log_file);
        _aul_ctx.log_file = NULL;
    }
    acn_destroy_instance(&_aul_ctx.pACN);
    aci_destroy_instance(&_aul_ctx.pACI);
    return 0;
}

int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                    const size_t size) {
    // Advance the client side memory tick (memory ops pending)
    acn_tick(_aul_ctx.pACN, eACN_memory);
    (void) mem_id;
    (void) ptr;
    (void) size;
    return -1;
}

int AUL_Mem_unprotect(const uint64_t mem_id) {
    // Advance the client side memory tick (memory ops pending)
    acn_tick(_aul_ctx.pACN, eACN_memory);
    (void) mem_id;
    return -1;
}

int AUL_Checkpoint(const int version, const char name[static AUL_NAME_LEN]) {
    // Wait for previous checkpoint to complete
    if (acn_await(_aul_ctx.pACN, eACN_checkpoint | eACN_version) != 0) {
        log_fatal("Server disconnected");
        return INT_MIN;
    }

    // Do Checkpoint

    // Setup the checkpoint name and version
    acn_set_name(_aul_ctx.pACN, name);
    acn_set(_aul_ctx.pACN, eACN_version, version);
    // Trigger for next checkpoint (client side tick)
    acn_tick(_aul_ctx.pACN, eACN_checkpoint | eACN_version);
    return -1;
}

int AUL_Restart(const int version, const char name[static AUL_NAME_LEN]) {
    // Wait for all pending operations to finish
    if (acn_await(_aul_ctx.pACN, eACN_systick | eACN_checkpoint | eACN_restore | eACN_version) != 0) {
        // Connection Failure
        log_fatal("Server disconnected");
        return INT_MIN;
    }

    // Setup the version and name we want
    acn_set_name(_aul_ctx.pACN, name);
    acn_set(_aul_ctx.pACN, eACN_version, version);
    acn_tick(_aul_ctx.pACN, eACN_restore | eACN_version);

    // Wait for restore

    if (acn_await(_aul_ctx.pACN, eACN_restore | eACN_version) != 0) {
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
