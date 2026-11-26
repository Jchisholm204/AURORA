/**
 * @file miniVite-checkpoint-aurora.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Claude-Haiku-5.5 OpenCode Go
 * @brief AURORA checkpoint layer for miniVite
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _MINIVITE_CHECKPOINT_AURORA_H_
#define _MINIVITE_CHECKPOINT_AURORA_H_

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

static const char miniVite_ckpt_name[AUL_NAME_LEN] = MINIVITE_CKPT_NAME;

static void _miniVite_ckpt_abort(const char *message) {
    printf("AURORA checkpoint error: %s\n", message);
    fflush(stdout);
    MPI_Abort(MPI_COMM_WORLD, 2);
}

/*
 * The AURORA engine resolves the persistent path itself and needs it to exist,
 * so resolve relative paths against this process's working directory and
 * create the directory if it is missing.
 */
static char *_miniVite_ckpt_dir(const char *path) {
    static char dir[PATH_MAX];
    const char *rel = path;
    if ((rel[0] == '.') && (rel[1] == '/')) {
        rel += 2;
    }

    if (rel[0] == '/') {
        if (snprintf(dir, sizeof(dir), "%s", rel) >= (int) sizeof(dir)) {
            _miniVite_ckpt_abort("checkpoint path too long");
        }
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            _miniVite_ckpt_abort("getcwd failed");
        }
        if (snprintf(dir, sizeof(dir), "%s/%s", cwd, rel) >= (int) sizeof(dir)) {
            _miniVite_ckpt_abort("checkpoint path too long");
        }
    }

    if ((mkdir(dir, 0755) != 0) && (errno != EEXIST)) {
        _miniVite_ckpt_abort("cannot create checkpoint directory");
    }
    return dir;
}

#define MINIVITE_CKPT_PROTECT_REGION(id, ptr, count, element_size)             \
    do {                                                                       \
        if (((count) > 0) && ((ptr) != NULL)) {                                \
            if (AUL_Mem_protect((uint64_t) (id), (ptr),                        \
                                (uint64_t) (count) *                           \
                                    (uint64_t) (element_size)) != 0) {         \
                _miniVite_ckpt_abort("AUL_Mem_protect failed");                \
            }                                                                  \
        }                                                                      \
    } while (0)

static void miniVite_ckpt_init(char *path, char *host) {
    if (path == NULL) {
        _miniVite_ckpt_abort("missing -ckpt <checkpoint-directory>");
    }

    int rank;
    int n_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);

    aul_configuration_t configuration = AUL_CONFIG_DEFAULT;
    configuration.rank = (uint64_t) rank;
    configuration.opt_group_id = 0;
    configuration.opt_group_size = n_ranks;
    configuration.persistent_path = _miniVite_ckpt_dir(path);
    if (host != NULL) {
        configuration.opt_hostname = host;
    }

    if (AUL_Init(&configuration) != 0) {
        _miniVite_ckpt_abort("AUL_Init failed");
    }
}

static int miniVite_ckpt_restart_test(void) {
    int version_local = AUL_Test(-1, miniVite_ckpt_name);
    int version_global = 0;

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    if (version_local != version_global) {
        version_local = AUL_Test(version_global, miniVite_ckpt_name);
    }

    MPI_Allreduce(&version_local, &version_global, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);

    return version_global;
}

static int miniVite_ckpt_restart(int version) {
    int restored = AUL_Restart(version, miniVite_ckpt_name);
    return (restored == version) ? 0 : -1;
}

static int miniVite_ckpt_checkpoint(int version) {
    return AUL_Checkpoint(version, miniVite_ckpt_name);
}

static void miniVite_ckpt_finalize(void) {
    (void) AUL_Finalize();
}

#endif
