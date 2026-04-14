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
#define AFV_METADATA_INC_MATCH

#include "afv/afv.h"
#include "aul.h"
#include "aul_internal.h"
#include "limits.h"
#include "log.h"

#include <unistd.h>

int AUL_Test(const int version, const char *name) {
    if (!_aul_ctx.pAFV || !_aul_ctx.pARM) {
        log_fatal("Not Initialized");
        return -1;
    }
    const afv_metadata_t *pMetadata =
        afv_get_metadata_versioned(_aul_ctx.pAFV, version, name);
    if (!pMetadata) {
        log_debug("%s-%d Not Loadable", name, version);
        return -1;
    }

    size_t n_regions = arm_get_n_local_regions(_aul_ctx.pARM);
    const amr_hndl *const region_list = arm_get_local_regions(_aul_ctx.pARM);

    eAFV_verif afvv_status =
        afv_metadata_match(pMetadata, region_list, n_regions, NULL);
    if (afvv_status != eAFV_VERIF_OK) {
        log_debug("%s-%d Not Loadable", name, version);
        afv_destroy_metadata((afv_metadata_t **) &pMetadata);
        return -afvv_status;
    }
    int64_t vfound = pMetadata->version;
    afv_destroy_metadata((afv_metadata_t **) &pMetadata);
    return vfound;
}

int AUL_Restart(const int version, const char name[static AUL_NAME_LEN]) {
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
        usleep(1000);
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
