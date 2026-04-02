/**
 * @file acr.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Command Runner
 * @version 0.1
 * @date Created: 2026-03-12
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#define ACR_INTERNAL
#include "acr/acr.h"

#include "acn/acn.h"
#include "afv/afv.h"
#include "aim.h"
#include "log.h"

#include <memory.h>
#include <pthread.h>
#include <stdatomic.h>
#include <threads.h>
#include <unistd.h>

acr_hndl *acr_init(aim_hndl *pAIM, size_t max_workers) {
    if (!pAIM) {
        log_warn("NULL Parameter");
        return NULL;
    }
    if ((max_workers & (max_workers - 1)) != 0) {
        log_error("Max workers must be a power of 2");
        return NULL;
    }
    acr_hndl *pHndl = malloc(sizeof(acr_hndl));
    if (!pHndl) {
        log_error("Could not allocate memory for ACR handle");
        return NULL;
    }
    pHndl->pAIM = pAIM;
    pHndl->refcount = 0;

    // Setup Thread Pool
    atomic_init(&pHndl->thread_ctx_head_idx, 0);
    atomic_init(&pHndl->refcount, 0);
    pHndl->thread_contexts =
        malloc(sizeof(struct aurora_command_ctx) * max_workers);
    for (size_t i = 0; i < max_workers; i++) {
        pHndl->thread_contexts[i].restricted.next = i + 1;
    }

    return pHndl;
}

int acr_finalize(acr_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }
    // Ensure the reference count is 0 before freeing memory
    if ((*ppHndl)->refcount != 0) {
        log_warn("Attempted to free the ACR, but refcount = %d",
                 (*ppHndl)->refcount);
        return -2;
    }

    free(*ppHndl);
    *ppHndl = NULL;

    return 0;
}

int acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, int flags,
            ACR_cmd_fn cmd_function) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return -1;
    }
    if (!cmd_function) {
        log_error("NULL Parameter");
        cmd_function = acr_cmd_nop;
    }

    // Early end for NOPs
    if (cmd_function == acr_cmd_nop) {
        struct aurora_command_ctx nop_ctx;
        nop_ctx.pInstance = pInstance;
        nop_ctx.flags = -1;
        nop_ctx.pAIM = pHndl->pAIM;
        log_debug("NOP flags=%d instance=0x%lx", flags, pInstance);
        (void) acr_cmd_nop(&nop_ctx);
        return 0;
    }

    int refcount = atomic_fetch_add(&pHndl->refcount, 1);

    if (refcount == MAX_WORKERS) {
        pHndl->refcount--; // Atomic
        // log_warn("MAX Workers Reached.. NOPed");
        struct aurora_command_ctx nop_ctx;
        nop_ctx.pInstance = pInstance;
        nop_ctx.flags = -1;
        nop_ctx.pAIM = pHndl->pAIM;
        (void) acr_cmd_nop(&nop_ctx);
        return 0;
    }

    struct aurora_command_ctx *pCCtx = NULL;
    while (true) {
        uint64_t head_idx = atomic_load(&pHndl->thread_ctx_head_idx);

        pCCtx = &pHndl->thread_contexts[head_idx];
        uint64_t next_idx = pCCtx->restricted.next;

        if (atomic_compare_exchange_strong(&pHndl->thread_ctx_head_idx,
                                           &head_idx, next_idx)) {
            pCCtx->restricted.next = head_idx;
            break;
        }
    }

    // Always just ensure this matches..
    pCCtx->pAIM = pHndl->pAIM;

    // MUST check, set and reset this every time
    if (pCCtx->pInstance) {
        if (aim_enqueue(pHndl->pAIM, pCCtx->pInstance)) {
            log_fatal("The server made an avoidable mistake");
        }
    }

    pCCtx->pInstance = pInstance;
    pCCtx->flags = flags;

    // Todo: Make this multi threaded
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    int pthread_status = 0;
    // pthread_create(&pCCtx->restricted.thread_manager,
    //                                 &thread_attr, cmd_function, pCCtx);
    cmd_function(pCCtx);
    if (pthread_status != 0) {
        log_error("Failed to start CMD=%d", pthread_status);
        if (aim_enqueue(pHndl->pAIM, pCCtx->pInstance)) {
            log_fatal("The server made an avoidable mistake");
        }
        pHndl->refcount--; // Atomic
        return 0;
    }

    return 0;
}

int _acr_ctx_release(struct aurora_command_ctx *pCtx) {

    // uint64_t current_head = atomic_load(pCtx->restricted.pThread_ctx_idx);
    // // Placed here by acr_run
    // uint64_t this_idx = pCtx->restricted.next;
    // while (true) {
    //
    //     pCtx->restricted.next = (uint32_t) (current_head & 0xFFFFFFFF);
    //
    //     uint32_t new_tag = (uint32_t) (current_head >> 32) + 1;
    //     uint64_t new_head = ((uint64_t) new_tag << 32) | this_idx;
    //
    //     if (atomic_compare_exchange_strong(pCtx->restricted.pThread_ctx_idx,
    //                                        &current_head, new_head)) {
    //         break;
    //     }
    // }
    //
    // // 5. Decrement refcount after the object is safely back in the pool
    // atomic_fetch_sub(pCtx->restricted.pRefcount, 1);
    // return 0;
}
