/**
 * @file afv_metadata.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv_metadata.h"

#include "arm/arm.h"
#include "log.h"

#include <malloc.h>
#include <memory.h>

// #define USE_EARLY_EXIT

afv_metadata_t *afv_create_metadata(size_t n_regions) {

    size_t metadata_size = afv_metadata_size(n_regions);

    log_debug("Metadata size=%d", metadata_size);

    afv_metadata_t *pMetadata = malloc(metadata_size);
    if (!pMetadata) {
        log_error("Bad Alloc??");
    }

    memset(pMetadata, 0, sizeof(afv_metadata_t));

    pMetadata->metadata_size = metadata_size;
    pMetadata->metadata_key = AFV_METADATA_VERIF_KEY;

    pMetadata->rank = 0;
    pMetadata->version = 0;
    pMetadata->n_regions = n_regions;

    return afv_metadata_ptr_init(pMetadata);
}

size_t afv_metadata_size(size_t n_regions) {
    size_t struct_size = sizeof(afv_metadata_t);
    size_t rgn_ids_size = sizeof(uint64_t) * n_regions;
    size_t rgn_sizes_size = sizeof(size_t) * n_regions;
    size_t rgn_names_size = sizeof(char[AFV_RGN_NAME_LEN]) * n_regions;
    return struct_size + rgn_ids_size + rgn_sizes_size + rgn_names_size;
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
    size_t rgn_ids_size = sizeof(uint64_t) * pMetadata->n_regions;
    size_t rgn_sizes_size = sizeof(size_t) * pMetadata->n_regions;
    size_t rgn_names_size =
        sizeof(char[AFV_RGN_NAME_LEN]) * pMetadata->n_regions;

    // Offsets
    size_t struct_offset = 0;
    size_t rgn_ids_offset = struct_offset + struct_size;
    size_t rgn_sizes_offset = rgn_ids_offset + rgn_ids_size;
    size_t rgn_names_offset = rgn_sizes_offset + rgn_sizes_size;
    // End of Struct
    (void) rgn_names_size;

    // Setup pointer offsets
    pMetadata->region_ids = (void *) ((uint8_t *) block_ptr + rgn_ids_offset);
    pMetadata->region_sizes =
        (void *) ((uint8_t *) block_ptr + rgn_sizes_offset);
    pMetadata->region_names =
        (void *) ((uint8_t *) block_ptr + rgn_names_offset);
    return pMetadata;
}

eAFV_verif afv_metadata_verify(const afv_metadata_t *pMetadata) {
    if (!pMetadata) {
        log_trace("Err");
        return eAFV_VERIF_ERR_NULL;
    }
    eAFV_verif status = eAFV_VERIF_OK;

    if (pMetadata->metadata_key != AFV_METADATA_VERIF_KEY) {
        log_trace("Err");
        status |= eAFV_VERIF_ERR_KEY;
    }

    if (pMetadata->metadata_size != afv_metadata_size(pMetadata->n_regions)) {
        log_trace("Err");
        status |= eAFV_VERIF_ERR_SIZE;
    }

    if (pMetadata->version < 0) {
        log_trace("Err");
        status |= eAFV_VERIF_ERR_VERSION;
    }

#warning "TODO: Implement other verifiers"

    return status;
}

eAFV_verif afv_metadata_match(const afv_metadata_t *pMetadata,
                              const amr_hndl *const region_list,
                              size_t n_regions, size_t *match_map_list) {
    if (!pMetadata) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    if (!region_list) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    if (!match_map_list) {
        log_warn("NULL Parameter");
        // return eAFV_VERIF_ERR_NULL;
    }

    log_trace("Attempting to match metadata (md_rgns=%d)",
              pMetadata->n_regions);

    eAFV_verif status = eAFV_VERIF_OK;
    eAFV_verif ignore_status = ~eAFV_VERIF_OK;

    if (n_regions != pMetadata->n_regions) {
        log_trace("%zu != %zu regions", n_regions, pMetadata->n_regions);
        status |= eAFV_VERIF_ERR_SIZE;
        ignore_status &= ~eAFV_VERIF_ERR_SIZE;
#if defined(USE_EARLY_EXIT)
        return status;
#endif
    }

    // BEGIN CLI Region Loop
    for (size_t i = 0; i < pMetadata->n_regions; i++) { // BEGIN CRL
        eAFV_verif match_status = eAFV_VERIF_OK;
        // BEGIN Metadata Region Loop
        for (size_t j = 0; j < n_regions; j++) { // BEGIN MRL
            const amr_hndl *pAMR = &region_list[j];
            match_status = eAFV_VERIF_OK;
            if (pMetadata->region_ids[i] != pAMR->id) {
                match_status |= eAFV_VERIF_ERR_RGN_IDS;
            }
            if (pMetadata->region_sizes[i] != pAMR->rgn_size) {
                match_status |= eAFV_VERIF_ERR_RGN_SIZES;
            }
            if (strcmp(pMetadata->region_names[i], pAMR->name)) {
                match_status |= eAFV_VERIF_ERR_RGN_NAMES;
            }
            if ((match_status & ignore_status) == eAFV_VERIF_OK) {
                log_trace("Matched: %d -> %d (%d)", i, j, pAMR->id);
                if (match_map_list) {
                    match_map_list[i] = j;
                }
                break;
            }
        } // END MRL
        if ((match_status & ignore_status) != eAFV_VERIF_OK) {
            status = status | match_status;
            log_trace("Failed to Match: %d=%d status=0x%lx/0x%lx", i,
                      pMetadata->region_ids[i], status, ignore_status);
#if defined(USE_EARLY_EXIT)
            return status;
#endif
        }
    } // END CLI Region Loop

    return status;
}
