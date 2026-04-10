/**
 * @file aul.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA User Library
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AURORA_USER_LIB_H_
#define _AURORA_USER_LIB_H_

#include "aul_configuration.h"

#include <stddef.h>
#include <stdint.h>

#ifndef AUL_NAME_LEN
#define AUL_NAME_LEN 16
#endif

extern int AUL_Init(const aul_configuration_t *pCFG);

extern int AUL_Finalize(void);

extern int AUL_Mem_protect(const uint64_t mem_id, const void *const ptr,
                           const size_t size);
extern int AUL_Mem_unprotect(const uint64_t mem_id);

extern int AUL_Checkpoint(const int version,
                          const char name[static AUL_NAME_LEN]);

extern int AUL_Test(const int version, const char name[static AUL_NAME_LEN]);

extern int AUL_Restart(const int version, const char name[static AUL_NAME_LEN]);

#endif
