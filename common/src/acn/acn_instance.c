/**
 * @file acn_instance.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#include "acn/acn.h"
#include "log.h"
#include <ucp/api/ucp.h>

struct aurora_completion_notifier {
    ucp_rkey_h rkey;
};

acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info) {
    acn_hndl * pHndl = malloc(sizeof(acn_hndl));
    if(!pHndl){
        log_error("Failed to create ACN Handle");
        return NULL;
    }

    return pHndl;
}

int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *conn_info) {
    return 0;
}

int acn_destroy_instance(acn_hndl **ppHndl) {
    if(!ppHndl){
        return -1;
    }
    if(!*ppHndl){
        return -1;
    }
    log_trace("Closing ACN");
    free(*ppHndl);
    *ppHndl = NULL;
    return 0;
}
