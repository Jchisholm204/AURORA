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

#include "acr/acr.h"
#include "log.h"

#include <unistd.h>

#ifndef ACL_CONNECTION_ATTEMPTS
#define ACL_CONNECTION_ATTEMPTS 400
#endif

struct acl_connection_context {
    aim_hndl *pAIM;
    int conn_socket;
};

void *_acl_connection_listener(void *arg);

acl_hndl *acl_init(aim_hndl *pAIM, acr_hndl *pACR) {
    if (!pAIM) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!pACR) {
        log_error("NULL Parameter");
        return NULL;
    }
    acl_hndl *pHndl = malloc(sizeof(acl_hndl));
    if (!pHndl) {
        log_error("Failed to create ACL handle");
        return NULL;
    }

    // Setup ADS
    pHndl->pADS = ads_init();
    pHndl->pAIM = pAIM;
    pHndl->pACR = pACR;

    if (!pHndl->pADS) {
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
        log_fatal("Failed to create main worker thread");
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
    (void) ads_finalize(&(*ppHndl)->pADS);
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
        int conn_socket = ads_accept_any(pHndl->pADS);
        if (conn_socket >= 0) {

            eACR_error acr_status = eACR_OK;
            int attempts = 0;
            do {
                acr_status = acr_run(pHndl->pACR, NULL, conn_socket,
                                     acr_cmd_connection_up);
                attempts++;
                if (acr_status != eACR_OK &&
                    attempts < ACL_CONNECTION_ATTEMPTS) {
                    usleep(5000);
                    continue;
                } else {
                    break;
                }
            } while (1);

            if (acr_status != eACR_OK) {
                log_error("Dropped Connection: %d / %d attempts", attempts,
                          ACL_CONNECTION_ATTEMPTS);
            }
        } else {
            // Spawned Connection Accept Handler Successfully
            continue;
        }
    }
    return NULL;
}
