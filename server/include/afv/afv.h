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
    afv_metadata_t *pMetadata;
}
#endif
;

typedef struct aurora_file_versioning_handle afv_hndl;

extern afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                                     int64_t group_size, char *persistent_path,
                                     bool with_ec);

extern void afv_destroy_instance(afv_hndl **ppHndl);

extern afv_metadata_t *afv_get_latest(afv_hndl *pHndl, int64_t group_id,
                                      uint64_t rank);

extern int afv_set_latest(afv_hndl *pHndl, int64_t group_id, uint64_t rank,
                          afv_metadata_t *pMetadata);

#endif
