/**
 * @file aim.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-26
 * @modified Last Modified: 2026-02-26
 *
 * @copyright Copyright (c) 2026
 */

#define AIM_INTERNAL
#include "aim.h"

#include "log.h"

aim_hndl *aim_init(size_t max_workers) {
    if ((max_workers & (max_workers - 1)) != 0) {
        log_error("Max workers must be a power of 2");
        return NULL;
    }

    aim_hndl *pHndl = malloc(sizeof(aim_hndl));
    if (!pHndl) {
        log_error("Failed to create AIM handle");
        return NULL;
    }

    // Worker storage area allocation
    pHndl->max_workers = max_workers;
    atomic_init(&pHndl->n_workers, 0);
    pHndl->workers = malloc(sizeof(aim_entry_t) * max_workers);
    if (!pHndl->workers) {
        log_error("Failed to allocate AIM worker storage area");
        (void) aim_finalize(&pHndl);
        return NULL;
    }

    // Worker Queue creation
    pHndl->queue.max_worker_ids = max_workers;
    pHndl->queue.worker_ids = malloc(sizeof(size_t) * max_workers);
    pHndl->queue.sequences = malloc(sizeof(atomic_size_t) * max_workers);
    pHndl->queue.mask = (max_workers - 1);
    if (!pHndl->queue.worker_ids) {
        log_error("Failed to allocate AIM worker queue");
        (void) aim_finalize(&pHndl);
        return NULL;
    }

    atomic_init(&pHndl->queue.dequeue_pos, 0);
    atomic_init(&pHndl->queue.enqueue_pos, 0);

    for (size_t i = 0; i < max_workers; i++) {
        atomic_init(&pHndl->queue.sequences[i], i);
        pHndl->workers[i].instance_index = i;
        atomic_init(&pHndl->workers[i].references, 0);
        pHndl->workers[i].pACI = NULL;
        pHndl->workers[i].pACN = NULL;
    }

    log_trace("AIM initialized %d max workers", max_workers);

    return pHndl;
}

int aim_finalize(aim_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }
    if ((*ppHndl)->workers) {
        free((*ppHndl)->workers);
    }
    if ((*ppHndl)->queue.worker_ids) {
        free((*ppHndl)->queue.worker_ids);
    }
    if ((*ppHndl)->queue.sequences) {
        free((*ppHndl)->queue.sequences);
    }
    free(*ppHndl);
    *ppHndl = NULL;
    log_trace("AIM instance destroyed");
    return 0;
}

aim_entry_t *aim_add_entry(aim_hndl *pHndl) {
    if (!pHndl) {
        log_error("Cannot add entry to NULL handler");
        return NULL;
    }
    size_t n_workers = atomic_fetch_add(&pHndl->n_workers, 1);
    log_trace("Attempting to add AIM entry.. %ld in existence", n_workers);
    if (n_workers == pHndl->max_workers) {
        // pHndl->n_workers--;
        log_error("AIM worker max capacity reached.");
        return NULL;
    }
    for (size_t i = 0; i < pHndl->max_workers; i++) {
        size_t expected = 0;
        if (atomic_compare_exchange_strong(&pHndl->workers[i].references,
                                           &expected, 1)) {
            return &pHndl->workers[i];
        }
    }
    return NULL;
}

int aim_remove_entry(aim_hndl *pHndl, aim_entry_t *pEntry) {
    if (!pHndl || !pEntry) {
        log_error("Remove entry called with NULL pointers");
        return -1;
    }
    pEntry->pACI = NULL;
    pEntry->pACN = NULL;
    atomic_store(&pEntry->references, 0);
    pHndl->n_workers--;
    return 0;
}

int aim_enqueue(aim_hndl *pHndl, aim_entry_t *pEntry) {
    if (!pHndl || !pEntry) {
        log_error("Enqueue called with NULL pointers");
        return -1;
    }
    size_t pos =
        atomic_load_explicit(&pHndl->queue.enqueue_pos, memory_order_relaxed);
    size_t idx = pEntry->instance_index;

    for (;;) {
        size_t seq = atomic_load_explicit(
            &pHndl->queue.sequences[pos & pHndl->queue.mask],
            memory_order_acquire);
        intptr_t diff = (intptr_t) seq - (intptr_t) pos;

        if (diff == 0) { // Slot is ready for a write
            if (atomic_compare_exchange_weak_explicit(&pHndl->queue.enqueue_pos,
                                                      &pos, pos + 1,
                                                      memory_order_relaxed,
                                                      memory_order_relaxed)) {
                break;
            }
        } else {
            pos = atomic_load_explicit(&pHndl->queue.enqueue_pos,
                                       memory_order_relaxed);
        }
    }

    // Write the index to the queue buffer
    pHndl->queue.worker_ids[pos & pHndl->queue.mask] = idx;

    // Commit: Signal that this slot is now ready to be popped
    atomic_store_explicit(&pHndl->queue.sequences[pos & pHndl->queue.mask],
                          pos + 1, memory_order_release);
    return 0;
}

aim_entry_t *aim_dequeue(aim_hndl *pHndl) {
    if (!pHndl) {
        log_error("Dequeue called with NULL pointers");
        return NULL;
    }
    size_t pos =
        atomic_load_explicit(&pHndl->queue.dequeue_pos, memory_order_relaxed);
    size_t worker_idx;

    for (;;) {
        size_t seq = atomic_load_explicit(
            &pHndl->queue.sequences[pos & pHndl->queue.mask],
            memory_order_acquire);
        intptr_t diff = (intptr_t) seq - (intptr_t) (pos + 1);

        if (diff == 0) { // Slot has data ready to be read
            if (atomic_compare_exchange_weak_explicit(&pHndl->queue.dequeue_pos,
                                                      &pos, pos + 1,
                                                      memory_order_relaxed,
                                                      memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return NULL; // Queue is empty
        } else {
            pos = atomic_load_explicit(&pHndl->queue.dequeue_pos,
                                       memory_order_relaxed);
        }
    }

    worker_idx = pHndl->queue.worker_ids[pos & pHndl->queue.mask];

    // Release the slot: Mark it as ready to be reused for the NEXT wrap-around
    atomic_store_explicit(&pHndl->queue.sequences[pos & pHndl->queue.mask],
                          pos + pHndl->queue.mask + 1, memory_order_release);

    return &pHndl->workers[worker_idx];
}
