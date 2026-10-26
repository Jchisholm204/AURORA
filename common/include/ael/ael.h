/**
 * @file ael.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief AURORA External Logger interface
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_H_
#define _AEL_H_

#include <stddef.h>
#include <stdint.h>

enum aurora_external_logger_error_e {
    eAEL_OK = 0,
    eAEL_ERR_NULL,
    eAEL_ERR_INTR,
    eAEL_ERR_SEND,
    eAEL_ERR_RECV,
    eAEL_ERR_SIZE,
    eAEL_ERR_EVENT,
    eAEL_ERR_CRC,
    eAEL_N_ERR,
};

struct aurora_external_logger_handle {
    uint64_t rank;
    uint64_t gid;
    uint64_t gsize;
    size_t n_regions;
};

typedef enum aurora_external_logger_error_e eAEL_error;
typedef struct aurora_external_logger_handle ael_hndl;

#endif
