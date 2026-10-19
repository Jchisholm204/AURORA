/**
 * @file aul_restart.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AUL_INTERNAL

#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "aul.h"
#include "aul_internal.h"
#include "limits.h"
#include "log.h"

#include <unistd.h>

int AUL_Test(const int version, const char *name) {
    if (!_aul_ctx.pAFV || !_aul_ctx.pARM) {
        log_fatal("Not Initialized");
        return -eAFV_VERIF_ERR_FATAL;
    }

    // Attempt load metadata
    const afv_metadata_t *pMetadata =
        afv_read_metadata(_aul_ctx.pAFV, version, name);
    if (!pMetadata) {
        log_debug("%s-%d Not Loadable", name, version);
        eAFV_verif load_status =
            eAFV_VERIF_ERR_VERSION | eAFV_VERIF_ERR_NAME | eAFV_VERIF_ERR_NULL;
        return -load_status;
    }

    // Verify local state matches metadata
    size_t n_regions = arm_get_n_local_regions(_aul_ctx.pARM);
    const amr_hndl *const region_list = arm_get_local_regions(_aul_ctx.pARM);

    if (pMetadata->n_regions != n_regions) {
        log_debug("metadata_regions (%ld) != (%ld) active regions",
                  pMetadata->n_regions, n_regions);

        // Cleanup
        afv_metadata_destroy((afv_metadata_t **) &pMetadata);
        return -(eAFV_VERIF_ERR_SIZE);
    }

    for (size_t i = 0; i < n_regions; i++) {
        const amr_hndl *const pAMR = &region_list[i];
        eAFV_verif region_status =
            afv_metadata_get_region(pMetadata, pAMR->name, pAMR->id,
                                    pAMR->rgn_size, NULL);
        if (region_status != eAFV_VERIF_OK) {
            log_debug("Region %d id=%d Verification Failure 0x%x", i, pAMR->id,
                      region_status);

            // Cleanup
            afv_metadata_destroy((afv_metadata_t **) &pMetadata);
            return -region_status;
        }
    }

    int64_t vfound = pMetadata->version;
    afv_metadata_destroy((afv_metadata_t **) &pMetadata);
    return vfound;
}

int AUL_Restart_remote(const int version,
                       const char name[static AUL_NAME_LEN]) {
    if (!_aul_ctx.pACI) {
        log_fatal("Not Initialized");
    }
    // Wait for all pending operations to finish
    eACN_error acn_status = eACN_OK;
    do {
        acn_status = acn_await(_aul_ctx.pACN,
                               eACN_systick | eACN_checkpoint | eACN_restore);
    } while (acn_status == eACN_ERR_TIMEOUT);
    if (acn_status != eACN_OK) {
        log_fatal("ACN Error: %d", acn_status);
        return INT_MIN;
    }

    // Setup the version and name we want
    acn_set_name(_aul_ctx.pACN, name);
    acn_set(_aul_ctx.pACN, eACN_version, ((int64_t) version));
    acn_tick(_aul_ctx.pACN, eACN_restore);

    // Wait for restore
    do {
        acn_status = acn_await(_aul_ctx.pACN, eACN_restore);
        usleep(10000);
    } while (acn_status == eACN_ERR_TIMEOUT);

    if (acn_status != eACN_OK) {
        log_fatal("ACN Error: %d", acn_status);
        return INT_MIN;
    }

    // Check the restored version
    int64_t vplaced = 0;
    do {
        acn_status =
            acn_get(_aul_ctx.pACN, eACN_version, (uint64_t *) &vplaced);
    } while (acn_status == eACN_ERR_TIMEOUT);

    if (acn_status != eACN_OK) {
        log_fatal("ACN Error: %d", acn_status);
        return INT_MIN;
    }
    return vplaced;
}

int AUL_Restart_local(const int version, const char name[static AUL_NAME_LEN]) {
    if (!_aul_ctx.pACI || !_aul_ctx.pAFV || !_aul_ctx.pARM) {
        log_fatal("Not Initialized");
        return INT_MIN;
    }

    // All variables in LOCAL_RESTART_FAIL must be declared here
    afv_checkpoint_t *pCheckpoint = NULL;
    size_t *amr_restore_order = NULL;
    int64_t restored_version = INT_MIN;

    pCheckpoint = afv_checkpoint_open(_aul_ctx.pAFV, version, name);
    if (!pCheckpoint) {
        log_debug("%s-%d Not Loadable", name, version);
        return INT_MIN;
    }

    const size_t arm_n_regions = arm_get_n_local_regions(_aul_ctx.pARM);
    // Read only pointer, do not free
    amr_hndl const *const arm_region_list =
        arm_get_local_regions(_aul_ctx.pARM);
    if (arm_n_regions == 0 || arm_region_list == NULL) {
        log_error("Attempted Checkpoint with no active regions");
        goto LOCAL_RESTART_FAIL;
    }

    const ssize_t checkpoint_n_regions = afv_checkpoint_n_regions(pCheckpoint);
    if (checkpoint_n_regions < 0) {
        log_error("Failed to get checkpoint region list 0x%x",
                  -checkpoint_n_regions);
        goto LOCAL_RESTART_FAIL;
    }

    if ((size_t) checkpoint_n_regions != arm_n_regions) {
        log_warn("checkpoint_regions (%ld) != (%ld) active memory regions",
                 checkpoint_n_regions, arm_n_regions);
    }

    amr_restore_order = calloc(arm_n_regions, sizeof(size_t));
    if (!amr_restore_order) {
        log_error("Bad Alloc?");
        goto LOCAL_RESTART_FAIL;
    }
    for (size_t i = 0; i < arm_n_regions; i++) {
        amr_hndl const *const pAMR = &arm_region_list[i];
        size_t index = 0;
        eAFV_verif status =
            afv_checkpoint_find_index(pCheckpoint, pAMR->id, pAMR->rgn_size,
                                      pAMR->name, &index);
        if (status != eAFV_VERIF_OK) {
            log_error("No Such Region: id=%d status=0x%x", pAMR->id, status);
            goto LOCAL_RESTART_FAIL;
        } else {
            log_debug("Region: id=%ld size=%ld order=%d", pAMR->id,
                      pAMR->rgn_size, index);
        }
        amr_restore_order[index] = i;
    }

    // Complete the Restore
    for (size_t i = 0; i < (size_t) checkpoint_n_regions;
         i++) { // BEGIN Region Loop
        // Index into the ARM through the hash map
        const size_t amr_index = amr_restore_order[i];
        const amr_hndl *const pAMR = &arm_region_list[amr_index];

        const ssize_t bytes_read =
            afv_checkpoint_read(pCheckpoint, pAMR->id, 0,
                                (void *) pAMR->pActive_memory, pAMR->rgn_size);
        if (bytes_read < 0) {
            log_error("Checkpoint Read Error: 0x%x", -bytes_read);
            goto LOCAL_RESTART_FAIL;
        }
        if ((size_t) bytes_read != pAMR->rgn_size) {
            log_warn("Failed to read region %d id=%d, %ld bytes missing", i,
                     pAMR->id, pAMR->rgn_size - bytes_read);
        }
    }

    restored_version = afv_checkpoint_version(pCheckpoint);

LOCAL_RESTART_FAIL:

    afv_checkpoint_destroy(&pCheckpoint);

    return restored_version;
}
