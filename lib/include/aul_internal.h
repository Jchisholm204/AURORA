/**
 * @file aul_internal.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */
#ifndef _AUL_INTERNAL_H_
#define _AUL_INTERNAL_H_
#ifdef AUL_INTERNAL

#include "aci/aci.h"
#include "acn/acn.h"
#include "afv/afv.h"
#include "arm/arm.h"
#include "aul.h"
#include "aul_configuration.h"
#include "operating_configuration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct aurora_user_library_context {
    FILE *log_file;
    aci_hndl *pACI;
    acn_hndl *pACN;
    arm_hndl *pARM;
    afv_hndl *pAFV;
    struct aurora_operating_configuration *pConfig;
};

extern struct aurora_user_library_context _aul_ctx;

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
             log_info("[TIMER] %-20s : %.6f ms", name, ms);                    \
             _t.done = 1;                                                      \
         }))

#endif
#endif
