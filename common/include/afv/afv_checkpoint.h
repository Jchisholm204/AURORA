/**
 * @file afv_checkpoint.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-08-12
 * @modified Last Modified: 2026-10-17
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_CHECKPOINT_H_
#define _AFV_CHECKPOINT_H_

#include "afv/afv.h"
#include "afv/afv_metadata.h"
#include "common_types.h"

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
    afv_metadata_t *pMetadata;
    afv_hndl *pAFV;
    enum aurora_file_versioning_checkpoint_mode file_mode;
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
 * @param version Checkpoint Version
 * @param name Checkpoint Name
 * @param n_regions Number of regions in the checkpoint
 * @return A new checkpoint or NULL if metadata verification fails
 *
 * Close with afv_checkpoint_close, or abandon with afv_checkpoint_destroy
 */
extern afv_checkpoint_t *afv_checkpoint_create(
    afv_hndl *pAFV, int64_t version, const char name[AFV_CKPT_NAME_LEN],
    size_t n_regions);

/**
 * @brief Get the name of the checkpoint
 *
 * @param pCheckpoint
 * @returns read only pointer, do not free
 */
extern const char *afv_checkpoint_name(afv_checkpoint_t *pCheckpoint);

/**
 * @brief Get the version of the checkpoint
 *
 * @param pCheckpoint
 * @return
 */
extern int64_t afv_checkpoint_version(afv_checkpoint_t *pCheckpoint);

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

/**
 * @brief Adds a region to the checkpoint, must call before writing
 *
 * assert(open=='W')
 *
 * @param pCheckpoint Checkpoint to add to
 * @param rgn_id ID of the new region
 * @param rgn_size Size of the new region
 * @param rgn_name Optional: Name of the new region
 * @return
 */
extern eAFV_verif afv_checkpoint_add(afv_checkpoint_t *pCheckpoint,
                                     uint64_t rgn_id, size_t rgn_size,
                                     const char *restrict rgn_name);

/**
 * @brief Finds a matching region within the checkpoint metadata
 *
 * assert(open=='R')
 *
 * @param pCheckpoint Checkpoint to look in
 * @param rgn_id ID of the region
 * @param rgn_size Size of teh region
 * @param rgn_name Optional: Name of the region
 * @param pIndex Return: Position of the region in the checkpoint file
 * @return
 */
extern eAFV_verif afv_checkpoint_find_index(afv_checkpoint_t *pCheckpoint,
                                            uint64_t rgn_id, size_t rgn_size,
                                            const char *restrict rgn_name,
                                            size_t *pIndex);

#define afv_checkpoint_find(pCheckpoint, rgn_id, rgn_size, rgn_name)           \
    afv_checkpoint_find_index(pCheckpoint, rgn_id, rgn_size, rgn_name, NULL)

/**
 * @brief Returns Lists of Region Information From the Checkpoint, Set to NULL
 * to disable copy. Size will still be returned if all list pointers are NULL.
 *
 * @param pCheckpoint Checkpoint
 * @param pID_list Optional: List of Region IDs (in checkpointing order)
 * @param pSize_list Optional: List of Region Sizes
 * @param pName_list Optional: List of Region Names
 * @returns size of the list, or -(eAFV_verif) on error
 */
extern ssize_t afv_checkpoint_regions(afv_checkpoint_t *pCheckpoint,
                                      uint64_t **pID_list, size_t **pSize_list,
                                      char **pName_list);

#define afv_checkpoint_n_regions(pCheckpoint)                                  \
    afv_checkpoint_regions(pCheckpoint, NULL, NULL, NULL)

/**
 * @brief Read a block from a checkpoint (must be opened in read mode)
 *
 * assert(open=='R')
 *
 * @param pCheckpoint Checkpoint to read from
 * @param region_id Region ID to read
 * @param region_offset Offset within the region
 * @param dst Destination pointer
 * @param size Read block size
 * @return size read from disk, -eAFV_verif on error
 */
extern ssize_t afv_checkpoint_read(afv_checkpoint_t *pCheckpoint,
                                   const uint64_t region_id,
                                   const size_t region_offset, void *dst,
                                   size_t size);

/**
 * @brief Write a block to a checkpoint
 *
 * assert(open=='W')
 *
 * @param pCheckpoint Checkpoint to write to
 * @param region_id Region ID to write to
 * @param region_offset Offset within the region to write to
 * @param src Source Pointer
 * @param size Size to write into disk
 * @return Size written to disk, -eAFV_verif on error
 */
extern ssize_t afv_checkpoint_write(afv_checkpoint_t *pCheckpoint,
                                    const uint64_t region_id,
                                    const void *const src, const size_t size);

/**
 * @brief Returns the current R/W head within the checkpoint region
 *
 * @param pCheckpoint Checkpoint to get the head of
 * @param pRegion_id Optional Return: ID of the region the head is pointing to
 * @return RW head location within the current region, -eAFV_verif on error
 */
extern ssize_t afv_checkpoint_head(afv_checkpoint_t *pCheckpoint,
                                   uint64_t *pRegion_id);

/**
 * @brief Verifies the contents of the checkpoint before writing
 *
 * @param pCheckpoint Checkpoint to Verify
 * @return
 */
extern eAFV_verif afv_checkpoint_verify(afv_checkpoint_t *pCheckpoint);

#endif
