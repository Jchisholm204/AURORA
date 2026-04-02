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

char *afv_get_filename(afv_hndl *pHndl, int version, char *checkpoint_name) {

    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!checkpoint_name) {
        log_error("NULL Parameter");
        return NULL;
    }

    static const char *format_string = "%s/%s-%d.abak";

    size_t filename_len =
        snprintf(NULL, 0, format_string, pHndl->persistent_path,
                 checkpoint_name, version) +
        2;
    char *filename = malloc(filename_len);
    if (!filename) {
        log_error("Bad Alloc??");
        return NULL;
    }

    snprintf(filename, filename_len, format_string, pHndl->persistent_path,
             checkpoint_name, version);

    return filename;
}

int afv_update_metadata(afv_hndl *pHndl, afv_metadata_t *const pMetadata) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return -1;
    }
    if (!pMetadata) {
        log_error("NULL Parameter");
        return -1;
    }
    log_trace("Metadata Version Update %d -> %d", pHndl->pMetadata->version,
              pMetadata->version);
    afv_destroy_metadata(&pHndl->pMetadata);
    pHndl->pMetadata = pMetadata;
    return 0;
}

const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }
    return pHndl->pMetadata;
}
