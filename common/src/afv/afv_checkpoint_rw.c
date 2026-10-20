/**
 * @file afv_checkpoint_rw.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA File Versioning Checkpoint Read/Write Operations
 * @version 0.1
 * @date Created: 2026-10-17
 * @modified Last Modified: 2026-10-17
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv_checkpoint.h"
#include "log.h"

#include <assert.h>

static inline eAFV_file_error _afv_checkpoint_autoseek(
    afv_checkpoint_t *pCheckpoint, const uint64_t region_id,
    const size_t region_offset) {
    // Internal function, should never be handed a NULL pointer
    assert(pCheckpoint != NULL);
    assert(pCheckpoint->pMetadata != NULL);
    assert(pCheckpoint->pFile != NULL);

    // Load the new region index
    size_t region_file_offset = pCheckpoint->region_base_offset;
    if (pCheckpoint->region_id != region_id) {
        log_debug("Region ID Change");
        uint64_t region_index = 0;
        // Block offset inside of file (sum of previous blocks)
        region_file_offset = 0;
        for (; region_index < pCheckpoint->pMetadata->n_regions;
             region_index++) {
            uint64_t id = pCheckpoint->pMetadata->region_ids[region_index];
            if (id == region_id) {
                break;
            }
            region_file_offset +=
                pCheckpoint->pMetadata->region_sizes[region_index];
        }
        if (region_index >= pCheckpoint->pMetadata->n_regions) {
            log_error("Invalid Region %d", region_id);
            return eAFV_FILE_ERR_RW;
        }

        // Save position
        pCheckpoint->region_index = region_index;
        pCheckpoint->region_id = region_id;
        pCheckpoint->region_base_offset = region_file_offset;

        // Move active pointer
        size_t seek_ptr = region_file_offset + region_offset;
        eAFV_file_error fstatus = afv_file_seek(pCheckpoint->pFile, seek_ptr);
        if (fstatus != eAFV_FILE_OK) {
            log_error("File Error");
            return fstatus;
        }
        pCheckpoint->file_offset = seek_ptr;
    }
    // Check for only offset changes
    else if (pCheckpoint->file_offset != (region_file_offset + region_offset)) {
        log_debug("Region Offset Change");
        if (pCheckpoint->file_mode == eAFVC_MODE_W) {
            log_warn("Possibly Corrupted Checkpoint");
        }
        size_t seek_ptr = region_file_offset + region_offset;
        eAFV_file_error fstatus = afv_file_seek(pCheckpoint->pFile, seek_ptr);
        if (fstatus != eAFV_FILE_OK) {
            log_error("File Error");
            return fstatus;
        }
        pCheckpoint->file_offset = seek_ptr;
    }

    return eAFV_FILE_OK;
}

ssize_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                            const uint64_t region_id,
                            const size_t region_offset, void *dst,
                            size_t size) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return -eAFV_VERIF_ERR_NULL;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_R);

    // Load the new region index
    eAFV_file_error afv_status =
        _afv_checkpoint_autoseek(pCheckpoint, region_id, region_offset);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return -eAFV_VERIF_ERR_FATAL;
    }

    // Handle read overflows
    uint64_t region_base = pCheckpoint->region_base_offset;
    uint64_t region_size =
        pCheckpoint->pMetadata->region_sizes[pCheckpoint->region_index];
    uint64_t region_max = region_base + region_size;
    uint64_t read_start = region_base + region_offset;
    if ((read_start + size) > region_max) {
        log_warn("Read Overflow by %d bytes", (read_start + size) - region_max);
        size = region_max - read_start;
    }

    afv_status = afv_file_read(pCheckpoint->pFile, dst, size);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return -eAFV_VERIF_ERR_FATAL;
    }

    // Save new offset as read + bytes_read
    pCheckpoint->file_offset = read_start + size;

    // Return bytes read
    return size;
}

ssize_t afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                             const uint64_t region_id, const void *const src,
                             size_t size) {

    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return -eAFV_VERIF_ERR_NULL;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_W);

    if (pCheckpoint->region_id != region_id) {
        if (pCheckpoint->region_id != ((uint64_t) -1)) {
            log_error("Checkpoint Region not Finalized");
        }
        // Attempt to seek the new region index (may fail)
        eAFV_file_error afv_status =
            _afv_checkpoint_autoseek(pCheckpoint, region_id, 0);
        if (afv_status != eAFV_FILE_OK) {
            log_error("AFV Error %d", afv_status);
            return -eAFV_VERIF_ERR_FATAL;
        }
    }

    // Handle write overflows
    uint64_t region_base = pCheckpoint->region_base_offset;
    uint64_t region_size =
        pCheckpoint->pMetadata->region_sizes[pCheckpoint->region_index];
    uint64_t region_max = region_base + region_size;
    uint64_t write_start = pCheckpoint->file_offset;
    if ((write_start + size) > region_max) {
        log_warn("Write Overflow by %d bytes",
                 (write_start + size) - region_max);
        size = region_max - write_start;
    }

    eAFV_file_error afv_status = afv_file_write(pCheckpoint->pFile, src, size);
    if (afv_status != eAFV_FILE_OK) {
        log_error("AFV Error %d", afv_status);
        return -eAFV_VERIF_ERR_FATAL;
    }

    // Save new offset as read + bytes_read
    pCheckpoint->file_offset = write_start + size;

    // Reached end of checkpoint, invalidate region
    if (pCheckpoint->file_offset == region_max) {
        pCheckpoint->region_id = (uint64_t) (-1);
        pCheckpoint->region_base_offset = region_max;
        pCheckpoint->region_index += 1;
    }

    // Return bytes read
    return size;
}

ssize_t afv_checkpoint_head(afv_checkpoint_t *pCheckpoint,
                            uint64_t *pRegion_id) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return -eAFV_VERIF_ERR_NULL;
    }
    if (pRegion_id) {
        *pRegion_id = pCheckpoint->region_id;
    }
    if (pCheckpoint->region_id == ((uint64_t) -1)) {
        return 0;
    }
    return pCheckpoint->file_offset - pCheckpoint->region_base_offset;
}
