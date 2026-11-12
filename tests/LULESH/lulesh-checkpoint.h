/**
 * @file lulesh-checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief Build-time selectable LULESH checkpoint interface
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _LULESH_CHECKPOINT_H_
#define _LULESH_CHECKPOINT_H_

#include "lulesh.h"

#define LULESH_CKPT_NAME "LULESH"

#if defined(LULESH_CKPT_AURORA)
#include "lulesh-checkpoint-aurora.h"

#define LULESH_CKPT_INIT(opts) lulesh_ckpt_init(opts)
#define LULESH_CKPT_RESTART_TEST() lulesh_ckpt_restart_test()
#define LULESH_CKPT_RESTART(version) lulesh_ckpt_restart(version)
#define LULESH_CKPT_CHECKPOINT(version) lulesh_ckpt_checkpoint(version)
#define LULESH_CKPT_FINALIZE() lulesh_ckpt_finalize()
#elif defined(LULESH_CKPT_VELOC)
#include "lulesh-checkpoint-veloc.h"

#define LULESH_CKPT_INIT(opts) lulesh_ckpt_init(opts)
#define LULESH_CKPT_RESTART_TEST() lulesh_ckpt_restart_test()
#define LULESH_CKPT_RESTART(version) lulesh_ckpt_restart(version)
#define LULESH_CKPT_CHECKPOINT(version) lulesh_ckpt_checkpoint(version)
#define LULESH_CKPT_FINALIZE() lulesh_ckpt_finalize()
#else
#define LULESH_CKPT_INIT(opts) ((void) 0)
#define LULESH_CKPT_PROTECT_REGION(id, ptr, count, element_size) ((void) 0)
#define LULESH_CKPT_RESTART_TEST() (-1)
#define LULESH_CKPT_RESTART(version) (0)
#define LULESH_CKPT_CHECKPOINT(version) (0)
#define LULESH_CKPT_FINALIZE() ((void) 0)
#endif

#endif
