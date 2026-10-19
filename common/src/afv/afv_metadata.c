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

afv_metadata_t *afv_metadata_create(size_t n_regions) {

    size_t metadata_size = afv_metadata_size(n_regions);

    log_trace("Creating Metadata size=%d", metadata_size);

    afv_metadata_t *pMetadata = malloc(metadata_size);
    if (!pMetadata) {
        log_error("Bad Alloc??");
        return NULL;
    }

    memset(pMetadata, 0, metadata_size);

    pMetadata->metadata_size = metadata_size;
    pMetadata->metadata_key = AFV_METADATA_VERIF_KEY;

    pMetadata->rank = 0;
    pMetadata->version = -1;
    pMetadata->n_regions = n_regions;

    pMetadata = afv_metadata_ptr_init(pMetadata);

    for (size_t i = 0; i < n_regions; i++) {
        pMetadata->region_ids[i] = (-1);
    }

    return pMetadata;
}

size_t afv_metadata_size(size_t n_regions) {
    size_t struct_size = sizeof(afv_metadata_t);
    size_t rgn_ids_size = sizeof(uint64_t) * n_regions;
    size_t rgn_sizes_size = sizeof(size_t) * n_regions;
    size_t rgn_names_size = sizeof(char[AFV_RGN_NAME_LEN]) * n_regions;
    return struct_size + rgn_ids_size + rgn_sizes_size + rgn_names_size;
}

void afv_metadata_destroy(afv_metadata_t **ppHndl) {
    if (ppHndl) {
        if (*ppHndl) {
            free(*ppHndl);
            *ppHndl = NULL;
        }
    }
}

