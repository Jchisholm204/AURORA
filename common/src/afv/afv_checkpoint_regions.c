/**
 * @file afv_checkpoint_regions.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA File Versioning Checkpoint Region Access/Control Code
 * @version 0.1
 * @date Created: 2026-10-17
 * @modified Last Modified: 2026-10-17
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL

#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "assert.h"
#include "log.h"

#include <memory.h>

extern eAFV_verif afv_checkpoint_add(afv_checkpoint_t *pCheckpoint,
                                     uint64_t rgn_id, size_t rgn_size,
                                     const char *restrict rgn_name) {

    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_W);

    // Add region to metadata
    eAFV_verif region_status =
        afv_metadata_add_region(pCheckpoint->pMetadata, rgn_name, rgn_id,
                                rgn_size);

    return region_status;
}

eAFV_verif afv_checkpoint_find_index(afv_checkpoint_t *pCheckpoint,
                                     uint64_t rgn_id, size_t rgn_size,
                                     const char *restrict rgn_name,
                                     size_t *pIndex) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    assert(pCheckpoint->file_mode == eAFVC_MODE_R);

    eAFV_verif region_status =
        afv_metadata_get_region(pCheckpoint->pMetadata, rgn_name, rgn_id,
                                rgn_size, pIndex);
    return region_status;
}

ssize_t _afv_conditional_copy(const void *src, void **pDst, size_t size) {
    // No Destination Supplied
    if (!pDst) {
        return eAFV_VERIF_OK;
    }
    if (!src) {
        log_error("NULL Parameter");
        return -eAFV_VERIF_ERR_NULL;
    }
    // Destination Pointer must be NULL
    if (*pDst) {
        return -eAFV_VERIF_ERR_SIZE;
    }
    // Create Destination Array
    *pDst = malloc(size);
    assert(*pDst != NULL);
    (void) memcpy(*pDst, src, size);
    return size;
}

ssize_t afv_checkpoint_regions(afv_checkpoint_t *pCheckpoint,
                               uint64_t **pID_list, size_t **pSize_list,
                               char **pName_list) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return -eAFV_VERIF_ERR_NULL;
    }
    // Expect metadata has been allocated by the create function
    assert(pCheckpoint->pMetadata != NULL);

    // Overflow safe assignment
    assert(pCheckpoint->pMetadata->n_regions < ((uint64_t) (-1)));
    ssize_t n_regions = (ssize_t) pCheckpoint->pMetadata->n_regions;

    (void) _afv_conditional_copy(pCheckpoint->pMetadata->region_ids,
                                 (void **) pID_list,
                                 n_regions * sizeof(uint64_t));
    (void) _afv_conditional_copy(pCheckpoint->pMetadata->region_sizes,
                                 (void **) pSize_list,
                                 n_regions * sizeof(size_t));
    (void) _afv_conditional_copy(pCheckpoint->pMetadata->region_names,
                                 (void **) pName_list,
                                 n_regions * AFV_RGN_NAME_LEN);
    (void) _afv_conditional_copy(pCheckpoint->pMetadata->region_ids,
                                 (void **) pName_list,
                                 n_regions * AFV_RGN_NAME_LEN);

    return n_regions;
}
