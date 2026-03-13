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
#include "acr.h"

#include "acn/acn.h"
#include "aim.h"
#include "log.h"

#include <stdatomic.h>

// ACR Threads
void *_acr_nop(void *);
void *_acr_checkpoint(void *);
void *_acr_restore(void *);
void *_acr_shutdowndisconnect(void *);

acr_hndl *acr_init(aim_hndl *pAIM) {
    if (!pAIM) {
        log_warn("Cannot Create Instance, AIM was NULL");
    }
    acr_hndl *pHndl = malloc(sizeof(acr_hndl));
    if (!pHndl) {
        log_error("Could not allocate memory for ACR handle");
        return NULL;
    }
    pHndl->pAIM = pAIM;
    pHndl->refcount = 0;
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

int acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, eACR_cmd ecmd) {
    if (!pHndl) {
        log_warn("ACR cannot run anything with a NULL handle");
        return -1;
    }
    void *(*_acr_cmd)(void *) = _acr_nop;
    switch (ecmd) {
    case eACR_nop:
        _acr_cmd = _acr_nop;
        break;
    case eACR_checkpoint:
        _acr_cmd = _acr_checkpoint;
        break;
    case eACR_restore:
        _acr_cmd = _acr_restore;
        break;
    case eACR_shutdowndisconnect:
    case eACR_N:
        _acr_cmd = _acr_shutdowndisconnect;
    }

    int refcount = atomic_fetch_add(&pHndl->refcount, 1);

    if (refcount == MAX_WORKERS) {
        pHndl->refcount--;
        log_debug("Cannot run command.. Max workers already running");
        return -3;
    }

    struct aurora_command_ctx *pCCtx = &pHndl->thread_contexts[refcount];
    // Always just ensure this matches..
    pCCtx->pAIM = pHndl->pAIM;

    // MUST check, set and reset this every time
    if (pCCtx->pInstance) {
        if (aim_enqueue(pHndl->pAIM, pCCtx->pInstance)) {
            log_fatal("The server made an avoidable mistake");
        }
    }
    pCCtx->pInstance = pInstance;

    if (!_acr_cmd) {
        log_fatal("You messed up bigtime..");
    }

    // Todo: Make this multi threaded
    (void) _acr_cmd(pCCtx);

    // Run this when we know the command has completed.
    pHndl->refcount--;

    return 0;
}

void *_acr_nop(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // Do the command stuff

    // NOP

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}

void *_acr_checkpoint(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // Do the command stuff

    // NOP

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}

void *_acr_restore(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    // Do the command stuff

    // NOP

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}

void *_acr_shutdowndisconnect(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    log_trace("ACR is intentionally destroying an instance");

    // Run the command stuffs
    acn_destroy_instance(&pCtx->pInstance->pACN);
    aci_disconnect_instance(pCtx->pInstance->pACI);
    aci_destroy_instance(&pCtx->pInstance->pACI);

    // Deal with AIM
    if (aim_remove_entry(pCtx->pAIM, pCtx->pInstance)) {
        log_error("Failed to remove AIM entry");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    return NULL;
}
