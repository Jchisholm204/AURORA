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

#include <ucs/type/status.h>
#define ACI_INTERNAL

#include "aci/aci_internal.h"
#include "acn/acn.h"
#include "log.h"

#include <ucp/api/ucp.h>

union aurora_completion_notifier_memory {
    struct {
        uint64_t mem_key;
        uint64_t checkpoint_key;
        uint64_t checkpoint_version;
    };
    uint64_t data[8];
};

struct aurora_completion_notifier {
    ucp_rkey_h ucp_rkey;
    ucp_mem_h ucp_mem;
    ucp_ep_h ucp_owner_ep;
    union aurora_completion_notifier_memory *pLocal;
    union aurora_completion_notifier_memory *pRemote;
};

acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info) {
    acn_hndl *pHndl = malloc(sizeof(acn_hndl));
    if (!pHndl) {
        log_error("Failed to create ACN Handle");
        return NULL;
    }

    pHndl->pLocal = malloc(sizeof(union aurora_completion_notifier_memory));
    pHndl->pRemote = NULL;

    if (!pHndl->pLocal) {
        log_error("Failed to create ACN memory region");
        free(pHndl);
        return NULL;
    }

    ucp_mem_map_params_t mparam;
    mparam.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mparam.address = pHndl->pLocal;
    mparam.length = sizeof(union aurora_completion_notifier_memory);

    ucp_context_h ucp_ctx = aci_get_context();
    ucs_status_t ucs_status = ucp_mem_map(ucp_ctx, &mparam, &pHndl->ucp_mem);
    aci_release_context(ucp_ctx);
    if(ucs_status != UCS_OK){
        log_error("Failed to map ACN memory with UCP");
        acn_destroy_instance(&pHndl);
        return NULL;
    }

    return pHndl;
}

int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *conn_info) {
    return 0;
}

int acn_destroy_instance(acn_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }
    log_trace("Closing ACN");
    free(*ppHndl);
    *ppHndl = NULL;
    return 0;
}
