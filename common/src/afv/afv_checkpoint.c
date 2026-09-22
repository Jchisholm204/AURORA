/**
 * @file afv_checkpoint.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-09-22
 * @modified Last Modified: 2026-09-22
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL

#include "afv/afv_checkpoint.h"

#include "log.h"

#include <malloc.h>

afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pHndl,
                                      afv_metadata_t *pMetadata,
                                      const char mode) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!pMetadata) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (mode != 'r' && mode != 'w') {
        log_error("Invalid Mode Option %c not in (r, w)", mode);
        return NULL;
    }

    afv_checkpoint_t *pCheckpoint = malloc(sizeof(afv_checkpoint_t));
    if (!pCheckpoint) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pCheckpoint->pAFV = pHndl;
    pCheckpoint->region_last_accessed = ((uint64_t) (-1));
    *((char *) &pCheckpoint->file_mode) = mode;
    pCheckpoint->pMetdata = pMetadata;
}

size_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint, uint64_t region_id,
                           size_t region_offset, void *dst, size_t size) {
}

afv_checkpoint_t *afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                                       uint64_t region_id, size_t region_offset,
                                       void *src, size_t size) {
}

afv_checkpoint_t *afv_checkpoint_close(afv_checkpoint_t *pCheckpoint) {
}
