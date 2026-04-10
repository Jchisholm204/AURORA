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
    pHndl->max_workers = max_workers;

    // Setup Thread Pool
    atomic_init(&pHndl->thread_ctx_head_idx, 0);
    atomic_init(&pHndl->refcount, 0);
    pHndl->thread_contexts =
        malloc(sizeof(struct aurora_command_ctx) * max_workers);
    for (size_t i = 0; i < max_workers; i++) {
        pHndl->thread_contexts[i].__restricted.next = i + 1;
        pHndl->thread_contexts[i].__restricted.pHndl = pHndl;
        pHndl->thread_contexts[i].pInstance = NULL;
        pHndl->thread_contexts[i].pAIM = pHndl->pAIM;
    }
    pHndl->thread_contexts[max_workers - 1].__restricted.next = 0xFFFFFFFF;
    pHndl->thread_contexts[max_workers - 1].__restricted.pHndl = NULL;

    return pHndl;
}

eACR_error acr_finalize(acr_hndl **ppHndl) {
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

    if ((*ppHndl)->thread_contexts) {
        free((*ppHndl)->thread_contexts);
        (*ppHndl)->thread_contexts = NULL;
    }
    free(*ppHndl);
    *ppHndl = NULL;

    return 0;
}

eACR_error acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, int flags,
                   ACR_cmd_fn cmd_function) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eACR_ERR_NULL;
    }
    if (!cmd_function) {
        log_error("NULL Parameter");
        return eACR_ERR_NULL;
    }

    // Early end for NOPs
    if (cmd_function == acr_cmd_nop) {
        struct aurora_command_ctx nop_ctx = {0};
        nop_ctx.pInstance = pInstance;
        nop_ctx.flags = -1;
        nop_ctx.pAIM = pHndl->pAIM;
        // log_debug("NOP flags=%d instance=0x%lx", flags, pInstance);
        (void) acr_cmd_nop(&nop_ctx);
        return eACR_OK;
    }

    size_t refcount = atomic_fetch_add(&pHndl->refcount, 1);

    if (refcount == pHndl->max_workers) {
        pHndl->refcount--; // Atomic
        log_warn("Max Workers Reached.");
        return eACR_ERR_NO_WORKERS;
    }

    struct aurora_command_ctx *pCCtx = NULL;
    uint64_t head_val = atomic_load(&pHndl->thread_ctx_head_idx);
    while (true) {
        uint32_t idx = head_val & 0xFFFFFFFF;
        if (idx >= pHndl->max_workers) {
            log_warn("Max Workers Reached.");
            return eACR_ERR_NO_WORKERS;
        }

        pCCtx = &pHndl->thread_contexts[idx];
        uint32_t next_idx = pCCtx->__restricted.next;
        uint32_t new_tag = (uint32_t) (head_val >> 32) + 1;
        uint64_t new_head = ((uint64_t) new_tag << 32) | next_idx;

        if (atomic_compare_exchange_strong(&pHndl->thread_ctx_head_idx,
                                           &head_val, new_head)) {
            pCCtx->__restricted.next = idx;
            break;
        }
    }

    // Always just ensure this matches..
    pCCtx->pAIM = pHndl->pAIM;

    // MUST check, set and reset this every time
    if (pCCtx->pInstance) {
        log_fatal("The server made an avoidable mistake");
        aim_enqueue(pHndl->pAIM, pCCtx->pInstance);
    }

    pCCtx->pInstance = pInstance;
    pCCtx->flags = flags;
    pCCtx->__restricted.pHndl = pHndl;

    // Todo: Make this multi threaded
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    int pthread_status = pthread_create(&pCCtx->__restricted.thread_manager,
                                        &thread_attr, cmd_function, pCCtx);

    if (pthread_status != 0) {
        log_error("Failed to start CMD=%d", pthread_status);
        (void) _acr_ctx_release_retry(pCCtx, 2);
        return eACR_ERR_THREAD;
    }

    return eACR_OK;
}

eACR_error _acr_ctx_release(struct aurora_command_ctx *pCtx) {

    if (!pCtx) {
        log_warn("NULL Parameter");
        return eACR_ERR_NULL;
    }

    // If the handle is null, this context was not part of the pool
    if (!pCtx->__restricted.pHndl) {
        // log_trace("Released NULL");
        return eACR_OK;
    }
    uint64_t current_head =
        atomic_load(&pCtx->__restricted.pHndl->thread_ctx_head_idx);
    // Placed here by acr_run
    uint64_t this_idx = pCtx->__restricted.next;
    while (true) {

        pCtx->__restricted.next = (uint32_t) (current_head & 0xFFFFFFFF);

        uint32_t new_tag = (uint32_t) (current_head >> 32) + 1;
        uint64_t new_head = ((uint64_t) new_tag << 32) | this_idx;

        if (atomic_compare_exchange_strong(
                &pCtx->__restricted.pHndl->thread_ctx_head_idx, &current_head,
                new_head)) {
            break;
        }
    }

    // 5. Decrement refcount after the object is safely back in the pool
    atomic_fetch_sub(&pCtx->__restricted.pHndl->refcount, 1);
    return eACR_OK;
}

eACR_error _acr_ctx_release_retry(struct aurora_command_ctx *pCtx, int count) {
    eACR_error acr_status = 0;
    int attempts = 0;
    do {
        attempts++;
        acr_status = _acr_ctx_release(pCtx);
    } while (acr_status != eACR_OK && attempts < count);
    if (acr_status != 0) {
        log_error("Failed to release after %d / %d attempts", attempts, count);
    }
    // else {
    //     log_trace("Released after %d / %d attempts", attempts, count);
    // }
    return acr_status;
}
