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
#include "log.h"

struct aurora_communication_interface_context _aci_ctx = {
    .refcount = 0,
    .ucp_ctx = NULL,
};

int _aci_init_context(void) {
    // Context already created
    if (_aci_ctx.ucp_ctx) {
        return 0;
    }
    ucs_status_t ucs_status;
    ucp_config_t *ucp_config;
    ucs_status = ucp_config_read(NULL, NULL, &ucp_config);
    if (ucs_status != UCS_OK) {
        log_error("Failed to read UCP Configuration: %s",
                  ucs_status_string(ucs_status));
        return -1;
    }

    ucp_params_t ucp_params;
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_AM | UCP_FEATURE_RMA | UCP_FEATURE_TAG;
    ucs_status = ucp_init(&ucp_params, ucp_config, &_aci_ctx.ucp_ctx);
    if (ucs_status != UCS_OK) {
        log_error("Failed to initialize UCP: %s",
                  ucs_status_string(ucs_status));
        return -1;
    }

    ucp_config_release(ucp_config);

    log_trace("UCP Context Created");
    return 0;
}

aci_hndl *aci_create_instance(aurora_blob_t *conn_info) {
    // Ensure the context was created and is active before continuing
    if (_aci_init_context() != 0 || !conn_info) {
        return NULL;
    }

    aci_hndl *pHndl = malloc(sizeof(aci_hndl));
    if (!pHndl) {
        log_error("Failed to create ACI handle");
        return NULL;
    }

    pHndl->ucp_worker = NULL;
    pHndl->ucp_ep = NULL;

    ucs_status_t ucs_status;

    ucp_worker_params_t worker_params;
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SERIALIZED;
    ucs_status =
        ucp_worker_create(_aci_ctx.ucp_ctx, &worker_params, &pHndl->ucp_worker);
    if (ucs_status != UCS_OK) {
        log_fatal("Failed to initialize UCP worker: %s",
                  ucs_status_string(ucs_status));
        free(pHndl);
        return NULL;
    }

    ucp_address_t *local_addr;
    size_t local_addr_len;

    ucs_status =
        ucp_worker_get_address(pHndl->ucp_worker, &local_addr, &local_addr_len);

    if (ucs_status != UCS_OK) {
        log_fatal("Failed to get worker address: %s",
                  ucs_status_string(ucs_status));
        ucp_worker_destroy(pHndl->ucp_worker);
        free(pHndl);
        return NULL;
    }

    conn_info->data = local_addr;
    conn_info->size = local_addr_len;

    _aci_ctx.refcount++;

    log_trace("ACI created, %d references", _aci_ctx.refcount);

    return pHndl;
}

int aci_connect_instance(aci_hndl *pHndl, aurora_blob_t *local_info,
                         aurora_blob_t *remote_info) {
    if (!pHndl || !local_info || !remote_info) {
        return -1;
    }

    if (!remote_info->data || remote_info->size == 0) {
        log_error("Remote address was NULL");
        return -2;
    }

    ucp_ep_params_t ep_params;
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = (const ucp_address_t *) remote_info->data;
    ucs_status_t ucs_status;
    ucs_status = ucp_ep_create(pHndl->ucp_worker, &ep_params, &pHndl->ucp_ep);
    if (ucs_status != UCS_OK) {
        log_error("Failed to create UCS endpoint");
        return -3;
    }

    ucp_worker_release_address(pHndl->ucp_worker, local_info->data);
    free(remote_info->data);

    log_trace("ACI connected");

    return 0;
}

int aci_destroy_instance(aci_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }

    // Decrement the context reference count
    _aci_ctx.refcount--;

    log_trace("Closing ACI.. %d references left", _aci_ctx.refcount);

    if ((*ppHndl)->ucp_ep) {
        ucp_request_param_t params = {0};
        // params.op_attr_mask = UCP_OP_ATTR_FIELD_REQUEST;
        ucp_ep_close_nbx((*ppHndl)->ucp_ep, &params);
    }
    if ((*ppHndl)->ucp_worker) {
        ucp_worker_destroy((*ppHndl)->ucp_worker);
    }
    free(*ppHndl);
    *ppHndl = NULL;

    if (_aci_ctx.refcount == 0) {
        ucp_cleanup(_aci_ctx.ucp_ctx);
        log_trace("Cleaned up UCP Context");
    }

    return 0;
}

int aci_poll(aci_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return ucp_worker_progress(pHndl->ucp_worker);
}

ucp_context_h aci_get_context(void) {
    if (_aci_init_context() != 0) {
        return NULL;
    }
    _aci_ctx.refcount++;
    return _aci_ctx.ucp_ctx;
}

void aci_release_context(ucp_context_h ctx) {
    if (ctx != NULL && ctx == _aci_ctx.ucp_ctx) {
        _aci_ctx.refcount--;
    }
}
