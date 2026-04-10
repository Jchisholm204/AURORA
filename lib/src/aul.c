/**
 * @file aul.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#define AUL_INTERNAL

#include "aul.h"

#include "aul_internal.h"

// Compile time error check name lengths
#if AUL_NAME_LEN != ACN_NAME_LEN
#error "AUL Name Length Must match ACN name length"
#endif

struct aurora_user_library_context _aul_ctx = {
    .log_file = NULL,
    .pACI = NULL,
    .pACN = NULL,
    .pARM = NULL,
    .pAFV = NULL,
    .pConfig = NULL,
};
