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
#define AFV_INTERNAL
#include "afv.h"

#include <stdio.h>

#define AFV_FNAME_LEN 256

struct aurora_file_versioning_file_handle
#ifdef AFV_INTERNAL
{
    char fname[AFV_FNAME_LEN];
    int64_t version;
    bool use_error_correction;
    afv_hndl *pAFV;
    FILE *pFile;
    size_t rw_ptr;
}
#endif
;

typedef struct aurora_file_versioning_file_handle afv_file_hndl;

extern afv_file_hndl *afv_file_open(afv_hndl *pAFV, int64_t version,
                                    const char *name);

extern int afv_file_close(afv_file_hndl **ppHndl);

extern int afv_file_seek(afv_file_hndl *pHndl, size_t seekptr);

extern int afv_file_jump(afv_file_hndl *pHndl, int64_t jsize);

extern int afv_file_write(afv_file_hndl *pHndl, void *data, size_t size);

extern int afv_file_read(afv_file_hndl *pHndl, void *data, size_t size);

#endif
