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
#include "afv/afv.h"
#include "aim.h"
#include "log.h"

#include <memory.h>
#include <stdatomic.h>
#include <unistd.h>

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
    aim_entry_t *pInstance = pCtx->pInstance;

    int acn_status = 0;
    acn_status = acn_await(pInstance->pACN, eACN_memory);
    if (acn_status != 0) {
        log_error("ACN Error");
    }

    // Setup the checkpoint
    char name[ACN_NAME_LEN];
    int64_t version;
    acn_get(pInstance->pACN, eACN_version, (uint64_t *) &version);
    acn_get_name(pCtx->pInstance->pACN, name);
    size_t arm_n_regions = arm_get_n_remote_regions(pInstance->pARM);
    const amr_hndl *arm_regions = arm_get_remote_regions(pInstance->pARM);
    const afv_metadata_t *pMetadata_old = afv_get_metadata(pInstance->pAFV);

    // Setup Metadata
    afv_metadata_t *pMetadata =
        afv_metadata_ptr_init(malloc(afv_metadata_size(arm_n_regions)));
    if (!pMetadata) {
        log_error("Bad Alloc?");
        if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
            log_fatal("Could Not Enqueue! A thread lost a connection");
        }
        return NULL;
    }

    log_info("Rank %d checkpointing %d rngs to %s", pMetadata_old->rank,
             arm_n_regions, afv_get_filename(pInstance->pAFV, version, name));

    // Complete the checkpoint
    for (size_t i = 0; i < arm_n_regions; i++) {
        log_trace("rgn: %d -> rgnid: %d", i, arm_regions[i].id);
        pMetadata->region_ids[i] = arm_regions[i].id;
        const amr_hndl *pAMR = &arm_regions[i];
        uint64_t test_data[4] = {0x0000, 0x1111, 0x2222, 0x3333};
        eARM_error arm_status =
            arm_read(pInstance->pARM, pAMR, pAMR->pShadow_memory, test_data,
                     pAMR->rgn_size);
        if (arm_status != eARM_OK) {
            log_error("ARM Err %d", arm_status);
        }
        for (int i = 0; i < 4; i++)
            log_trace("TD %d 0x%lx", i, test_data[i]);
    }

    // Finalize the Checkpoint
    pMetadata->rank = pMetadata_old->rank;
    pMetadata->version = version;
    memcpy(pMetadata->chkpt_name, name, ACN_NAME_LEN);

    log_debug("checkpoint: %d %.*s", version, ACN_NAME_LEN, name);

    // NOP
    acn_tick(pInstance->pACN, eACN_checkpoint);

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
    aim_entry_t *pInstance = pCtx->pInstance;

    // Do the command stuff

    usleep(500);

    int64_t restore_version = 0;
    acn_get(pInstance->pACN, eACN_version, (uint64_t *) &restore_version);

    char restore_name[ACN_NAME_LEN] = "NULL";
    acn_get_name(pInstance->pACN, restore_name);

    log_debug("Restore Triggered -> %d: %.*s (%d)", restore_version,
              ACN_NAME_LEN, restore_name, 0);

    size_t inst_n_rgns = arm_get_n_remote_regions(pInstance->pARM);
    log_trace("Remote has %lu regions", inst_n_rgns);

    const amr_hndl *AMR_list = arm_get_remote_regions(pInstance->pARM);

    uint64_t test_data[4] = {0x0000, 0x1111, 0x2222, 0x3333};

    for (size_t i = 0; i < inst_n_rgns; i++) {
        const amr_hndl *pAMR = &AMR_list[i];
        log_trace("RGN: %d: %s", pAMR->id, pAMR->name);
        eARM_error arm_status =
            arm_write(pInstance->pARM, pAMR, pAMR->pActive_memory, test_data,
                      pAMR->rgn_size);
        if (arm_status != eARM_OK) {
            log_error("ARM Err %d", arm_status);
        }
    }

    acn_set(pInstance->pACN, eACN_version, 7);

    log_trace("Set Version");

    acn_tick(pInstance->pACN, eACN_restore);

    log_trace("Ticked Restore");

    // Deal with AIM
    if (aim_enqueue(pCtx->pAIM, pCtx->pInstance) != 0) {
        log_fatal("Could Not Enqueue! A thread lost a connection");
        return NULL;
    }
    // Handler cleanup
    pCtx->pInstance = NULL;
    log_debug("Restore Finished -> %d: %.*s (%d)", restore_version,
              ACN_NAME_LEN, restore_name, 0);
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
