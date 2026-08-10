/**
 * @file block_test.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-04-18
 * @modified Last Modified: 2026-07-17
 *
 * @copyright Copyright (c) 2026
 */

#include <memory.h>
// #include <openmpi-x86_64/mpi.h>
#include "veloc.h"

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

    if (argc < 3) {
        printf("Usage: <global_chkpt_size_mb> <veloc_config.cfg> \n");
        exit(3);
    }
    size_t memory_size = 0;
    if (sscanf(argv[1], "%ld", &memory_size) != 1) {
        printf("Wrong memory size! See usage\n");
        exit(3);
    }

    MPI_Init(&argc, &argv);

    int rank, n_ranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);

    if (VELOC_Init(MPI_COMM_WORLD, argv[2]) != VELOC_SUCCESS) {
        printf("Error initializing VELOC! Aborting...\n");
        MPI_Abort(MPI_COMM_WORLD, 3);
    }

    if (rank == 0) {
        printf("[Rank 0] Initialized with %d total ranks\n", n_ranks);
        printf("Each process writing %d MB\n", memory_size / n_ranks);
    }

    size_t proc_mem_size =
        ((size_t) memory_size) * 1024ULL * 1024ULL / ((size_t) n_ranks);
    char *buffer = malloc(proc_mem_size);

    // Fill memory with 'random' values
    for (size_t i = 0; i < proc_mem_size; i++) {
        buffer[i] = (char) (i % 255);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    char ckpt_name[32];
    int ckpt_version = 1;
    snprintf(ckpt_name, sizeof(ckpt_name), "BLOCKTEST");

    MPI_Barrier(MPI_COMM_WORLD);

    BENCH("mem_protect", rank,
          { VELOC_Mem_protect(0, buffer, proc_mem_size, sizeof(char)); });

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0)
        printf("Starting distributed restore...\n");

    // Poison Memory
    for (size_t i = 0; i < proc_mem_size; i++) {
        buffer[i] = 0x3F;
    }

    BENCH("restart", rank, {
        int v = VELOC_Restart_test(ckpt_name, ckpt_version);
        if (VELOC_Restart(ckpt_name, ckpt_version) != VELOC_SUCCESS) {
            printf("[Rank %d] Restart failed\n", rank);
        } else {
            printf("[Rank %d] Restart Success\n", rank);
        }
    });

    size_t failures = 0;
    for (size_t i = 0; i < proc_mem_size; i++) {
        if (buffer[i] != (char) (i % 255)) {
            failures++;
        }
    }

    if (failures) {
        printf("[Rank %d] had %ld restart failures\n", rank, failures);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
        printf("Starting distributed checkpoint...\n");

    // Fill memory with 'random' values
    for (size_t i = 0; i < proc_mem_size; i++) {
        buffer[i] = (char) (i % 255);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    BENCH("ckpt_total", rank, {
        int checkpoint_status = 0;
        BENCH("ckpt_app_block", rank, {
            checkpoint_status = VELOC_Checkpoint(ckpt_name, ckpt_version);
        });

        if (checkpoint_status != VELOC_SUCCESS) {
            printf("[Rank %d] Checkpoint failed with status %d\n", rank,
                   checkpoint_status);
        }

        // Memory regions cannot be unprotected while checkpoint is in progress
        // Use this call to wait for the checkpoint to complete
        BENCH("mem_unprotect", rank, { VELOC_Mem_unprotect(0); });
    });

    MPI_Barrier(MPI_COMM_WORLD);

    if (VELOC_Finalize(0) != VELOC_SUCCESS) {
        printf("[Rank %d] Veloc Finalize Failure", rank);
    }

    MPI_Finalize();
    return 0;
}
