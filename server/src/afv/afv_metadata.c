/**
 * @file afv_metadata.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv_metadata.h"

#include "log.h"

#include <malloc.h>
#include <memory.h>

const afv_metadata_t *afv_create_metadata(
    uint64_t rank, int64_t version, char chkpt_name[AFV_CKPT_NAME_LEN],
    size_t n_regions, uint64_t *region_ids,
    char (*region_names)[AFV_RGN_NAME_LEN]) {
    if (!chkpt_name) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!region_ids && (n_regions > 0)) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!region_names && (n_regions > 0)) {
        log_error("NULL Parameter");
        return NULL;
    }

    if (version < 0) {
        log_error("Version Must be Positive");
    }

    size_t metadata_size = afv_metadata_size(n_regions);

    log_debug("Metadata size=%d", metadata_size);

    afv_metadata_t *pMetadata = afv_metadata_ptr_init(malloc(metadata_size));
    if (!pMetadata) {
        log_error("Bad Alloc??");
    }

    pMetadata->metadata_size = metadata_size;

    pMetadata->rank = rank;
    pMetadata->version = version;
    (void) memcpy(pMetadata->chkpt_name, chkpt_name, AFV_CKPT_NAME_LEN);
    pMetadata->n_regions = n_regions;
    if (n_regions > 0) {
        (void) memccpy(pMetadata->region_ids, region_ids, n_regions,
                       sizeof(uint64_t));
        (void) memccpy(pMetadata->region_names, region_names, n_regions,
                       sizeof(char[AFV_RGN_NAME_LEN]));
    }

    return pMetadata;
}

size_t afv_metadata_size(size_t n_regions) {
    size_t struct_size = sizeof(afv_metadata_t);
    size_t name_size = sizeof(char[AFV_CKPT_NAME_LEN]);
    size_t rgn_ids_size = sizeof(uint64_t) * n_regions;
    size_t rgn_sizes_size = sizeof(size_t) * n_regions;
    size_t rgn_names_size = sizeof(char[AFV_RGN_NAME_LEN]) * n_regions;
    return struct_size + name_size + rgn_ids_size + rgn_sizes_size +
           rgn_names_size;
}

void afv_destroy_metadata(afv_metadata_t **ppHndl) {
    if (ppHndl) {
        if (*ppHndl) {
            free(*ppHndl);
            *ppHndl = NULL;
        }
    }
}

size_t afv_metadata_ptr_size(void *block_ptr) {
    if (!block_ptr) {
        return 0;
    }
    // Head should always be the struct
    afv_metadata_t *pMetadata = (void *) block_ptr;

    // Return Reserved Region
    return pMetadata->metadata_size;
}

afv_metadata_t *afv_metadata_ptr_init(void *block_ptr) {
    if (!block_ptr) {
        log_error("NULL Parameter");
        return NULL;
    }

    // Head should always be the struct
    afv_metadata_t *pMetadata = (void *) block_ptr;

    // Sizes
    size_t struct_size = sizeof(afv_metadata_t);
    size_t name_size = sizeof(char[AFV_CKPT_NAME_LEN]);
    size_t rgn_ids_size = sizeof(uint64_t) * pMetadata->n_regions;
    size_t rgn_sizes_size = sizeof(size_t) * pMetadata->n_regions;
    size_t rgn_names_size =
        sizeof(char[AFV_RGN_NAME_LEN]) * pMetadata->n_regions;

    // Offsets
    size_t struct_offset = 0;
    size_t name_offset = struct_offset + struct_size;
    size_t rgn_ids_offset = name_offset + name_size;
    size_t rgn_sizes_offset = rgn_ids_offset + rgn_ids_size;
    size_t rgn_names_offset = rgn_sizes_offset + rgn_sizes_size;
    // End of Struct
    (void) rgn_names_size;

    // Setup pointer offsets
    pMetadata->chkpt_name = (void *) ((uint8_t *) block_ptr + name_offset);
    pMetadata->region_ids = (void *) ((uint8_t *) block_ptr + rgn_ids_offset);
    pMetadata->region_sizes = (void *) ((uint8_t *) block_ptr + rgn_sizes_size);
    pMetadata->region_names =
        (void *) ((uint8_t *) block_ptr + rgn_names_offset);
    return pMetadata;
}
