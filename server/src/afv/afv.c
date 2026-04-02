/**
 * @file afv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief File Versioning System
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv.h"

#include "log.h"

#include <malloc.h>
#include <string.h>

afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                              int64_t group_size, char *persistent_path,
                              bool with_ec) {
    if (!persistent_path) {
        log_error("NULL Parameter");
        return NULL;
    }

    afv_hndl *pHndl = malloc(sizeof(afv_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->persistent_path = strdup(persistent_path);
    pHndl->pMetadata = NULL;

    if (!pHndl->persistent_path) {
        log_error("Bad Alloc??");
        free(pHndl);
        return NULL;
    }

    if (!pHndl->pMetadata) {
        log_info("Unable to find metadata for %d of %d in group %d at path: %s",
                 rank, group_size, group_id, persistent_path);
    }

    // Need to load metadata from the file
    pHndl->pMetadata =
        (afv_metadata_t *) afv_create_metadata(rank, 0, "______INIT_____", 0,
                                               NULL, NULL);
    if (!pHndl->pMetadata) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->rank = rank;
    pHndl->group_id = group_id;
    pHndl->use_error_correction = with_ec;

    return pHndl;
}

void afv_destroy_instance(afv_hndl **ppHndl) {
#warning "Unimplemented"
    if (!ppHndl) {
        return;
    }
    afv_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        return;
    }
    if (pHndl->persistent_path) {
        free(pHndl->persistent_path);
    }
    if (pHndl->pMetadata) {
        afv_destroy_metadata(&pHndl->pMetadata);
    }
    free(pHndl);
    *ppHndl = NULL;
}

const afv_metadata_t *afv_get_metadata_versioned(afv_hndl *pHndl,
                                                 int64_t version, char *name) {
}

const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }
    return pHndl->pMetadata;
}

int afv_write_metadata(afv_hndl *pHndl, afv_metadata_t *const pMetadata) {
}

uint64_t afv_get_rank(afv_hndl *pHndl) {
}
