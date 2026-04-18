/**
 * @file aul_init.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library - Initialization
 * @version 0.2
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AUL_INTERNAL

#include "ads/ads.h"
#include "aul.h"
#include "aul_internal.h"
#include "log.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>

static char *find_host_bf(void) {
    static char hostname[256];
    static char target_name[256];
    struct hostent *he;

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return NULL;
    }

    char *digits = hostname;
    while (*digits && !isdigit(*digits)) {
        digits++;
    }
    if (*digits == '\0') {
        return NULL;
    }

    const int versions[] = {3, 2, 1};
    for (size_t i = 0; i < sizeof(versions) / sizeof(int); i++) {
        int version = versions[i];
        (void) snprintf(target_name, sizeof(target_name), "romebf%da%s",
                        version, digits);
        log_debug("Looking for BF%d @ %s", version, target_name);
        he = gethostbyname(target_name);
        if (he) {
            struct in_addr in_addr;
            (void) memcpy(&in_addr, he->h_addr_list[0], sizeof(struct in_addr));
            return inet_ntoa(in_addr);
        }
    }

    log_trace("BF hostname not found");

    return NULL;
}

int AUL_Init(const aul_configuration_t *pCFG) {
    if (!pCFG) {
        log_error("No Configuration Provided");
        return -1;
    }

    log_set_level(LOG_DEBUG);

    // Setup the log file
    if (pCFG->opt_log_file) {
        _aul_ctx.log_file = fopen(pCFG->opt_log_file, "a");
        if (_aul_ctx.log_file) {
            log_add_fp(_aul_ctx.log_file, LOG_TRACE);
            log_set_level(LOG_INFO);
        }
    }

    // Configuration Trace
    log_info("Initializing AUL");
    log_debug("\tSAVE: %s", pCFG->persistent_path);
    log_trace("\tEC: %d", pCFG->use_error_correction);
    log_trace("\tCon Mode: %d", pCFG->connection_mode);
    log_trace("\tOpt IP: %s", pCFG->opt_ip);
    log_debug("\tLog File: %s", pCFG->opt_log_file);
    log_debug("\tRank: %d", pCFG->rank);
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

    _aul_ctx.pAFV =
        afv_create_instance(_aul_ctx.pConfig->rank, _aul_ctx.pConfig->group.id,
                            _aul_ctx.pConfig->group.size,
                            _aul_ctx.pConfig->chkpt_opts.persistent_path,
                            _aul_ctx.pConfig->chkpt_opts.use_error_correction);

    if (!_aul_ctx.pAFV) {
        log_fatal("Configuration Failed");
        aoc_free(&_aul_ctx.pConfig);
        if (_aul_ctx.log_file) {
            fclose(_aul_ctx.log_file);
            _aul_ctx.log_file = NULL;
        }
        return -2;
    }

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
        .timeout_ms = 15000,
        .opt_server_ip = pCFG->opt_ip,
    };
    switch (pCFG->connection_mode) {
    default:
        /* fallthrough */
    case eAULCModeAuto:
        log_trace("Automatic Connection Enabled");
        /* fallthrough */
    case eAULCModeBF:
        // Find BF through hostname search
        ads_conf.opt_server_ip = find_host_bf();
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
    _aul_ctx.pARM = arm_create_instance(_aul_ctx.pACI, _aul_ctx.pACN);

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
    afv_destroy_instance(&_aul_ctx.pAFV);
    return 0;
}
