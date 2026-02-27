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

struct acl_connection_context {
    aim_hndl *pAIM;
    int conn_socket;
};

void *_acl_connection_listener(void *arg);
void *_acl_connection_accept(void *arg);

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

    // Setup ADS
    pHndl->pADS = ads_init();

    if(!pHndl->pADS){
        log_fatal("ADS instance creation Failed");
        free(pHndl);
        return NULL;
    }

    // Setup Listener Thread
    pHndl->running = true;
    atomic_init(&pHndl->n_worker_threads, 0);

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);

    int pthread_status = pthread_create(&pHndl->thread_manager, &thread_attr,
                                        _acl_connection_listener, pHndl);
    if (pthread_status != 0) {
        log_error("Failed to create main worker thread");
    }

    return pHndl;
}

int acl_finalize(acl_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }
    (*ppHndl)->running = false;
    pthread_join((*ppHndl)->thread_manager, NULL);
    free(*ppHndl);
    *ppHndl = NULL;
    return 0;
}

void *_acl_connection_listener(void *arg) {
    acl_hndl *pHndl = (acl_hndl *) arg;
    if (!pHndl) {
        log_fatal("Connection Listener got NULL input!?!");
        return NULL;
    }

    log_trace("Connection Listener Listening");

    while (pHndl->running) {
        struct acl_connection_context *pCctx =
            malloc(sizeof(struct acl_connection_context));
        if (!pCctx) {
            log_warn("ACL failed to runtime allocation for internal structure");
            continue;
        }
        pCctx->pAIM = pHndl->pAIM;
        pCctx->conn_socket = ads_accept_any(pHndl->pADS);
        if (pCctx->conn_socket >= 0) {
            pthread_attr_t thread_attr;
            pthread_attr_init(&thread_attr);
            pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

            int pthread_status =
                pthread_create(&pHndl->thread_manager, &thread_attr,
                               _acl_connection_accept, pCctx);
            if (pthread_status != 0) {
                log_error("Failed to create connection acceptance thread: %d",
                          pthread_status);
            } else {
                // Spawned Connection Accept Handler Successfully
                continue;
            }
        }
        // Error Case.. Only called if Connection Handler was not spawned
        log_warn("ACL had a connection failure event: %d", pCctx->conn_socket);
        free(pCctx);
    }
    return NULL;
}

void *_acl_connection_accept(void *arg) {
    struct acl_connection_context *pCtx = arg;
    if(!pCtx){
        return NULL;
    }

    log_trace("Attempting to accept new connection");

    ads_exchange_data_t ads_data = {
        .comm = {"HelloWorld", 0},
        .notif = {"Yeetskeet", 0},
    };
    ads_exchange(pCtx->conn_socket, &ads_data);

    free(pCtx);

    log_trace("New connection accepted.");

    return NULL;
}
