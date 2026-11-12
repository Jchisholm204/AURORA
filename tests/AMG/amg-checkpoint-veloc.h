/**
 * @file amg-checkpoint-veloc.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author GLM-5.3-Flash-Free OpenCode Free
 * @brief VELOC checkpoint layer for AMG
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AMG_CHECKPOINT_VELOC_H_
#define _AMG_CHECKPOINT_VELOC_H_

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>

#include "veloc.h"

static inline void _amg_ckpt_abort(const char *message) {
    printf("VELOC checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

static inline int amg_ckpt_protect_region(uint64_t id, const void *ptr,
                                          uint64_t count, size_t element_size) {
    if ((count == 0) || (ptr == NULL)) {
        return 0;
    }
    if (VELOC_Mem_protect((int) id, (void *) ptr, (int64_t) count,
                          element_size) != VELOC_SUCCESS) {
        _amg_ckpt_abort("VELOC_Mem_protect failed");
        return -1;
    }
    return 0;
}

static inline void amg_ckpt_init(char *ckpt_path, char *ckpt_host) {
    (void) ckpt_host;
    if (ckpt_path == NULL) {
        _amg_ckpt_abort("missing -ckpt <veloc-config-file>");
    }

    if (VELOC_Init(MPI_COMM_WORLD, ckpt_path) != VELOC_SUCCESS) {
        _amg_ckpt_abort("VELOC_Init failed");
    }
}

static inline int amg_ckpt_restart_test(void) {
    return VELOC_Restart_test(AMG_CKPT_NAME, 0);
}

static inline int amg_ckpt_restart(int version) {
    return (VELOC_Restart(AMG_CKPT_NAME, version) == VELOC_SUCCESS) ? 0 : -1;
}

static inline int amg_ckpt_checkpoint(int version) {
    return (VELOC_Checkpoint(AMG_CKPT_NAME, version) == VELOC_SUCCESS) ? 0 : -1;
}

static inline void amg_ckpt_finalize(void) {
    (void) VELOC_Finalize(1);
}

#endif
