/**
 * @file miniVite-checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Claude-Haiku-5.5 OpenCode Go
 * @brief Build-time selectable miniVite checkpoint interface
 * @version 0.1
 * @date Created: 2026-11-11
 * @modified Last Modified: 2026-11-11
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _MINIVITE_CHECKPOINT_H_
#define _MINIVITE_CHECKPOINT_H_

#define MINIVITE_CKPT_NAME "miniVite"

#if defined(MINIVITE_CKPT_AURORA)
#include "miniVite-checkpoint-aurora.h"

#define MINIVITE_CKPT_INIT(path, host) miniVite_ckpt_init(path, host)
#define MINIVITE_CKPT_RESTART_TEST() miniVite_ckpt_restart_test()
#define MINIVITE_CKPT_RESTART(version) miniVite_ckpt_restart(version)
#define MINIVITE_CKPT_CHECKPOINT(version) miniVite_ckpt_checkpoint(version)
#define MINIVITE_CKPT_FINALIZE() miniVite_ckpt_finalize()
#elif defined(MINIVITE_CKPT_VELOC)
#include "miniVite-checkpoint-veloc.h"

#define MINIVITE_CKPT_INIT(path, host) miniVite_ckpt_init(path, host)
#define MINIVITE_CKPT_RESTART_TEST() miniVite_ckpt_restart_test()
#define MINIVITE_CKPT_RESTART(version) miniVite_ckpt_restart(version)
#define MINIVITE_CKPT_CHECKPOINT(version) miniVite_ckpt_checkpoint(version)
#define MINIVITE_CKPT_FINALIZE() miniVite_ckpt_finalize()
#else
#define MINIVITE_CKPT_INIT(path, host) ((void) 0)
#define MINIVITE_CKPT_PROTECT_REGION(id, ptr, count, element_size) ((void) 0)
#define MINIVITE_CKPT_RESTART_TEST() (0)
#define MINIVITE_CKPT_RESTART(version) (0)
#define MINIVITE_CKPT_CHECKPOINT(version) (0)
#define MINIVITE_CKPT_FINALIZE() ((void) 0)
#endif

#endif
