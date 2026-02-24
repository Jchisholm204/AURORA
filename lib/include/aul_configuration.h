/**
 * @file aul_configuration.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library Configuration
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AUL_CONFIGURATION_H_
#define _AUL_CONFIGURATION_H_

#include <stdbool.h>
#include <stddef.h>

// Aurora Connection Mode
enum eAULConnMode {
    eAULCModeAuto,
    eAULCModeBF,
    eAULCModeHost,
    eAULCModeTarget,

    // Final member
    eACMode_N
};

typedef struct {
    char *persistent_path;
    bool use_error_correction;
    enum eAULConnMode connection_mode;
    char *opt_ip;
    char *opt_log_file;
} aul_configuration_t;

extern const aul_configuration_t AUL_CONFIG_DEFAULT;

#endif
