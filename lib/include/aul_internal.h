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
#include "arm/arm.h"
#include "aul.h"
#include "aul_configuration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct aurora_user_library_context {
    FILE *log_file;
    aci_hndl *pACI;
    acn_hndl *pACN;
    arm_hndl *pARM;
};

extern struct aurora_user_library_context _aul_ctx;

#endif
#endif
