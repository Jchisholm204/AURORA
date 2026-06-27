#ifndef _HEATDIS_H
#define _HEATDIS_H

#include <math.h>
// #include <openmpi-x86_64/mpi.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PRECISION 0.00001
#define ITER_TIMES 600
#define ITER_OUT 50
#define WORKTAG 5
#define REDUCE 1

#ifdef __cplusplus
extern "C" {
#endif

void initData(int nbLines, int M, int rank, double *h);
double doWork(int numprocs, int rank, int M, int nbLines, double *g, double *h);

#define TIME_REGION(name)                                                      \
    for (struct {                                                              \
             struct timespec start;                                            \
             int done;                                                         \
         } _t = {{0}, 0};                                                      \
         !_t.done && (clock_gettime(CLOCK_MONOTONIC, &_t.start), 1);           \
         __extension__({                                                       \
             struct timespec end;                                              \
             clock_gettime(CLOCK_MONOTONIC, &end);                             \
             double ms =                                                       \
                 (double) (end.tv_sec - _t.start.tv_sec) * 1000.0 +            \
                 (double) (end.tv_nsec - _t.start.tv_nsec) / 1000000.0;        \
             printf("[TIMER_APP] %-20s : %.6f ms", name, ms);                  \
             _t.done = 1;                                                      \
         }))

#ifdef __cplusplus
}
#endif

#endif /* ----- #ifndef _HEATDIS_H  ----- */
