/**
 * @file aci.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Connection Instance
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACI_ACI_H_
#define _ACI_ACI_H_

#include "common_types.h"

#include <stdlib.h>

struct aurora_connection_instance;
typedef struct aurora_connection_instance aci_hndl;

extern aci_hndl *aci_create_instance(aurora_blob_t *conn_info);

extern int aci_connect_instance(aci_hndl *pHndl, aurora_blob_t *conn_info);

extern int aci_destroy_instance(aci_hndl **ppHndl);

#endif
