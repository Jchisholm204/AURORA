/**
 * @file afv.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.3
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-10-15
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AVF_AVF_H_
#define _AVF_AVF_H_

#include "afv_metadata.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct aurora_file_versioning_handle
#ifdef AFV_INTERNAL
{
    uint64_t rank;
    int64_t group_id;
    uint64_t group_size;
    char *persistent_path;
    bool use_error_correction;
    atomic_size_t reference_counter;
}
#endif
;

typedef struct aurora_file_versioning_handle afv_hndl;

extern afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                                     int64_t group_size, char *persistent_path,
                                     bool with_ec);

extern void afv_destroy_instance(afv_hndl **ppHndl);

/**
 * @brief Gets the matching checkpoint version metadata from the disk
 *
 * @param pHndl AFV Handle
 * @param version Version of Checkpoint to get
 * @param name Checkpoint Name or NULL for `*`
 * @returns NULL if the version was not found, free with
 * `afv_metadata_destroy((afv_metadata_t **) &pMetadata);`
 */
extern const afv_metadata_t *afv_read_metadata(afv_hndl *pHndl, int64_t version,
                                               const char *name);

/**
 * @brief Writes the metadata out to disk
 *
 * @param pHndl AFV Handle to write out to
 * @param pMetadata Metadata to write out
 * @return Success or Verification Error
 */
extern eAFV_verif afv_write_metadata(afv_hndl *pHndl,
                                     const afv_metadata_t *pMetadata);

extern uint64_t afv_get_rank(afv_hndl *pHndl);

#endif
