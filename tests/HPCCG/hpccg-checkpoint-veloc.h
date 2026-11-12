/**
 * @file hpccg-checkpoint-veloc.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4 Pro
 * @brief VELOC checkpoint layer for HPCCG
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _HPCCG_CHECKPOINT_VELOC_H_
#define _HPCCG_CHECKPOINT_VELOC_H_

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>

#include "veloc.h"

static void _hpccg_ckpt_abort(const char *message) {
    printf("VELOC checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

#define HPCCG_CKPT_PROTECT_REGION(id, ptr, count, element_size)               \
    do {                                                                       \
        if (((count) > 0) && ((ptr) != NULL)) {                                \
            if (VELOC_Mem_protect((int) (id), (ptr), (int64_t) (count),        \
                                  (size_t) (element_size)) != VELOC_SUCCESS) { \
                _hpccg_ckpt_abort("VELOC_Mem_protect failed");                \
            }                                                                  \
        }                                                                      \
    } while (0)

static void hpccg_ckpt_init(char *ckpt_path, char *ckpt_host) {
    (void) ckpt_host;
    if (ckpt_path == NULL) {
        _hpccg_ckpt_abort("missing -ckpt <veloc-config-file>");
    }

    if (VELOC_Init(MPI_COMM_WORLD, ckpt_path) != VELOC_SUCCESS) {
        _hpccg_ckpt_abort("VELOC_Init failed");
    }
}

static int hpccg_ckpt_restart_test(void) {
    return VELOC_Restart_test(HPCCG_CKPT_NAME, 0);
}

static int hpccg_ckpt_restart(int version) {
    return (VELOC_Restart(HPCCG_CKPT_NAME, version) == VELOC_SUCCESS) ? 0 : -1;
}

static int hpccg_ckpt_checkpoint(int version) {
    return (VELOC_Checkpoint(HPCCG_CKPT_NAME, version) == VELOC_SUCCESS) ? 0
                                                                         : -1;
}

static void hpccg_ckpt_finalize(void) {
    (void) VELOC_Finalize(1);
}

#endif
