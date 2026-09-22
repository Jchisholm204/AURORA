/**
 * @file afv_checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-08-12
 * @modified Last Modified: 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_CHECKPOINT_H_
#define _AFV_CHECKPOINT_H_

#include "afv/afv.h"
#include "afv/afv_file.h"
#include "afv/afv_metadata.h"

#include <stddef.h>
#include <stdint.h>

#ifndef AFV_RESERVED_SIZE
#define AFV_RESERVED_SIZE 2
#endif

#if AFV_RESERVED_SIZE < 3
#error "AFV Checkpoint requires a larger reserved region"
#endif

struct __attribute__((packed)) aurora_file_versioning_checkpoint {
#ifdef AFV_INTERNAL
    afv_hndl *pAFV;
    uint64_t region_last_accessed;
    const char file_mode;
    afv_metadata_t *pMetdata;
    afv_file_hndl *pFile;
#endif
};

typedef struct aurora_file_versioning_checkpoint afv_checkpoint_t;

/**
 * @brief Loads the checkpoint file from its metadata
 *
 * @param pHndl AFV Handle (stored internally)
 * @param pMetadata Checkpoint Metadata (stored internally)
 * @return
 */
extern afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pHndl,
                                             afv_metadata_t *pMetadata,
                                             const char mode);

extern size_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                                  uint64_t region_id, size_t region_offset,
                                  void *dst, size_t size);

extern afv_checkpoint_t *afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                                              uint64_t region_id,
                                              size_t region_offset, void *src,
                                              size_t size);

extern afv_checkpoint_t *afv_checkpoint_close(afv_checkpoint_t *pCheckpoint);

#endif
