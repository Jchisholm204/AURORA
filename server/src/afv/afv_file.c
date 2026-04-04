/**
 * @file afv_file.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#define AFV_INTERNAL
#include "afv/afv_file.h"

#include "afv/afv.h"
#include "log.h"

#include <malloc.h>
#include <memory.h>

afv_file_hndl *afv_file_open_w(afv_hndl *pAFV, int64_t version,
                               const char *name) {
    if (!pAFV) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (!name) {
        log_error("NULL Parameter");
        return NULL;
    }
    if (version < 0) {
        log_error("NULL Parameter");
        return NULL;
    }

    afv_file_hndl *pHndl = malloc(sizeof(afv_file_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    // Setup this handle with the configuration from the AFV

    pHndl->version = version;
    pHndl->use_error_correction = pAFV->use_error_correction;
    pHndl->mode_w = true;
    snprintf(pHndl->fname, AFV_FNAME_LEN, "%s/%.*s-%lu-%lu-%lu.afvdat",
             pAFV->persistent_path, AFV_CKPT_NAME_LEN, name, version,
             pAFV->rank, pAFV->group_id);

    log_trace("Opening: %s", pHndl->fname);

    pHndl->pFile = fopen(pHndl->fname, "w");

    if (!pHndl->pFile) {
        log_error("File Error");
        free(pHndl);
        return NULL;
    }

    return pHndl;
}

afv_file_hndl *afv_file_open_r(afv_hndl *pAFV,
                               const afv_metadata_t *pMetadata) {
    if (!pAFV) {
        log_error("NULL Parameter");
        return NULL;
    }

    if (!pMetadata) {
        log_error("NULL Parameter");
        return NULL;
    }

    eAFV_verif afv_verif_status = afv_metadata_verify(pMetadata);

    if (afv_verif_status != eAFV_VERIF_OK) {
        log_error("Verification Error 0x%lx", afv_verif_status);
        return NULL;
    }

    afv_file_hndl *pHndl = malloc(sizeof(afv_file_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    // Setup this handle with the configuration from the metadata
    pHndl->version = pMetadata->version;
    pHndl->use_error_correction = pMetadata->ops.with_ec;
    pHndl->mode_w = false;
    snprintf(pHndl->fname, AFV_FNAME_LEN, "%s/%.*s-%lu-%lu-%lu.afvdat",
             pAFV->persistent_path, AFV_CKPT_NAME_LEN, pMetadata->chkpt_name,
             pMetadata->version, pMetadata->rank, pAFV->group_id);

    log_trace("Opening: %s", pHndl->fname);

    pHndl->pFile = fopen(pHndl->fname, "r");

    if (!pHndl->pFile) {
        log_error("File Error");
        free(pHndl);
        return NULL;
    }

    return pHndl;
}

eAFV_file_error afv_file_close(afv_file_hndl **ppHndl) {
    if (!ppHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    afv_file_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }

    if (pHndl->pFile) {
        fclose(pHndl->pFile);
    }

    pHndl->pFile = NULL;

    log_debug("Closed File: %s", pHndl->fname);

    return eAFV_FILE_OK;
}

eAFV_file_error afv_file_seek(afv_file_hndl *pHndl, size_t seekptr) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    fseek(pHndl->pFile, seekptr, SEEK_SET);

    return eAFV_FILE_OK;
}

eAFV_file_error afv_file_jump(afv_file_hndl *pHndl, int64_t jsize) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    fseek(pHndl->pFile, jsize, SEEK_CUR);

    return eAFV_FILE_OK;
}

eAFV_file_error afv_file_write(afv_file_hndl *pHndl, void *restrict data,
                               size_t size) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    size_t bytes_written = fwrite_unlocked(data, 1, size, pHndl->pFile);

    if (bytes_written != size) {
        return eAFV_FILE_ERR_RW;
    }
    return eAFV_FILE_OK;
}

eAFV_file_error afv_file_read(afv_file_hndl *pHndl, void *data, size_t size) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAFV_FILE_NULL;
    }
    size_t bytes_read = fread_unlocked(data, 1, size, pHndl->pFile);

    if (bytes_read != size) {
        return eAFV_FILE_ERR_RW;
    }
    return eAFV_FILE_OK;
}
