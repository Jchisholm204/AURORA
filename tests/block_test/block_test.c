/**
 * @file block_test.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-18
 * @modified Last Modified: 2026-04-18
 *
 * @copyright Copyright (c) 2026
 */

#include "aul.h"

#include <memory.h>
// #include <openmpi-x86_64/mpi.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BENCH(name, rank, func)                                                \
    do {                                                                       \
        double t_start, t_local;                                               \
        t_start = MPI_Wtime();                                                 \
        func;                                                                  \
        t_local = MPI_Wtime() - t_start;                                       \
        printf("__RANK_BENCH__ {\"id\":\"%s\",\"rank\":%d,\"time\":%.9f}\n",   \
               name, rank, t_local * 1000);                                    \
    } while (0)

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Usage: %s <global_chkpt_size_kb>\n", argv[0]);
        exit(3);
    }
    int memory_size = 0;
    if (sscanf(argv[1], "%d", &memory_size) != 1) {
        printf("Wrong memory size! See usage\n");
        exit(3);
    }

    MPI_Init(&argc, &argv);

    int rank, n_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);

    // 2. Initialize AUL (Ideally on all ranks if they perform local I/O)
    aul_configuration_t cfg = AUL_CONFIG_DEFAULT;
    cfg.rank = rank;
    cfg.opt_group_id = rank;
    cfg.opt_group_size = n_ranks;
    int aul_status = AUL_Init(&cfg);

    if (aul_status != 0) {
        printf("AUL Init Failed");
        MPI_Abort(MPI_COMM_WORLD, 3);
    }

    if (rank == 0) {
        printf("[Rank 0] AUL Initialized %d with %d total ranks\n", aul_status,
               n_ranks);
        printf("Each process writing %d KB\n", memory_size / n_ranks);
    }

    char *buffer = malloc(memory_size * 1024 / n_ranks);

    MPI_Barrier(MPI_COMM_WORLD);

    BENCH("mem_protect", rank,
          { AUL_Mem_protect(0, buffer, memory_size * 1024 / n_ranks); });

    MPI_Barrier(MPI_COMM_WORLD);

    char ckpt_name[32];
    int ckpt_version = 1;
    snprintf(ckpt_name, sizeof(ckpt_name), "BLOCKTEST");

    if (rank == 0)
        printf("Starting distributed checkpoint...\n");

    MPI_Barrier(MPI_COMM_WORLD);

    BENCH("ckpt_total", rank, {
        int checkpoint_status = 0;
        BENCH("ckpt_start", rank,
              { checkpoint_status = AUL_Checkpoint(ckpt_version, ckpt_name); });

        if (checkpoint_status != 0) {
            printf("[Rank %d] Checkpoint failed with status %d\n", rank,
                   checkpoint_status);
        }

        // Memory regions cannot be unprotected while checkpoint is in progress
        // Use this call to wait for the checkpoint to complete
        BENCH("mem_unprotect", rank, { AUL_Mem_unprotect(0); });
    });

    // Need to re-register region for the restore to work
    AUL_Mem_protect(0, buffer, memory_size * 1024 / n_ranks);

    // 7. Restart Logic
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0)
        printf("Starting distributed restore...\n");

    BENCH("restart", rank, {
        if (AUL_Restart(ckpt_version, ckpt_name) != ckpt_version) {
            printf("[Rank %d] Restart failed\n", rank);
        }
    });

    AUL_Finalize();

    MPI_Finalize();
    return 0;
}
