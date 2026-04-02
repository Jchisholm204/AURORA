/**
 * @file acr.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Command Runner
 * @version 0.1
 * @date Created: 2026-03-12
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#include "aim.h"

#ifndef _ACR_ACR_H_
#define _ACR_ACR_H_

#ifdef ACR_INTERNAL
#include <pthread.h>
#include <stdatomic.h>
#include <threads.h>
struct aurora_command_ctx {
    aim_hndl *pAIM;
    aim_entry_t *pInstance;
    int flags;
    // Not For Thread Access
    struct {
        pthread_t thread_manager;
        uint64_t next;
        struct aurora_command_runner_hndl *pHndl;
    } __restricted;
};
#endif

struct aurora_command_runner_hndl
#ifdef ACR_INTERNAL
{
    atomic_int refcount;
    _Atomic(uint64_t) thread_ctx_head_idx;
    aim_hndl *pAIM;
    struct aurora_command_ctx *thread_contexts;
    size_t max_workers;
}
#endif
;

enum aurora_command_runner_error_e {
    eACR_OK,
    eACR_ERR_NULL,
    eACR_ERR_NO_WORKERS,
    eACR_ERR_THREAD,
    eACR_N_ERR,
};

typedef void *(*ACR_cmd_fn)(void *);
typedef struct aurora_command_runner_hndl acr_hndl;
typedef enum aurora_command_runner_error_e eACR_error;

extern acr_hndl *acr_init(aim_hndl *pAIM, size_t max_workers);

extern eACR_error acr_finalize(acr_hndl **ppHndl);

extern eACR_error acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, int flags,
                          ACR_cmd_fn cmd_function);

// Always ONLY invoke these using the ACR Runner
extern void *acr_cmd_nop(void *);
extern void *acr_cmd_checkpoint(void *);
extern void *acr_cmd_restart(void *);
extern void *acr_cmd_connection_up(void *);
extern void *acr_cmd_connection_down(void *);

#ifdef ACR_INTERNAL

extern eACR_error _acr_ctx_release(struct aurora_command_ctx *pCtx);
extern eACR_error _acr_ctx_release_retry(struct aurora_command_ctx *pCtx, int count);

#endif

#endif
