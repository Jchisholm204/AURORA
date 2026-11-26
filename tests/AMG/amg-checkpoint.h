/**
 * @file amg-checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author GLM-5.3-Flash-Free OpenCode Free
 * @brief Build-time selectable AMG checkpoint interface
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AMG_CHECKPOINT_H_
#define _AMG_CHECKPOINT_H_

#define AMG_CKPT_NAME "AMG"

#if defined(AMG_CKPT_AURORA)
#include "amg-checkpoint-aurora.h"

#define AMG_CKPT_INIT(ckpt_path, ckpt_host) amg_ckpt_init(ckpt_path, ckpt_host)
#define AMG_CKPT_PROTECT_REGION(id, ptr, count, element_size)              \
    amg_ckpt_protect_region((uint64_t) (id), (ptr), (uint64_t) (count),    \
                            (size_t) (element_size))
#define AMG_CKPT_RESTART_TEST() amg_ckpt_restart_test()
#define AMG_CKPT_RESTART(version) amg_ckpt_restart(version)
#define AMG_CKPT_CHECKPOINT(version) amg_ckpt_checkpoint(version)
#define AMG_CKPT_FINALIZE() amg_ckpt_finalize()
#elif defined(AMG_CKPT_VELOC)
#include "amg-checkpoint-veloc.h"

#define AMG_CKPT_INIT(ckpt_path, ckpt_host) amg_ckpt_init(ckpt_path, ckpt_host)
#define AMG_CKPT_PROTECT_REGION(id, ptr, count, element_size)              \
    amg_ckpt_protect_region((uint64_t) (id), (ptr), (uint64_t) (count),    \
                            (size_t) (element_size))
#define AMG_CKPT_RESTART_TEST() amg_ckpt_restart_test()
#define AMG_CKPT_RESTART(version) amg_ckpt_restart(version)
#define AMG_CKPT_CHECKPOINT(version) amg_ckpt_checkpoint(version)
#define AMG_CKPT_FINALIZE() amg_ckpt_finalize()
#else
#define AMG_CKPT_INIT(ckpt_path, ckpt_host) ((void) 0)
#define AMG_CKPT_PROTECT_REGION(id, ptr, count, element_size) ((void) 0)
#define AMG_CKPT_RESTART_TEST() (-1)
#define AMG_CKPT_RESTART(version) (0)
#define AMG_CKPT_CHECKPOINT(version) (0)
#define AMG_CKPT_FINALIZE() ((void) 0)
#endif

#endif
