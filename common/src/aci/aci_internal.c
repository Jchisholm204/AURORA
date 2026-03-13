/**
 * @file aci_internal.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-03-02
 * @modified Last Modified: 2026-03-02
 *
 * @copyright Copyright (c) 2026
 */

#define ACI_INTERNAL

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

ucs_status_t aci_mem_map(aci_hndl *pHndl, const ucp_mem_map_params_t *params,
                         ucp_mem_h *pMemh) {
    if (!pHndl || _aci_ctx.ucp_ctx == NULL) {
        log_error("Memmap called with null handle");
        return UCS_ERR_NO_RESOURCE;
    }

    return ucp_mem_map(_aci_ctx.ucp_ctx, params, pMemh);
}

ucs_status_t aci_mem_unmap(aci_hndl *pHndl, ucp_mem_h memh) {
    if (!pHndl || _aci_ctx.ucp_ctx == NULL) {
        log_error("Mem unmap called with null handle");
        return UCS_ERR_NO_RESOURCE;
    }
    return ucp_mem_unmap(_aci_ctx.ucp_ctx, memh);
}

ucs_status_t aci_rkey_pack(aci_hndl *pHndl, ucp_mem_h memh, void **pRkey_buffer,
                           size_t *pSize) {
    if (!pHndl || _aci_ctx.ucp_ctx == NULL) {
        log_error("rkey pack called with null handle");
        return UCS_ERR_NO_RESOURCE;
    }
    return ucp_rkey_pack(_aci_ctx.ucp_ctx, memh, pRkey_buffer, pSize);
}

ucs_status_t aci_rkey_unpack(aci_hndl *pHndl, const void *rkey_buffer,
                             ucp_rkey_h *pRkey) {
    if (!pHndl || _aci_ctx.ucp_ctx == NULL) {
        log_error("rkey unpack called with null handle");
        return UCS_ERR_NO_RESOURCE;
    }
    return ucp_ep_rkey_unpack(pHndl->ucp_ep, rkey_buffer, pRkey);
}

ucs_status_ptr_t aci_put(aci_hndl *pHndl, const void *buffer, size_t count,
                         uint64_t remote_addr, ucp_rkey_h rkey,
                         const ucp_request_param_t *param) {
    if (!pHndl) {
        log_error("aci put called with null handle");
        return NULL;
    }
    return ucp_put_nbx(pHndl->ucp_ep, buffer, count, remote_addr, rkey, param);
}

ucs_status_ptr_t aci_get(aci_hndl *pHndl, void *buffer, size_t count,
                         uint64_t remote_addr, ucp_rkey_h rkey,
                         const ucp_request_param_t *param) {
    if (!pHndl) {
        log_error("aci get called with null handle");
        return NULL;
    }
    return ucp_get_nbx(pHndl->ucp_ep, buffer, count, remote_addr, rkey, param);
}

void aci_request_cancel(aci_hndl *pHndl, void *request) {
    if (!pHndl) {
        log_error("aci request cancel called with null handle");
        return;
    }
    ucp_request_cancel(pHndl->ucp_worker, request);
}
