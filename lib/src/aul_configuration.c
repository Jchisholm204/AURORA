/**
 * @file aul_configuration.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#include "aul_configuration.h"

const aul_configuration_t AUL_CONFIG_DEFAULT = {
    .persistent_path = "./checkpoints",
    .use_error_correction = false,
    .connection_mode = eAULCModeAuto,
    .opt_ip = NULL,
    .opt_log_file = NULL,
};
