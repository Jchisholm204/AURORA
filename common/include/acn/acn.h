/**
 * @file acn.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACN_ACN_H_
#define _ACN_ACN_H_

#include "aci/aci.h"

struct aurora_completion_notifier;
typedef struct aurora_completion_notifier acn_hndl;

extern acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info);

extern int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *conn_info);

extern int acn_destroy_instance(acn_hndl **ppHndl);

#endif
