/**
 * @file aul_configuration.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library Configuration
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AUL_CONFIGURATION_H_
#define _AUL_CONFIGURATION_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Aurora Connection Mode
enum eAULConnMode {
    eAULCModeAuto,
    eAULCModeHostName,
    eAULCModeLocalHost,
    eAULCModeTargetIP,

    // Final member
    eACMode_N
};

struct aul_external_logger_client_configuration {
    char *ip;
    uint16_t port;
};

typedef struct {
    // MPI
    uint64_t rank;
    uint64_t opt_group_id;
    int64_t opt_group_size;

    // Checkpointing
    char *persistent_path;
    bool use_error_correction;

    // Connection
    enum eAULConnMode connection_mode;
    char *opt_ip;
    char *opt_hostname;

    // Logging
    char *opt_log_file;
    size_t opt_n_AEL_configs;
    struct aul_external_logger_client_configuration *opt_AEL_configs;
} aul_configuration_t;

typedef struct aul_external_logger_client_configuration aul_el_client_t;
extern const aul_configuration_t AUL_CONFIG_DEFAULT;

#ifdef __cplusplus
}
#endif

#endif
