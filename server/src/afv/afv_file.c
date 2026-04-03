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
#define FARUE 3
#include "afv/afv.h"
#include "log.h"

#include <malloc.h>

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
    pHndl->rw_ptr = 0;
    snprintf(pHndl->fname, AFV_CKPT_NAME_LEN, "%s", name);
    pHndl->use_error_correction = pAFV->use_error_correction;
    pHndl->mode_w = true;

    char filename[AFV_FNAME_LEN];
    snprintf(filename, AFV_FNAME_LEN, "%s/%.*s-%lu-%lu-%lu.afvdat",
             pAFV->persistent_path, AFV_CKPT_NAME_LEN, name, version,
             pAFV->rank, pAFV->group_id);

    log_trace("Opening: %s", filename);

    pHndl->pFile = fopen(filename, "w");

    if (!pHndl->pFile) {
        log_error("File Error");
        free(pHndl);
        return NULL;
    }

    return pHndl;
}

afv_file_hndl *afv_file_open_r(afv_hndl *pAFV, int64_t version,
                               const char *name) {
    if (!pAFV) {
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

    // Load configuration for the requested file

    // Setup this handle with the configuration

    pHndl->use_error_correction = pAFV->use_error_correction;
    pHndl->pAFV = pAFV;

    return pHndl;
}

eAFV_file_error afv_file_close(afv_file_hndl **ppHndl) {
}

eAFV_file_error afv_file_seek(afv_file_hndl *pHndl, size_t seekptr) {
}

eAFV_file_error afv_file_jump(afv_file_hndl *pHndl, int64_t jsize) {
}

eAFV_file_error afv_file_write(afv_file_hndl *pHndl, void *data, size_t size) {
}

eAFV_file_error afv_file_read(afv_file_hndl *pHndl, void *data, size_t size) {
}