size_t afv_metadata_ptr_size(const void *const block_ptr) {
    if (!block_ptr) {
        return 0;
    }
    // Head should always be the struct
    const afv_metadata_t *const pMetadata = block_ptr;

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
        log_warn("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    eAFV_verif status = eAFV_VERIF_OK;

    if (pMetadata->metadata_key != AFV_METADATA_VERIF_KEY) {
        log_debug("Metadata Key Verification Failure");
        status |= eAFV_VERIF_ERR_KEY;
    }

    if (pMetadata->metadata_size != afv_metadata_size(pMetadata->n_regions)) {
        log_debug("Metadata Size Verification Failure");
        status |= eAFV_VERIF_ERR_SIZE;
    }

    if (pMetadata->version < 0) {
        log_debug("Invalid Checkpoint Version");
        status |= eAFV_VERIF_ERR_VERSION;
    }

    for (size_t i = 0; i < pMetadata->n_regions; i++) {
        uint64_t id = pMetadata->region_ids[i];
        uint64_t size = pMetadata->region_sizes[i];
        if (size == 0) {
            log_debug("Region %d Verification Size Failure", i);
            status |= eAFV_VERIF_ERR_RGN_SIZES;
        }
        if (((int64_t) id) < 0) {
            log_debug("Region %d Verification ID Failure", i);
            status |= eAFV_VERIF_ERR_RGN_IDS;
        }
    }

    return status;
}

eAFV_verif afv_metadata_set_name(afv_metadata_t *pMetadata,
                                 const char *restrict name) {
    if (!pMetadata) {
        log_warn("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    if (!name) {
        memset(pMetadata->chkpt_name, '\0', AFV_CKPT_NAME_LEN);
        return eAFV_VERIF_ERR_NULL | eAFV_VERIF_ERR_SIZE;
    }

    size_t bytes_written =
        snprintf(pMetadata->chkpt_name, AFV_CKPT_NAME_LEN, "%s", name);

    if (bytes_written == AFV_CKPT_NAME_LEN) {
        return eAFV_VERIF_ERR_SIZE;
    }

    return eAFV_VERIF_OK;
}

eAFV_verif afv_metadata_set_version(afv_metadata_t *pMetadata,
                                    int64_t version) {
    if (version < 0) {
        return eAFV_VERIF_ERR_VERSION;
    }
    if (!pMetadata) {
        log_warn("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    pMetadata->version = version;
    return eAFV_VERIF_OK;
}

eAFV_verif afv_metadata_add_region(afv_metadata_t *pMetadata,
                                   const char *restrict name, uint64_t id,
                                   size_t size) {

    // Check parameters
    if (size == 0) {
        return eAFV_VERIF_ERR_RGN_SIZES;
    }
    if (id == ((uint64_t) (-1))) {
        return eAFV_VERIF_ERR_RGN_IDS;
    }

    if (!pMetadata) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    // Find next open index
    size_t index = 0;
    for (; index < pMetadata->n_regions; index++) {
        size_t rsize = pMetadata->region_sizes[index];
        uint64_t rid = pMetadata->region_ids[index];
        if (rsize == 0 && rid == ((uint64_t) -1)) {
            break;
        }
        // Assume duplicates overwrite
        if (rid == id) {
            log_warn("Overwriting region %d id=%d", index, id);
            break;
        }
    }

    if (index >= pMetadata->n_regions) {
        log_error("Over Capacity");
        return eAFV_VERIF_ERR_SIZE;
    }

    pMetadata->region_ids[index] = id;
    pMetadata->region_sizes[index] = size;
    if (name) {
        (void) snprintf(pMetadata->region_names[index], AFV_RGN_NAME_LEN, "%s",
                        name);
    }
    return eAFV_VERIF_OK;
}

eAFV_verif afv_metadata_get_region(const afv_metadata_t *pMetadata,
                                   const char *restrict name, uint64_t id,
                                   size_t size, size_t *pIndex) {
    if (!pMetadata) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    // Verification Status Collector
    eAFV_verif verification = eAFV_VERIF_OK;

    // Find region within the list
    size_t index = 0;
    for (; index < pMetadata->n_regions; index++) {
        size_t rsize = pMetadata->region_sizes[index];
        uint64_t rid = pMetadata->region_ids[index];
        if ((rsize == size || size == 0) && rid == id) {
            break;
        } else if (rid == id) {
            // Found Region but size does not match
            verification |= eAFV_VERIF_ERR_RGN_SIZES;
            break;
        }
        // Assume duplicates overwrite
        if (rid == id) {
            log_warn("Overwriting region %d id=%d", index, id);
            break;
        }
    }

    // Region not found
    if (index >= pMetadata->n_regions) {
        log_error("Over Capacity");
        return eAFV_VERIF_ERR_RGN_IDS;
    }

    // Name verification
    if (name) {
        size_t name_len = strnlen(name, AFV_RGN_NAME_LEN);
        char match = 0x00;
        for (size_t i = 0; i < name_len; i++) {
            match |= name[i] ^ pMetadata->region_names[index][i];
        }
        if (match != 0x00) {
            // Collect Status
            verification |= eAFV_VERIF_ERR_RGN_NAMES;
        }
    }

    if (pIndex) {
        log_trace("Returning Region %d at index %d", id, index);
        *pIndex = index;
    }
    return verification;
}

eAFV_verif afv_metadata_check(const afv_metadata_t *pMetadata, int64_t version,
                              char *name, size_t n_regions) {
    if (!pMetadata) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    log_debug("Regions %d == %d", pMetadata->n_regions, n_regions);
    if (pMetadata->n_regions != n_regions && n_regions != 0) {
        return eAFV_VERIF_ERR_SIZE;
    }

    if (pMetadata->version != version || version < 0 ||
        pMetadata->version < 0) {
        return eAFV_VERIF_ERR_VERSION;
    }

    // Name verification
    if (name) {
        size_t name_len = strnlen(name, AFV_RGN_NAME_LEN);
        char match = 0x00;
        for (size_t i = 0; i < name_len; i++) {
            match |= name[i] ^ pMetadata->chkpt_name[i];
        }
        if (match != 0x00) {
            return eAFV_VERIF_ERR_NAME;
        }
    }

    return eAFV_VERIF_OK;
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
        // May be NULL if not being checked
        // Log this to prevent easily fixable errors
        log_debug("NULL Parameter");
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
