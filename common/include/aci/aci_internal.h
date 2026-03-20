/**
 * @file aci_internal.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACI_INTERNAL_H_
#define _ACI_INTERNAL_H_

#include "aci.h"

#include <stdatomic.h>
#include <ucp/api/ucp.h>

ucs_status_t aci_mem_map(aci_hndl *pHndl, const ucp_mem_map_params_t *params,
                         ucp_mem_h *pMemh);

ucs_status_t aci_mem_unmap(aci_hndl *pHndl, ucp_mem_h memh);

ucs_status_t aci_rkey_pack(aci_hndl *pHndl, ucp_mem_h memh, void **pRkey_buffer,
                           size_t *pSize);

ucs_status_t aci_rkey_unpack(aci_hndl *pHndl, const void *rkey_buffer,
                             ucp_rkey_h *pRkey);

ucs_status_ptr_t aci_put(aci_hndl *pHndl, const void *buffer, size_t count,
                         uint64_t remote_addr, ucp_rkey_h rkey,
                         const ucp_request_param_t *param);

ucs_status_ptr_t aci_get(aci_hndl *pHndl, void *buffer, size_t count,
                         uint64_t remote_addr, ucp_rkey_h rkey,
                         const ucp_request_param_t *param);

void aci_request_cancel(aci_hndl *pHndl, void *request);

ucs_status_t aci_set_am_recv_handler(aci_hndl *pHndl,
                                     const ucp_am_handler_param_t *params);

#endif
