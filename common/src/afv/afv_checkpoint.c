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

#include <assert.h>
#include <malloc.h>
#include <memory.h>

afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pAFV, int64_t version,
                                      const char *name, size_t n_regions,
                                      const eAFVC_mode mode) {
    if (!pAFV) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (mode >= eAFVC_N_MODE) {
        log_error("Invalid Mode Option");
        return NULL;
    }

    afv_checkpoint_t *pCheckpoint = malloc(sizeof(afv_checkpoint_t));
    if (!pCheckpoint) {
        log_error("Bad Alloc??");
        return NULL;
    }

    memset(pCheckpoint, 0, sizeof(afv_checkpoint_t));

    pCheckpoint->pAFV = pAFV;
    *((char *) &pCheckpoint->file_mode) = mode;
    pCheckpoint->file_offset = 0;
    pCheckpoint->region_last_id = ((uint64_t) (-1));
    pCheckpoint->region_last_index = 0;
    pCheckpoint->region_offset = 0;

    assert(mode < eAFVC_N_MODE);
    switch (mode) {
    case eAFVC_MODE_R:
        pCheckpoint->pMetdata =
            (afv_metadata_t *) afv_get_metadata_versioned(pAFV, version, name);
        if (!pCheckpoint->pMetdata) {
            log_error("Failed to open checkpoint metadata");
            goto CHECKPOINT_OPEN_FILE_FAIL;
        }
        if (pCheckpoint->pMetdata->n_regions != n_regions) {
            log_error("Checkpoint Region mismatch");
            goto CHECKPOINT_OPEN_FILE_FAIL;
        }
        pCheckpoint->pFile = afv_file_open_r(pAFV, pCheckpoint->pMetdata);
        break;
    case eAFVC_MODE_W:
        pCheckpoint->pMetdata = afv_create_metadata(n_regions);
        pCheckpoint->pFile = afv_file_open_w(pAFV, version, name);
        break;
    case eAFVC_N_MODE:
        break;
    }

    return pCheckpoint;

CHECKPOINT_OPEN_FILE_FAIL:
    afv_destroy_metadata(&pCheckpoint->pMetdata);
    if (pCheckpoint) {
        free(pCheckpoint);
        pCheckpoint = NULL;
    }
    return NULL;
}

eAFV_file_error _afv_checkpoint_autoseek(afv_checkpoint_t *pCheckpoint,
                                         const uint64_t region_id,
                                         const size_t region_offset) {
    // Load the new region index
    size_t file_offset = pCheckpoint->file_offset;
    if (pCheckpoint->region_last_id != region_id) {
        uint64_t region_index = 0;
        // Block offset inside of file (sum of previous blocks)
        file_offset = 0;
        for (; region_index < pCheckpoint->pMetdata->n_regions;
             region_index++) {
            file_offset += pCheckpoint->pMetdata->region_sizes[region_index];
            if (pCheckpoint->pMetdata->region_sizes[region_index] ==
                region_id) {
                break;
            }
        }
        if (region_index >= pCheckpoint->pMetdata->n_regions) {
            log_error("Invalid Region");
            return eAFV_FILE_ERR_RW;
        }

        // Save position
        pCheckpoint->region_last_index = region_index;
        pCheckpoint->region_last_id = region_id;
        pCheckpoint->file_offset = file_offset;

        // Move active pointer
        size_t seek_ptr = file_offset + region_offset;
        afv_file_seek(pCheckpoint->pFile, seek_ptr);
    }
    // Check for only offset changes
    else if (pCheckpoint->region_offset != region_offset) {
        size_t seek_ptr = file_offset + region_offset;
        afv_file_seek(pCheckpoint->pFile, seek_ptr);
    }

    return eAFV_FILE_OK;
}

size_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                           const uint64_t region_id, const size_t region_offset,
                           void *dst, size_t size) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return 0;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_R);

    // Load the new region index
    eAFV_file_error afv_status =
        _afv_checkpoint_autoseek(pCheckpoint, region_id, region_offset);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return 0;
    }

    // Handle read overflows
    uint64_t region_index = pCheckpoint->region_last_index;
    if ((region_offset + size) >
        pCheckpoint->pMetdata->region_sizes[region_index]) {
        log_warn("Read Overflow");
        size =
            pCheckpoint->pMetdata->region_sizes[region_index] - region_offset;
    }

    afv_status = afv_file_read(pCheckpoint->pFile, dst, size);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return 0;
    }

    // Save new offset as read + bytes_read
    pCheckpoint->region_offset = region_offset + size;

    // Return bytes read
    return size;
}

size_t afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                            const uint64_t region_id,
                            const size_t region_offset, const void *const src,
                            size_t size) {

    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return 0;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_W);

    // Load the new region index
    eAFV_file_error afv_status =
        _afv_checkpoint_autoseek(pCheckpoint, region_id, region_offset);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return 0;
    }

    // Write out new region data
    uint64_t region_index = pCheckpoint->region_last_index;
    pCheckpoint->pMetdata->region_sizes[region_index] += size;

    afv_status = afv_file_write(pCheckpoint->pFile, src, size);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return 0;
    }

    // Save new offset as read + bytes_read
    pCheckpoint->region_offset = region_offset + size;

    // Return bytes read
    return size;
}

long int afv_checkpoint_add_region(afv_checkpoint_t *pCheckpoint,
                                   const uint64_t region_id,
                                   const char name[AFV_RGN_NAME_LEN]) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return -1;
    }
    uint64_t region_index = 0;
    // Block offset inside of file (sum of previous blocks)
    for (; region_index < pCheckpoint->pMetdata->n_regions; region_index++) {
        if (pCheckpoint->pMetdata->region_sizes[region_index] ==
            ((uint64_t) (-1))) {
            break;
        }
    }
    if (region_index == pCheckpoint->pMetdata->n_regions) {
        log_error("No Free Region Slots");
        return -2;
    }

    pCheckpoint->pMetdata->region_ids[region_index] = region_id;
    pCheckpoint->pMetdata->region_sizes[region_index] = 0;
    char *rgn_name = pCheckpoint->pMetdata->region_names[region_index];
    (void) memcpy(rgn_name, name, AFV_RGN_NAME_LEN);

    // Return the metadata index (should match region index)
    return (long int) region_index;
}

eAFV_file_error afv_checkpoint_close(afv_checkpoint_t **ppCheckpoint) {
    if (!ppCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    afv_checkpoint_t *pCheckpoint = *ppCheckpoint;
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    eAFV_file_error afv_status = afv_file_close(&pCheckpoint->pFile);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV File Error %d", afv_status);
        return afv_status;
    }

    if (pCheckpoint->file_mode == eAFVC_MODE_W) {
        afv_write_metadata(pCheckpoint->pAFV, pCheckpoint->pMetdata);
        if (afv_status != eAFV_FILE_OK) {
            log_error("AFV File Error %d", afv_status);
            return afv_status;
        }
        pCheckpoint->pMetdata = NULL;
    }

    afv_checkpoint_free(ppCheckpoint);

    return eAFV_FILE_OK;
}

void afv_checkpoint_free(afv_checkpoint_t **ppCheckpoint) {
    if (!ppCheckpoint) {
        return;
    }
    afv_checkpoint_t *pCheckpoint = *ppCheckpoint;
    if (!pCheckpoint) {
        return;
    }
    if (pCheckpoint->pMetdata) {
        afv_destroy_metadata(&pCheckpoint->pMetdata);
    }
}
