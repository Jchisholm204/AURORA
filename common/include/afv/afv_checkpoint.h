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

enum aurora_file_versioning_checkpoint_mode {
    eAFVC_MODE_W,
    eAFVC_MODE_R,
    eAFVC_N_MODE,
};

struct aurora_file_versioning_checkpoint
#ifdef AFV_INTERNAL
{
    afv_hndl *pAFV;
    const enum aurora_file_versioning_checkpoint_mode file_mode;
    size_t file_offset;
    afv_metadata_t *pMetdata;
    afv_file_hndl *pFile;
    uint64_t region_last_id;
    uint64_t region_last_index;
    size_t region_offset;
}
#endif
;

typedef struct aurora_file_versioning_checkpoint afv_checkpoint_t;
typedef enum aurora_file_versioning_checkpoint_mode eAFVC_mode;

/**
 * @brief Loads the checkpoint file from its metadata
 *
 * @param pHndl AFV Handle (stored internally)
 * @param version Version of Checkpoint to get
 * @param name Checkpoint Name or NULL for `*`
 * @param mode Opening Mode (eAFVC_MODE_R or W)
 * @return
 */
extern afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pAFV, int64_t version,
                                             const char *name, size_t n_regions,
                                             const eAFVC_mode mode);

extern size_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                                  const uint64_t region_id,
                                  const size_t region_offset, void *dst,
                                  size_t size);

extern size_t afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                                   const uint64_t region_id,
                                   const size_t region_offset,
                                   const void *const src, const size_t size);

extern long int afv_checkpoint_add_region(afv_checkpoint_t *pCheckpoint,
                                          const uint64_t region_id,
                                          const char name[AFV_RGN_NAME_LEN]);

extern eAFV_file_error afv_checkpoint_close(afv_checkpoint_t **pCheckpoint);

extern void afv_checkpoint_free(afv_checkpoint_t **pCheckpoint);

#endif
