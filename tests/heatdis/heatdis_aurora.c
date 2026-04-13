#include "aul.h"
#include "heatdis.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    This sample application is based on the heat distribution code
    originally developed within the FTI project: github.com/leobago/fti
*/

static const unsigned int CKPT_FREQ = ITER_TIMES / 3;

void initData(int nbLines, int M, int rank, double *h) {
    int i, j;
    for (i = 0; i < nbLines; i++) {
        for (j = 0; j < M; j++) {
            h[(i * M) + j] = 0;
        }
    }
    if (rank == 0) {
        for (j = (M * 0.1); j < (M * 0.9); j++) {
            h[j] = 100;
        }
    }
}

double doWork(int numprocs, int rank, int M, int nbLines, double *g,
              double *h) {
    int i, j;
    MPI_Request req1[2], req2[2];
    MPI_Status status1[2], status2[2];
    double localerror;
    localerror = 0;
    for (i = 0; i < nbLines; i++) {
        for (j = 0; j < M; j++) {
            h[(i * M) + j] = g[(i * M) + j];
        }
    }
    if (rank > 0) {
        MPI_Isend(g + M, M, MPI_DOUBLE, rank - 1, WORKTAG, MPI_COMM_WORLD,
                  &req1[0]);
        MPI_Irecv(h, M, MPI_DOUBLE, rank - 1, WORKTAG, MPI_COMM_WORLD,
                  &req1[1]);
    }
    if (rank < numprocs - 1) {
        MPI_Isend(g + ((nbLines - 2) * M), M, MPI_DOUBLE, rank + 1, WORKTAG,
                  MPI_COMM_WORLD, &req2[0]);
        MPI_Irecv(h + ((nbLines - 1) * M), M, MPI_DOUBLE, rank + 1, WORKTAG,
                  MPI_COMM_WORLD, &req2[1]);
    }
    if (rank > 0) {
        MPI_Waitall(2, req1, status1);
    }
    if (rank < numprocs - 1) {
        MPI_Waitall(2, req2, status2);
    }
    for (i = 1; i < (nbLines - 1); i++) {
        for (j = 0; j < M; j++) {
            g[(i * M) + j] =
                0.25 * (h[((i - 1) * M) + j] + h[((i + 1) * M) + j] +
                        h[(i * M) + j - 1] + h[(i * M) + j + 1]);
            if (localerror < fabs(g[(i * M) + j] - h[(i * M) + j])) {
                localerror = fabs(g[(i * M) + j] - h[(i * M) + j]);
            }
        }
    }
    if (rank == (numprocs - 1)) {
        for (j = 0; j < M; j++) {
            g[((nbLines - 1) * M) + j] = g[((nbLines - 2) * M) + j];
        }
    }
    return localerror;
}

int main(int argc, char *argv[]) {
    int rank, nbProcs, nbLines, i, M, arg;
    double wtime, *h, *g, memSize, localerror, globalerror = 1;

    if (argc < 2) {
        printf("Usage: %s <mem_in_mb>\n", argv[0]);
        exit(1);
    }

    const char prog_name[AUL_NAME_LEN];
    memset(prog_name, 0, sizeof(prog_name));
    strcpy(prog_name, "heatdis");

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nbProcs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (sscanf(argv[1], "%d", &arg) != 1) {
        printf("Wrong memory size! See usage\n");
        exit(3);
    }

    aul_configuration_t aul_conf = AUL_CONFIG_DEFAULT;
    aul_conf.rank = rank;
    aul_conf.opt_group_id = 0;
    aul_conf.opt_group_size = nbProcs;

    if (AUL_Init(&aul_conf) != 0) {
        printf("Error initializing AURORA! Aborting...\n");
        exit(2);
    }

    M = (int) sqrt((double) (arg * 1024.0 * 1024.0 * nbProcs) /
                   (2 * sizeof(double))); // two matrices needed
    nbLines = (M / nbProcs) + 3;
    h = (double *) malloc(sizeof(double *) * M * nbLines);
    g = (double *) malloc(sizeof(double *) * M * nbLines);
    initData(nbLines, M, rank, g);
    memSize = M * nbLines * 2 * sizeof(double) / (1024 * 1024);

    if (rank == 0)
        printf("Local data size is %d x %d = %f MB (%d).\n", M, nbLines,
               memSize, arg);
    if (rank == 0)
        printf("Target precision : %f \n", PRECISION);
    if (rank == 0)
        printf("Maximum number of iterations : %d \n", ITER_TIMES);

    AUL_Mem_protect(0, &i, 1 * sizeof(int));
    AUL_Mem_protect(1, h, M * nbLines * sizeof(double));
    AUL_Mem_protect(2, g, M * nbLines * sizeof(double));

    wtime = MPI_Wtime();
    // Use -1 for latest version found
    int v = AUL_Test(400, prog_name);
    MPI_Barrier(MPI_COMM_WORLD);
    if (v > 0) {
        printf("Previous checkpoint found at iteration %d, initiating "
               "restart...\n",
               v);
        MPI_Barrier(MPI_COMM_WORLD);
        // v can be any version, independent of what VELOC_Restart_test is
        // returning
        int v_restored = AUL_Restart(v, prog_name);
        if (v_restored != v) {
            printf("%d Error restarting from checkpoint %d! Aborting...\n",
                   rank, v_restored);
            exit(2);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        printf("Done Restoring.");
    } else
        i = 0;
    while (i < ITER_TIMES) {
        localerror = doWork(nbProcs, rank, M, nbLines, g, h);
        if (((i % ITER_OUT) == 0) && (rank == 0))
            printf("Step : %d, error = %f\n", i, globalerror);
        if ((i % REDUCE) == 0)
            MPI_Allreduce(&localerror, &globalerror, 1, MPI_DOUBLE, MPI_MAX,
                          MPI_COMM_WORLD);
        if (globalerror < PRECISION)
            break;
        i++;
        if (i % CKPT_FREQ == 0)
            if (AUL_Checkpoint(i, prog_name) != 0) {
                printf("Error checkpointing! Aborting...\n");
                exit(2);
            }
    }
    if (rank == 0)
        printf("Execution finished in %lf seconds.\n", MPI_Wtime() - wtime);

    MPI_Barrier(MPI_COMM_WORLD);

    // Waits for checkpoint to finish
    // Deregisters the RMA regions
    AUL_Mem_unprotect(0);
    AUL_Mem_unprotect(1);
    AUL_Mem_unprotect(2);
    free(h);
    free(g);
    AUL_Finalize();
    MPI_Finalize();
    return 0;
}
