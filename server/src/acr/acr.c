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
#include <stdatomic.h>
#include <unistd.h>

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

int acr_run(acr_hndl *pHndl, aim_entry_t *pInstance, ACR_cmd_fn cmd_function) {
    if (!pHndl) {
        log_warn("ACR cannot run anything with a NULL handle");
        return -1;
    }
    if (!cmd_function) {
        log_error("NULL Parameter");
        cmd_function = acr_cmd_nop;
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

    // Todo: Make this multi threaded
    (void) cmd_function(pCCtx);

    // Run this when we know the command has completed.
    pHndl->refcount--;

    return 0;
}

void *_acr_shutdowndisconnect(void *arg) {
    if (!arg) {
        log_fatal("Command Argument was NULL");
        return NULL;
    }
    struct aurora_command_ctx *pCtx = arg;

    log_trace("ACR is intentionally destroying an instance");

    // Run the command stuffs
    arm_destroy_instance(&pCtx->pInstance->pARM);
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
