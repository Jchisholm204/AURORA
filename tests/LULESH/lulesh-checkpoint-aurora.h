/**
 * @file lulesh-checkpoint-aurora.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief AURORA checkpoint layer for LULESH
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _LULESH_CHECKPOINT_AURORA_H_
#define _LULESH_CHECKPOINT_AURORA_H_

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>

#include "aul.h"

static void _lulesh_ckpt_abort(const char *message) {
    printf("AURORA checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

#define LULESH_CKPT_PROTECT_REGION(id, ptr, count, element_size)               \
    do {                                                                       \
        if (((count) > 0) && ((ptr) != NULL)) {                                \
            if (AUL_Mem_protect((uint64_t) (id), (ptr),                        \
                                (uint64_t) (count) *                           \
                                    (uint64_t) (element_size)) != 0) {         \
                _lulesh_ckpt_abort("AUL_Mem_protect failed");                  \
            }                                                                  \
        }                                                                      \
    } while (0)

static void lulesh_ckpt_init(const cmdLineOpts &opts) {
    if (opts.ckptPath == NULL) {
        _lulesh_ckpt_abort("missing -ckpt <checkpoint-directory>");
    }

    int rank;
    int n_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);

    aul_configuration_t configuration = AUL_CONFIG_DEFAULT;
    configuration.rank = (uint64_t) rank;
    configuration.opt_group_id = 0;
    configuration.opt_group_size = n_ranks;
    configuration.persistent_path = opts.ckptPath;
    if (opts.ckptHost != NULL) {
        configuration.opt_hostname = opts.ckptHost;
    }

    if (AUL_Init(&configuration) != 0) {
        _lulesh_ckpt_abort("AUL_Init failed");
    }
}

static int lulesh_ckpt_restart_test(void) {
    int version_local = AUL_Test(-1, LULESH_CKPT_NAME);
    int version_global = 0;

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    if (version_local != version_global) {
        version_local = AUL_Test(version_global, LULESH_CKPT_NAME);
    }

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    return version_global;
}

static int lulesh_ckpt_restart(int version) {
    int restored = AUL_Restart(version, LULESH_CKPT_NAME);
    return (restored == version) ? 0 : -1;
}

static int lulesh_ckpt_checkpoint(int version) {
    return AUL_Checkpoint(version, LULESH_CKPT_NAME);
}

static void lulesh_ckpt_finalize(void) {
    (void) AUL_Finalize();
}

#endif
