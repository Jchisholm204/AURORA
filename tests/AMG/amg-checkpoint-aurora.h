/**
 * @file amg-checkpoint-aurora.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author GLM-5.3-Flash-Free OpenCode Free
 * @brief AURORA checkpoint layer for AMG
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AMG_CHECKPOINT_AURORA_H_
#define _AMG_CHECKPOINT_AURORA_H_

#include <errno.h>
#include <limits.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "aul.h"

static const char amg_ckpt_name[AUL_NAME_LEN] = AMG_CKPT_NAME;

/*
 * AURORA regions are keyed by registration order, not by ID, so re-protecting
 * a region ID must first unprotect the existing registration. GMRES re-allocates
 * its work arrays on every solve, so every protected ID is re-registered once
 * per solve. Track the active registrations here.
 */
#define AMG_CKPT_MAX_REGIONS 512

static inline void _amg_ckpt_abort(const char *message) {
    printf("AURORA checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

/*
 * The AURORA engine resolves the persistent path itself and needs it to exist,
 * so resolve relative paths against this process's working directory and
 * create the directory if it is missing.
 */
static inline char *_amg_ckpt_dir(const char *path) {
    static char dir[PATH_MAX];
    const char *rel = path;
    if ((rel[0] == '.') && (rel[1] == '/')) {
        rel += 2;
    }

    if (rel[0] == '/') {
        if (snprintf(dir, sizeof(dir), "%s", rel) >= (int) sizeof(dir)) {
            _amg_ckpt_abort("checkpoint path too long");
        }
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            _amg_ckpt_abort("getcwd failed");
        }
        if (snprintf(dir, sizeof(dir), "%s/%s", cwd, rel) >= (int) sizeof(dir)) {
            _amg_ckpt_abort("checkpoint path too long");
        }
    }

    if ((mkdir(dir, 0755) != 0) && (errno != EEXIST)) {
        _amg_ckpt_abort("cannot create checkpoint directory");
    }
    return dir;
}

static inline int amg_ckpt_protect_region(uint64_t id, const void *ptr,
                                          uint64_t count, size_t element_size) {
    static unsigned char ckpt_protected[AMG_CKPT_MAX_REGIONS];

    if ((count == 0) || (ptr == NULL)) {
        return 0;
    }
    if (id >= AMG_CKPT_MAX_REGIONS) {
        _amg_ckpt_abort("region ID out of range");
        return -1;
    }

    if (ckpt_protected[id]) {
        if (AUL_Mem_unprotect(id) != 0) {
            _amg_ckpt_abort("AUL_Mem_unprotect failed");
            return -1;
        }
    }
    if (AUL_Mem_protect(id, ptr,
                        (uint64_t) count * (uint64_t) element_size) != 0) {
        _amg_ckpt_abort("AUL_Mem_protect failed");
        return -1;
    }
    ckpt_protected[id] = 1;
    return 0;
}

static inline void amg_ckpt_init(char *ckpt_path, char *ckpt_host) {
    if (ckpt_path == NULL) {
        _amg_ckpt_abort("missing -ckpt <checkpoint-directory>");
    }

    int rank;
    int n_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);

    aul_configuration_t configuration = AUL_CONFIG_DEFAULT;
    configuration.rank = (uint64_t) rank;
    configuration.opt_group_id = 0;
    configuration.opt_group_size = n_ranks;
    configuration.persistent_path = _amg_ckpt_dir(ckpt_path);
    if (ckpt_host != NULL) {
        configuration.opt_hostname = ckpt_host;
    }

    if (AUL_Init(&configuration) != 0) {
        _amg_ckpt_abort("AUL_Init failed");
    }
}

static inline int amg_ckpt_restart_test(void) {
    int version_local = AUL_Test(-1, amg_ckpt_name);
    int version_global = 0;

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    if (version_local != version_global) {
        version_local = AUL_Test(version_global, amg_ckpt_name);
    }

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    return version_global;
}

static inline int amg_ckpt_restart(int version) {
    int restored = AUL_Restart(version, amg_ckpt_name);
    return (restored == version) ? 0 : -1;
}

static inline int amg_ckpt_checkpoint(int version) {
    return AUL_Checkpoint(version, amg_ckpt_name);
}

static inline void amg_ckpt_finalize(void) {
    (void) AUL_Finalize();
}

#endif
