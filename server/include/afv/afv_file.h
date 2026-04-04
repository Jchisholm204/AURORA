/**
 * @file afv_file.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-02
 * @modified Last Modified: 2026-04-02
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_FILE_H_
#define _AFV_FILE_H_
#include "afv.h"

#include <stdio.h>

#define AFV_FNAME_LEN 512

enum aurora_file_versioning_file_error_e {
    eAFV_FILE_OK,
    eAFV_FILE_NULL,
    eAFV_FILE_ERR_RW,
};

struct aurora_file_versioning_file_handle
#ifdef AFV_INTERNAL
#define FARUE
{
    int64_t version;
    FILE *pFile;
    char fname[AFV_FNAME_LEN];
    bool use_error_correction;
    bool mode_w;
}
#endif
;

typedef struct aurora_file_versioning_file_handle afv_file_hndl;
typedef enum aurora_file_versioning_file_error_e eAFV_file_error;

extern afv_file_hndl *afv_file_open_w(afv_hndl *pAFV, int64_t version,
                                      const char *name);

extern afv_file_hndl *afv_file_open_r(afv_hndl *pAFV,
                                      const afv_metadata_t *pMetadata);

extern eAFV_file_error afv_file_close(afv_file_hndl **ppHndl);

extern eAFV_file_error afv_file_seek(afv_file_hndl *pHndl, size_t seekptr);

extern eAFV_file_error afv_file_jump(afv_file_hndl *pHndl, int64_t jsize);

extern eAFV_file_error afv_file_write(afv_file_hndl *pHndl, const void *restrict data,
                                      size_t size);

extern eAFV_file_error afv_file_read(afv_file_hndl *pHndl, void *data,
                                     size_t size);

#endif
