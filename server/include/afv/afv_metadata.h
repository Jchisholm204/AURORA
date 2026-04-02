/**
 * @file afv_metadata.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Metadata included in all AURORA Checkpoint Files
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_METADATA_H_
#define _AFV_METADATA_H_

#ifndef AVF_RGN_NAME_LEN
#define AVF_RGN_NAME_LEN 16
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct aurora_file_versioning_metadata {
    uint64_t __reserved[2];
    uint64_t rank;
    int64_t version;
    char *chkpt_name;
    size_t n_regions;
    uint64_t *region_ids;
    char *region_names[AVF_RGN_NAME_LEN];
};

typedef struct aurora_file_versioning_metadata afv_metadata_t;

extern const afv_metadata_t *afv_create_metadata(
    uint64_t rank, uint64_t version, char *chkpt_name, size_t n_regions,
    uint64_t *region_ids, char *region_names[static AVF_RGN_NAME_LEN]);

extern void afv_destroy_metadata(afv_metadata_t **ppHndl);


#endif
