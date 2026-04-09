/**
 * @file afv_metadata.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Metadata included in all AURORA Checkpoint Files
 * @version 0.2
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-02
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
#define AFV_RESERVED_SIZE 4
#endif

#ifndef AFV_RESERVED_BYTES
#define AFV_RESERVED_BYTES (AFV_RESERVED_SIZE * sizeof(uint64_t))
#endif

#ifndef BIT
#define BIT(x) (1 << x)
#endif

#ifdef AFV_INTERNAL
#ifndef AFV_METADATA_VERIF_KEY
#define _AFV_VERSION_ ((int32_t) 1)
#define AFV_METADATA_VERIF_KEY                                                 \
    ((uint64_t) ((uint64_t) (-_AFV_VERSION_) << 32) | _AFV_VERSION_)
// #define AFV_METADATA_VERIF_KEY ((uint64_t) 0xFFFFFFFE00000001ULL)
#endif
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum aurora_file_versioning_metadata_verification_e {
    eAFV_VERIF_OK = 0,
    eAFV_VERIF_ERR_NULL = BIT(0),
    eAFV_VERIF_ERR_KEY = BIT(1),
    eAFV_VERIF_ERR_SIZE = BIT(2),
    eAFV_VERIF_ERR_VERSION = BIT(3),
    eAFV_VERIF_ERR_RGN_SIZE = BIT(4),
    eAFV_VERIF_ERR_RGN_IDS = BIT(5),
    eAFV_VERIF_ERR_RGN_SIZES = BIT(6),
    eAFV_VERIF_ERR_RGN_NAMES = BIT(7),
};

struct __attribute__((packed)) aurora_file_versioning_metadata_ops {
    union {
        uint64_t __reserved;
        struct {
            uint64_t version_tag : 4;
            uint64_t with_ec : 1;
        };
    };
};

struct aurora_file_versioning_metadata {
    union {
#ifdef AFV_INTERNAL
        struct {
            size_t metadata_size;
            size_t metadata_key;
            struct aurora_file_versioning_metadata_ops ops;
        };
#endif
        uint64_t __reserved[AFV_RESERVED_SIZE];
    };
    uint64_t rank;
    int64_t version;
    char chkpt_name[AFV_CKPT_NAME_LEN];
    size_t n_regions;
    uint64_t *region_ids;
    size_t *region_sizes;
    char (*region_names)[AFV_RGN_NAME_LEN];
};

typedef struct aurora_file_versioning_metadata afv_metadata_t;
typedef enum aurora_file_versioning_metadata_verification_e eAFV_verif;

extern afv_metadata_t *afv_create_metadata(size_t n_regions);

extern size_t afv_metadata_size(size_t n_regions);

extern void afv_destroy_metadata(afv_metadata_t **ppHndl);

extern size_t afv_metadata_ptr_size(void *);
extern afv_metadata_t *afv_metadata_ptr_init(void *);

extern eAFV_verif afv_metadata_verify(const afv_metadata_t *pMetadata);

// Let this function be included explicitly through a macro,
//  or implicitly if the ARM is already defined and present in the file
#if defined(AFV_METADATA_INC_MATCH) || defined(_ARM_ARM_H_)
#include "arm/arm.h"
extern eAFV_verif afv_metadata_match(const afv_metadata_t *pMetadata,
                                     const amr_hndl *const region_list,
                                     size_t *match_map_list);
#endif

#endif
