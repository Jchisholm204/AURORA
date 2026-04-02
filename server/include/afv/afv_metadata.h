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

#ifndef AFV_RGN_NAME_LEN
#define AFV_RGN_NAME_LEN 16
#endif

#ifndef AFV_CKPT_NAME_LEN
#define AFV_CKPT_NAME_LEN 16
#endif

#ifndef AFV_RESERVED_SIZE
#define AFV_RESERVED_SIZE 2
#endif

#ifndef AFV_RESERVED_BYTES
#define AFV_RESERVED_BYTES (AFV_RESERVED_SIZE * sizeof(uint64_t))
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct aurora_file_versioning_metadata {
    union {
#ifdef AFV_INTERNAL
        struct {
            size_t metadata_size;
        };
#endif
        uint64_t __reserved[AFV_RESERVED_SIZE];
    };
    uint64_t rank;
    int64_t version;
    char *chkpt_name;
    size_t n_regions;
    uint64_t *region_ids;
    size_t *region_sizes;
    char (*region_names)[AFV_RGN_NAME_LEN];
};

typedef struct aurora_file_versioning_metadata afv_metadata_t;

extern const afv_metadata_t *afv_create_metadata(
    uint64_t rank, int64_t version, char chkpt_name[AFV_CKPT_NAME_LEN],
    size_t n_regions, uint64_t *region_ids,
    char (*region_names)[AFV_RGN_NAME_LEN]);

extern size_t afv_metadata_size(size_t n_regions);

extern void afv_destroy_metadata(afv_metadata_t **ppHndl);

extern size_t afv_metadata_ptr_size(void *);
extern afv_metadata_t *afv_metadata_ptr_init(void *);

#endif
