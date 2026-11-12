/**
 * @file hpccg-checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4 Pro
 * @brief Build-time selectable HPCCG checkpoint interface
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _HPCCG_CHECKPOINT_H_
#define _HPCCG_CHECKPOINT_H_

#define HPCCG_CKPT_NAME "HPCCG"

#if defined(HPCCG_CKPT_AURORA)
#include "hpccg-checkpoint-aurora.h"

#define HPCCG_CKPT_INIT(path, host) hpccg_ckpt_init(path, host)
#define HPCCG_CKPT_RESTART_TEST() hpccg_ckpt_restart_test()
#define HPCCG_CKPT_RESTART(version) hpccg_ckpt_restart(version)
#define HPCCG_CKPT_CHECKPOINT(version) hpccg_ckpt_checkpoint(version)
#define HPCCG_CKPT_FINALIZE() hpccg_ckpt_finalize()
#elif defined(HPCCG_CKPT_VELOC)
#include "hpccg-checkpoint-veloc.h"

#define HPCCG_CKPT_INIT(path, host) hpccg_ckpt_init(path, host)
#define HPCCG_CKPT_RESTART_TEST() hpccg_ckpt_restart_test()
#define HPCCG_CKPT_RESTART(version) hpccg_ckpt_restart(version)
#define HPCCG_CKPT_CHECKPOINT(version) hpccg_ckpt_checkpoint(version)
#define HPCCG_CKPT_FINALIZE() hpccg_ckpt_finalize()
#else
#define HPCCG_CKPT_INIT(path, host) ((void) 0)
#define HPCCG_CKPT_PROTECT_REGION(id, ptr, count, element_size) ((void) 0)
#define HPCCG_CKPT_RESTART_TEST() (-1)
#define HPCCG_CKPT_RESTART(version) (0)
#define HPCCG_CKPT_CHECKPOINT(version) (0)
#define HPCCG_CKPT_FINALIZE() ((void) 0)
#endif

#endif
