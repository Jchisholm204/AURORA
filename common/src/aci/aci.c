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
    atomic_store(&pHndl->worker_in_use, 0);

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
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                           UCP_EP_PARAM_FIELD_ERR_HANDLER |
                           UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE;
    ep_params.err_handler.arg = pHndl;
    ep_params.err_handler.cb = _aci_err_cb;
    ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
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

    log_trace("ACI connected");

    return 0;
}

int aci_disconnect_instance(aci_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }

    log_trace("Disconnecting Instance");

    ucs_status_ptr_t ucs_pStatus = NULL;
    // Request a close if the ep is open
    if (pHndl->ucp_ep) {
        ucp_request_param_t ucp_request_param = {0};
        ucp_request_param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
        ucp_request_param.flags = UCP_EP_CLOSE_FLAG_FORCE;
        ucs_pStatus = ucp_ep_close_nbx(pHndl->ucp_ep, &ucp_request_param);
    }

    // Wait for the ep to get shut down
    if (UCS_PTR_IS_PTR(ucs_pStatus)) {
        while (ucp_request_check_status(ucs_pStatus) == UCS_INPROGRESS) {
            (void) aci_poll(pHndl);
        }
        ucp_request_free(ucs_pStatus);
    } else if (UCS_PTR_IS_ERR(ucs_pStatus)) {
        log_error("Error closing endpoint: %s",
                  ucs_status_string(UCS_PTR_STATUS(ucs_pStatus)));
    }

    for (int i = 0; i < 1000; i++) {
        while (ucp_worker_progress(pHndl->ucp_worker) > 0)
            ;
    }

    // Set endpoint to null
    pHndl->ucp_ep = NULL;

    return 0;
}

int aci_destroy_instance(aci_hndl **ppHndl) {
    if (!ppHndl) {
        return -1;
    }
    if (!*ppHndl) {
        return -1;
    }

    if (atomic_exchange(&(*ppHndl)->worker_in_use, 1) == 1) {
        log_fatal("THREAD COLLISION");
    }

    // Endpoint should be cleared and null by now (err if not)
    if ((*ppHndl)->ucp_ep) {
        // May segfault if the instance was not yet disconnected by higher level
        // software.. That is the users fault
        log_debug("USAGE: ACI Disconnect must be called prior");
        log_debug("Attempting to clean up for the user...");
        (void) aci_disconnect_instance(*ppHndl);
    }

    // Decrement the context reference count
    _aci_ctx.refcount--;

    log_trace("Closing ACI.. %d references left", _aci_ctx.refcount);

    if ((*ppHndl)->ucp_worker) {
        ucp_worker_destroy((*ppHndl)->ucp_worker);
    }
    free(*ppHndl);
    *ppHndl = NULL;

    if (_aci_ctx.refcount == 0) {
        ucp_cleanup(_aci_ctx.ucp_ctx);
        _aci_ctx.ucp_ctx = NULL;
        log_trace("Cleaned up UCP Context");
    }

    return 0;
}

int aci_poll(aci_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    if (atomic_exchange(&pHndl->worker_in_use, 1) == 1) {
        log_fatal("THREAD COLLISION");
    }
    (void) ucp_worker_progress(pHndl->ucp_worker);
    atomic_store(&pHndl->worker_in_use, 0);
    return pHndl->status;
}

int aci_wait(aci_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    if (atomic_exchange(&pHndl->worker_in_use, 1) == 1) {
        log_fatal("THREAD COLLISION");
    }
    pHndl->status = ucp_worker_wait(pHndl->ucp_worker);
    atomic_store(&pHndl->worker_in_use, 0);
    return pHndl->status;
}

void aci_keepalive(bool enable) {
    if (enable) {
        _aci_init_context();
        _aci_ctx.refcount++;
        log_debug("Keeping ACI context alive");
    } else {
        if (_aci_ctx.refcount >= 1) {
            _aci_ctx.refcount--;
        }
        log_debug("ACI context keepalive disabled.. %d refs remaining",
                  _aci_ctx.refcount);
    }
    if (_aci_ctx.refcount == 0) {
        ucp_cleanup(_aci_ctx.ucp_ctx);
        _aci_ctx.ucp_ctx = NULL;
        log_trace("Cleaned up UCP Context (keepalive)");
    }
}
