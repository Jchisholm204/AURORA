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

#include "aci/aci_callbacks.h"
#include "log.h"

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

    pHndl->status = UCS_OK;

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
    ep_params.field_mask =
        UCP_EP_PARAM_FIELD_REMOTE_ADDRESS | UCP_EP_PARAM_FIELD_ERR_HANDLER;
    ep_params.err_handler.arg = pHndl;
    ep_params.err_handler.cb = _aci_err_cb;
    ep_params.err_mode = UCS_ERR_ENDPOINT_TIMEOUT | UCS_ERR_CONNECTION_RESET;
    ep_params.address = (const ucp_address_t *) remote_info->data;
    ucs_status_t ucs_status;
    ucs_status = ucp_ep_create(pHndl->ucp_worker, &ep_params, &pHndl->ucp_ep);
    if (ucs_status != UCS_OK) {
        log_error("Failed to create UCS endpoint: %s",
                  ucs_status_string(ucs_status));
        return -3;
    }

    ucp_worker_release_address(pHndl->ucp_worker, local_info->data);
    free(remote_info->data);

    log_debug("ACI connected");

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

    ucs_status_ptr_t pUcs_status = NULL;
    ucp_request_param_t rparam;
    rparam.op_attr_mask = 0;
    pUcs_status = ucp_worker_flush_nbx((*ppHndl)->ucp_worker, &rparam);
    if (UCS_PTR_IS_PTR(pUcs_status)) {
        while (ucp_request_check_status(pUcs_status) == UCS_INPROGRESS) {
            aci_poll(*ppHndl);
        }
        ucp_request_free(pUcs_status);
    } else if (UCS_PTR_IS_ERR(pUcs_status)) {
        log_error("Flushing Worker Yeilded Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(pUcs_status)));
    }

    if ((*ppHndl)->ucp_ep) {
        ucp_request_param_t params = {0};
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
    if (pHndl->status != UCS_OK) {
        return pHndl->status;
    }
    (void) ucp_worker_progress(pHndl->ucp_worker);
    return 0;
}
