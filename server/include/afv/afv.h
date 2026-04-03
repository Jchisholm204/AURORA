/**
 * @file afv.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AVF_AVF_H_
#define _AVF_AVF_H_

#include "afv_metadata.h"

struct aurora_file_versioning_handle
#ifdef AFV_INTERNAL
{
    uint64_t rank;
    int64_t group_id;
    char *persistent_path;
    bool use_error_correction;
    afv_metadata_t *pMetadata;
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
 * @returns NULL if the version was not found
 */
extern const afv_metadata_t *afv_get_metadata_versioned(afv_hndl *pHndl,
                                                        int64_t version,
                                                        const char *name);

/**
 * @brief Gets the internally cached metadata pointer
 *
 * @param pHndl
 * @return
 */
extern const afv_metadata_t *afv_get_metadata(afv_hndl *pHndl);

extern int afv_write_metadata(afv_hndl *pHndl, afv_metadata_t *const pMetadata);

extern uint64_t afv_get_rank(afv_hndl *pHndl);

#endif
