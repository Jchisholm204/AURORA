/**
 * @file afv_checkpoint.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-09-22
 * @modified Last Modified: 2026-09-22
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL

#include "afv/afv_checkpoint.h"
#include "afv/afv_metadata.h"

#include "log.h"

#include <assert.h>
#include <malloc.h>
#include <memory.h>

afv_checkpoint_t *_afv_checkpoint_malloc(const eAFVC_mode mode) {
    afv_checkpoint_t *pCheckpoint = malloc(sizeof(afv_checkpoint_t));
    if (!pCheckpoint) {
        log_error("Bad Alloc??");
        return NULL;
    }

    (void) memset(pCheckpoint, 0, sizeof(afv_checkpoint_t));

    pCheckpoint->file_mode = mode;
    pCheckpoint->region_id = (uint64_t) -1;

    return pCheckpoint;
}

afv_checkpoint_t *afv_checkpoint_open(afv_hndl *pAFV, int64_t version,
                                      const char *name) {
    if (!pAFV) {
        log_error("NULL Parameter");
        return NULL;
    }

    const afv_metadata_t *pMetadata = afv_read_metadata(pAFV, version, name);
    if (!pMetadata) {
        log_error("Unable to find metadata %v %s", version, name);
        return NULL;
    }

    afv_file_hndl *pFile = afv_file_open_r(pAFV, pMetadata);
    if (!pFile) {
        log_error("Unable to open checkpoint");
        afv_metadata_destroy((afv_metadata_t **) &pMetadata);
        return NULL;
    }

    afv_checkpoint_t *pCheckpoint = _afv_checkpoint_malloc(eAFVC_MODE_R);
    if (!pCheckpoint) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pCheckpoint->pAFV = pAFV;
    pCheckpoint->pMetadata = (afv_metadata_t *) pMetadata;
    pCheckpoint->pFile = pFile;

    // Increment AFV reference counter
    atomic_fetch_add(&pAFV->reference_counter, 1);

    return pCheckpoint;
}

afv_checkpoint_t *afv_checkpoint_create(afv_hndl *pAFV, int64_t version,
                                        const char name[AFV_CKPT_NAME_LEN],
                                        size_t n_regions) {
    if (!pAFV) {
        log_error("NULL Parameter");
        return NULL;
    }

    if (version < 0) {
        log_error("Invalid Checkpoint Version");
        return NULL;
    }

    afv_metadata_t *pMetadata = afv_metadata_create(n_regions);
    if (!pMetadata) {
        log_error("Bad Alloc");
        return NULL;
    }

    eAFV_verif metadata_status = eAFV_VERIF_OK;
    metadata_status |= afv_metadata_set_version(pMetadata, version);
    metadata_status |= afv_metadata_set_name(pMetadata, name);
    // Ignore size verification errors
    metadata_status &= ~eAFV_VERIF_ERR_SIZE;
    if (metadata_status != eAFV_VERIF_OK) {
        log_error("Metadata Verification Failure 0x%x", metadata_status);
        afv_metadata_destroy(&pMetadata);
        return NULL;
    }

    afv_file_hndl *pFile = afv_file_open_w(pAFV, version, name);
    if (!pFile) {
        log_error("Filesystem Error");
        afv_metadata_destroy(&pMetadata);
        return NULL;
    }

    afv_checkpoint_t *pCheckpoint = _afv_checkpoint_malloc(eAFVC_MODE_W);
    if (!pCheckpoint) {
        log_error("Bad Alloc??");
        afv_metadata_destroy(&pMetadata);
        afv_file_close(&pFile);
        return NULL;
    }

    pCheckpoint->pAFV = pAFV;
    pCheckpoint->pMetadata = pMetadata;
    pCheckpoint->pFile = pFile;

    // Increment AFV reference counter
    atomic_fetch_add(&pAFV->reference_counter, 1);

    return pCheckpoint;
}

eAFV_verif afv_checkpoint_close(afv_checkpoint_t **ppCheckpoint) {
    if (!ppCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    afv_checkpoint_t *pCheckpoint = *ppCheckpoint;
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }

    eAFV_verif afv_verif_status = afv_metadata_verify(pCheckpoint->pMetadata);
    if (afv_verif_status != eAFV_VERIF_OK) {
        log_error("AFV Verification Error 0x%x", afv_verif_status);
        return afv_verif_status;
    }

    eAFV_file_error afv_file_status = afv_file_close(&pCheckpoint->pFile);
    if (afv_file_status != eAFV_FILE_OK) {
        log_error("AFV File Error %d", afv_file_status);
        (void) afv_checkpoint_destroy(ppCheckpoint);
        return eAFV_VERIF_ERR_FATAL;
    }

    if (pCheckpoint->file_mode == eAFVC_MODE_W) {
        afv_verif_status =
            afv_write_metadata(pCheckpoint->pAFV, pCheckpoint->pMetadata);
        if (afv_verif_status != eAFV_VERIF_OK) {
            log_error("AFV Metadata Error %d", afv_verif_status);
            (void) afv_checkpoint_destroy(ppCheckpoint);
            return eAFV_VERIF_ERR_FATAL;
        }
    }

    // Decrement counter and destroy memory
    afv_checkpoint_destroy(ppCheckpoint);

    return eAFV_VERIF_OK;
}

void afv_checkpoint_destroy(afv_checkpoint_t **ppCheckpoint) {
    if (!ppCheckpoint) {
        return;
    }
    afv_checkpoint_t *pCheckpoint = *ppCheckpoint;
    if (!pCheckpoint) {
        return;
    }

    if (pCheckpoint->pFile) {
        (void) afv_file_close(&pCheckpoint->pFile);
    }

    if (pCheckpoint->pMetadata) {
        (void) afv_metadata_destroy(
            (afv_metadata_t **) &pCheckpoint->pMetadata);
    }

    // Decrement AFV reference counter
    atomic_fetch_sub(&pCheckpoint->pAFV->reference_counter, 1);

    free(pCheckpoint);
    *ppCheckpoint = NULL;
}

eAFV_verif afv_checkpoint_verify(afv_checkpoint_t *pCheckpoint) {
    if (!pCheckpoint) {
        log_error("NULL Parameter");
        return eAFV_VERIF_ERR_NULL;
    }
    if (!pCheckpoint->pMetadata) {
        log_error("ASSERT FAIL");
        return eAFV_VERIF_ERR_NULL | eAFV_VERIF_ERR_FATAL;
    }
    if (!pCheckpoint->pFile) {
        log_error("ASSERT FAIL");
        return eAFV_VERIF_ERR_NULL | eAFV_VERIF_ERR_FATAL;
    }
    eAFV_verif metadata_status = afv_metadata_verify(pCheckpoint->pMetadata);

    // Assign Critical Errors
    if (metadata_status & eAFV_VERIF_ERR_KEY) {
        log_error("Metadata Key Missing");
        metadata_status |= eAFV_VERIF_ERR_FATAL;
    }

    return metadata_status;
}
