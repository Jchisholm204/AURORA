/**
 * @file lulesh-checkpoint-veloc.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief VELOC checkpoint layer for LULESH
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _LULESH_CHECKPOINT_VELOC_H_
#define _LULESH_CHECKPOINT_VELOC_H_

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>

#include "veloc.h"

static void _lulesh_ckpt_abort(const char *message) {
    printf("VELOC checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

#define LULESH_CKPT_PROTECT_REGION(id, ptr, count, element_size)               \
    do {                                                                       \
        if (((count) > 0) && ((ptr) != NULL)) {                                \
            if (VELOC_Mem_protect((int) (id), (ptr), (int64_t) (count),        \
                                  (size_t) (element_size)) != VELOC_SUCCESS) { \
                _lulesh_ckpt_abort("VELOC_Mem_protect failed");                \
            }                                                                  \
        }                                                                      \
    } while (0)

static void lulesh_ckpt_init(const cmdLineOpts &opts) {
    if (opts.ckptPath == NULL) {
        _lulesh_ckpt_abort("missing -ckpt <veloc-config-file>");
    }

    if (VELOC_Init(MPI_COMM_WORLD, opts.ckptPath) != VELOC_SUCCESS) {
        _lulesh_ckpt_abort("VELOC_Init failed");
    }
}

static int lulesh_ckpt_restart_test(void) {
    return VELOC_Restart_test(LULESH_CKPT_NAME, 0);
}

static int lulesh_ckpt_restart(int version) {
    return (VELOC_Restart(LULESH_CKPT_NAME, version) == VELOC_SUCCESS) ? 0 : -1;
}

static int lulesh_ckpt_checkpoint(int version) {
    return (VELOC_Checkpoint(LULESH_CKPT_NAME, version) == VELOC_SUCCESS) ? 0
                                                                         : -1;
}

static void lulesh_ckpt_finalize(void) {
    (void) VELOC_Finalize(1);
}

#endif
