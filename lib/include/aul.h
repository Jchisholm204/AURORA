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

extern int AUL_Test(const int version, const char *name);

// Compatibility Binding for AURORA<v0.0.3, VeloC Codebases
#ifndef AUL_DEFAULT_LOCAL
#define AUL_Restart(version, name) AUL_Restart_remote(version, name)
#else
#define AUL_Restart(version, name) AUL_Restart_local(version, name)
#endif

extern int AUL_Restart_remote(const int version,
                              const char name[static AUL_NAME_LEN]);
extern int AUL_Restart_local(const int version,
                             const char name[static AUL_NAME_LEN]);

#endif
