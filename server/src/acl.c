/**
 * @file acl.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Connection Listener
 * @version 0.1
 * @date Created: 2026-02-26
 * @modified Last Modified: 2026-02-26
 *
 * @copyright Copyright (c) 2026
 */

#define ACL_INTERNAL
#include "acl.h"

#include "log.h"

void *_acl_worker(void *arg);

acl_hndl *acl_init(aim_hndl *pAIM) {
    if (!pAIM) {
        log_error("Cannot create ACL without AIM");
        return NULL;
    }
    acl_hndl *pHndl = malloc(sizeof(acl_hndl));
    if (!pHndl) {
        log_error("Failed to create ACL handle");
        return NULL;
    }
    pHndl->running = true;
    atomic_init(&pHndl->n_worker_threads, 0);

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);

    int r = pthread_create(&pHndl->thread_manager, &thread_attr, _acl_worker,
                           pHndl);
    if (r != 0) {
        log_error("Failed to create main worker thread");
    }

    return pHndl;
}

int acl_finialize(acl_hndl **ppHndl) {
}

void *_acl_worker(void *arg) {

    return NULL;
}
