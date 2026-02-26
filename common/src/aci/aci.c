/**
 * @file aci.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#define ACI_INTERNAL

#include "aci/aci.h"

#include "aci/aci_internal.h"

ucp_context_h _aci_context = NULL;

aci_hndl *aci_create_instance(aurora_blob_t *conn_info) {
    aci_hndl *pHndl = malloc(sizeof(aci_hndl));
    if(!pHndl){
        return NULL;
    }
    return pHndl;
}

int aci_connect_instance(aci_hndl *pHndl, aurora_blob_t *conn_info) {
}

int aci_destroy_instance(aci_hndl **ppHndl) {
}
