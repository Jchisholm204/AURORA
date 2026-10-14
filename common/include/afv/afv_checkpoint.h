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
#include "afv/afv_metadata.h"

#ifdef AFV_INTERNAL
#include "afv/afv_file.h"
#endif

#include <stddef.h>
#include <stdint.h>

enum aurora_file_versioning_checkpoint_mode {
    eAFVC_MODE_W,
    eAFVC_MODE_R,
    eAFVC_N_MODE,
};

struct aurora_file_versioning_checkpoint
#ifdef AFV_INTERNAL
{
    afv_hndl *pAFV;
    enum aurora_file_versioning_checkpoint_mode file_mode;
    afv_metadata_t *pMetdata;
    afv_file_hndl *pFile;
    // Last Accessed Region Information
    uint64_t region_id;
    uint64_t region_index;
    // Base offset of the last region within the file
    size_t region_base_offset;
    // Pointer to the RW head on the file
    size_t file_offset;
}
#endif
;

typedef struct aurora_file_versioning_checkpoint afv_checkpoint_t;
typedef enum aurora_file_versioning_checkpoint_mode eAFVC_mode;

/**
 * @brief Open a checkpoint from disk
 *
 * @param pAFV File Versioning Handle
 * @param version Version to attempt to load (-1 for latest)
 * @param name or NULL for any
 * @param AFV_CKPT_NAME_LEN
 * @return readable checkpoint file or NULL, destroy with afv_checkpoint_destroy
 */
extern afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pAFV, int64_t version,
                                             const char *name);

/**
 * @brief Create a new checkpoint from a metadata pointer
 *
 * @param pAFV File Versioning Handle
 * @param pMetdata Metadata for the new checkpoint (retains ownership on
 * success)
 * @return A new checkpoint or NULL if metadata verification fails
 * Close with afv_checkpoint_close, or abandon with afv_checkpoint_destroy
 */
extern afv_checkpoint_t *afv_checkpoint_create(afv_hndl *pAFV,
                                               afv_metadata_t *pMetdata);

/**
 * @brief Read a block from a checkpoint (must be opened in read mode)
 *
 * @param pCheckpoint Checkpoint to read from
 * @param region_id Region ID to read
 * @param region_offset Offset within the region
 * @param dst Destination pointer
 * @param size Read block size
 * @return size read from disk
 */
extern size_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                                  const uint64_t region_id,
                                  const size_t region_offset, void *dst,
                                  size_t size);

/**
 * @brief Write a block to a checkpoint
 *
 * @param pCheckpoint Checkpoint to write to
 * @param region_id Region ID to write to
 * @param region_offset Offset within the region to write to
 * @param src Source Pointer
 * @param size Size to write into disk
 * @return Size written to disk
 */
extern size_t afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                                   const uint64_t region_id,
                                   const size_t region_offset,
                                   const void *const src, const size_t size);

/**
 * @brief Close a checkpoint file (writes to disk, frees all memory)
 *
 * @param pCheckpoint Checkpoint to write out
 * @return Checkpoint verification status
 */
extern eAFV_verif afv_checkpoint_close(afv_checkpoint_t **pCheckpoint);

/**
 * @brief Destroy a checkpoint file (frees all memory)
 *
 * @param pCheckpoint Checkpoint to free/destroy
 */
extern void afv_checkpoint_destroy(afv_checkpoint_t **pCheckpoint);

#endif
