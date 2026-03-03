/**
 * @file acn_tick.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-03-03
 * @modified Last Modified: 2026-03-03
 *
 * @copyright Copyright (c) 2026
 */

#define ACN_INTERNAL
#include "acn/acn.h"
#include "log.h"

int acn_await_tick(acn_hndl *pHndl) {
    if (!pHndl) {
        log_error("Tick called with NULL handle");
        return -1;
    }
    while (pHndl->pLocal->tick < pHndl->local_private.tick) {
    }

    pHndl->local_private.tick = pHndl->pLocal->tick;

    return 0;
}

int acn_await_checkpoint(acn_hndl *pHndl) {
}

int acn_await_version(acn_hndl *pHndl, int64_t version) {
}

int acn_await_memory(acn_hndl *pHndl) {
}

int acn_tick_remote(acn_hndl *pHndl) {
}

int acn_tick_checkpoint(acn_hndl *pHndl) {
}

int acn_tick_memory(acn_hndl *pHndl) {
}
