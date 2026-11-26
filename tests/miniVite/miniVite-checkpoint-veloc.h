/**
 * @file miniVite-checkpoint-veloc.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Claude-Haiku-5.5 OpenCode Go
 * @brief VELOC checkpoint layer for miniVite
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _MINIVITE_CHECKPOINT_VELOC_H_
#define _MINIVITE_CHECKPOINT_VELOC_H_

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>

#include "veloc.h"

static void _miniVite_ckpt_abort(const char *message) {
    printf("VELOC checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

#define MINIVITE_CKPT_PROTECT_REGION(id, ptr, count, element_size)             \
    do {                                                                       \
        if (((count) > 0) && ((ptr) != NULL)) {                                \
            if (VELOC_Mem_protect((int) (id), (ptr), (int64_t) (count),        \
                                  (size_t) (element_size)) != VELOC_SUCCESS) { \
                _miniVite_ckpt_abort("VELOC_Mem_protect failed");              \
            }                                                                  \
        }                                                                      \
    } while (0)

static void miniVite_ckpt_init(char *path, char *host) {
    (void) host;
    if (path == NULL) {
        _miniVite_ckpt_abort("missing -ckpt <veloc-config-file>");
    }

    if (VELOC_Init(MPI_COMM_WORLD, path) != VELOC_SUCCESS) {
        _miniVite_ckpt_abort("VELOC_Init failed");
    }
}

static int miniVite_ckpt_restart_test(void) {
    return VELOC_Restart_test(MINIVITE_CKPT_NAME, 0);
}

static int miniVite_ckpt_restart(int version) {
    return (VELOC_Restart(MINIVITE_CKPT_NAME, version) == VELOC_SUCCESS) ? 0
                                                                         : -1;
}

static int miniVite_ckpt_checkpoint(int version) {
    return (VELOC_Checkpoint(MINIVITE_CKPT_NAME, version) == VELOC_SUCCESS)
               ? 0
               : -1;
}

static void miniVite_ckpt_finalize(void) {
    (void) VELOC_Finalize(1);
}

#endif
