/**
 * @file test_mpi_checkpoint.c
 * @brief MPI-enabled test for the AUL checkpoint/restart framework
 * AI AI AI
 * This was written with AI
 */

#include "aul.h"

#include <memory.h>
#include <mpi.h>
#include <stdio.h>

#define MEM_SIZE 4

int main(int argc, char **argv) {
    // 1. Initialize MPI environment
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 2. Initialize AUL (Ideally on all ranks if they perform local I/O)
    aul_configuration_t cfg = AUL_CONFIG_DEFAULT;
    cfg.rank = rank;
    cfg.opt_group_id = rank;
    cfg.opt_group_size = size;
    int ini_s = AUL_Init(&cfg);

    if (rank == 0) {
        printf("[Rank 0] AUL Initialized %d with %d total ranks\n", ini_s,
               size);
    }

    // 3. Create rank-specific data
    // Each rank has unique data: 0xABCD + rank index
    uint64_t buf[MEM_SIZE];
    uint64_t original[MEM_SIZE];
    for (int i = 0; i < MEM_SIZE; i++) {
        buf[i] = (uint64_t) (0xABCD0000 + (rank * 100) + i);
        original[i] = buf[i];
    }

    // 4. Register memory with AUL
    // Edge Case: Ensuring all ranks register their local buffers
    AUL_Mem_protect(0, buf, sizeof(buf));

    // 5. Checkpoint Logic
    // Sync before checkpointing to ensure all ranks are ready
    MPI_Barrier(MPI_COMM_WORLD);

    char ckpt_name[32];
    snprintf(ckpt_name, sizeof(ckpt_name), "MpiCkpt_R%d", rank);

    if (rank == 0)
        printf("Starting distributed checkpoint...\n");

    int s = AUL_Checkpoint(1, ckpt_name);

    if (s != 0) {
        printf("[Rank %d] Checkpoint failed with status %d\n", rank, s);
    }

    // 6. Corrupt the data on all ranks
    for (int i = 0; i < MEM_SIZE; i++) {
        buf[i] = 0;
    }

    // 7. Restart Logic
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0)
        printf("Starting distributed restore...\n");

    int v = AUL_Restart(1, ckpt_name);

    // 8. Verification & Edge Case Handling
    int local_errors = 0;
    for (int i = 0; i < MEM_SIZE; i++) {
        if (buf[i] != original[i]) {
            printf("[Rank %d] Error: Index %d (0x%lx != 0x%lx)\n", rank, i,
                   buf[i], original[i]);
            local_errors++;
        }
    }

    // Global Error Aggregation
    int global_errors = 0;
    MPI_Reduce(&local_errors, &global_errors, 1, MPI_INT, MPI_SUM, 0,
               MPI_COMM_WORLD);

    if (rank == 0) {
        if (global_errors == 0) {
            printf("SUCCESS: All ranks restored correctly.\n");
        } else {
            printf("FAILURE: %d total errors detected across cluster.\n",
                   global_errors);
        }
        AUL_Finalize();
        printf("AUL Finalized\n");
    } else {
        // Ranks other than 0 should still finalize if they initialized
        AUL_Finalize();
    }

    MPI_Finalize();
    return 0;
}
