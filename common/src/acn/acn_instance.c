/**
 * @file acn_instance.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#define ACN_INTERNAL
#include "aci/aci_internal.h"
#include "acn/acn.h"
#include "log.h"

#include <memory.h>
#include <ucs/type/status.h>

acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info) {
    if (!pACI || !conn_info) {
        log_error("ACN create instance called with NULL params");
        return NULL;
    }

    acn_hndl *pHndl = malloc(sizeof(acn_hndl));
    if (!pHndl) {
        log_error("Failed to create ACN Handle");
        return NULL;
    }
    // Reference the base ACI handle for UCP operations
    pHndl->pACI = pACI;

    // Setup Local/Remote
    pHndl->pLocal = malloc(sizeof(union aurora_completion_notifier_memory));
    pHndl->pRemote = NULL;
    pHndl->remote_rkey = NULL;

    if (!pHndl->pLocal) {
        log_error("Failed to create ACN memory region");
        free(pHndl);
        return NULL;
    } else {
        memset(pHndl->pLocal, 0,
               sizeof(union aurora_completion_notifier_memory));
    }

    ucp_mem_map_params_t mparam;
    mparam.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mparam.address = pHndl->pLocal;
    mparam.length = sizeof(union aurora_completion_notifier_memory);

    ucs_status_t ucs_status = UCS_OK;
    ucs_status = aci_mem_map(pACI, &mparam, &pHndl->local_mem_hndl);
    if (ucs_status != UCS_OK) {
        log_error("ACN Memmap failed.");
        free(pHndl->pLocal);
        free(pHndl);
        return NULL;
    }

    void *rkey_buf = NULL;
    ucs_status =
        aci_rkey_pack(pACI, pHndl->local_mem_hndl, &rkey_buf, &conn_info->size);
    if (ucs_status != UCS_OK) {
        log_error("Error packing rkey: %s", ucs_status_string(ucs_status));
        (void) acn_destroy_instance(&pHndl);
        return NULL;
    }

    // Realloc the rkey to make the swap data include the addr
    conn_info->data = (uint64_t *) malloc(conn_info->size + sizeof(uint64_t));
    if (!conn_info->data) {
        log_error("Failed to allocate rkey");
        ucp_rkey_buffer_release(rkey_buf);
        (void) acn_destroy_instance(&pHndl);
        return NULL;
    }

    // Put the local address into the buffer
    ((uint64_t *) conn_info->data)[0] = (uint64_t) pHndl->pLocal;
    // Copy the rkey into the remaining space
    (void) memcpy(&(((uint64_t *) conn_info->data)[1]), rkey_buf,
                  conn_info->size);

    log_trace("Releasing RKEY");

    // Release the UCP rkey
    ucp_rkey_buffer_release(rkey_buf);

    // Adjust size of rkey buffer to include address
    conn_info->size += sizeof(uint64_t);
    log_trace("Total Exchange size = %ld", conn_info->size);

    pHndl->ucs_pRequest = NULL;

    aci_poll(pACI);

    return pHndl;
}

eACN_error acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *local_info,
                                aurora_blob_t *remote_info) {
    if (!pHndl || !local_info || !remote_info) {
        log_error("Connect instance called with NULL params");
        return eACN_ERR_NULL;
    }

    ucs_status_t ucs_status = UCS_OK;

    // Free memory allocated by create instance
    if (!local_info->data || local_info->size == 0) {
        log_error("Local Info NULL.. Unable to connect ACN");
        return eACN_ERR_NULL;
    }

    // Setup the remote notification instance
    if (!remote_info->data || remote_info->size == 0) {
        log_error("Remote Info NULL.. Unable to connect ACN");
        return eACN_ERR_NULL;
    }

    memcpy(&pHndl->pRemote, remote_info->data, sizeof(uint64_t));

    ucs_status =
        aci_rkey_unpack(pHndl->pACI,
                        (uint8_t *) remote_info->data + sizeof(uint64_t),
                        &pHndl->remote_rkey);
    if (ucs_status != UCS_OK) {
        log_error("Error unpacking ACN RKEY: %s",
                  ucs_status_string(ucs_status));
        return eACN_ERR_UCS;
    }

    // Free Remote Data
    free(remote_info->data);
    remote_info->data = NULL;

    // Free Local Data
    free(local_info->data);
    local_info->data = NULL;

    log_debug("ACN Connected");

    return eACN_OK;
}

eACN_error acn_destroy_instance(acn_hndl **ppHndl) {
    if (!ppHndl) {
        return eACN_ERR_NULL;
    }
    if (!*ppHndl) {
        return eACN_ERR_NULL;
    }
    log_trace("Closing ACN");

    ucs_status_t ucs_status = UCS_OK;
    if ((*ppHndl)->ucs_pRequest) {
        aci_request_cancel((*ppHndl)->pACI, (*ppHndl)->ucs_pRequest);
        ucs_status = UCS_INPROGRESS;
        while (ucs_status == UCS_INPROGRESS) {
            // Pray that this closes..
            (void) aci_poll((*ppHndl)->pACI);
            ucs_status = ucp_request_check_status((*ppHndl)->ucs_pRequest);
        }
        ucp_request_free((*ppHndl)->ucs_pRequest);
        (*ppHndl)->ucs_pRequest = NULL;
    }

    // 2. Free RKEY
    if ((*ppHndl)->remote_rkey) {
        ucp_rkey_destroy((*ppHndl)->remote_rkey);
    }

    // 3. Unmap memory
    if ((*ppHndl)->local_mem_hndl) {
        ucs_status = aci_mem_unmap((*ppHndl)->pACI, (*ppHndl)->local_mem_hndl);
    }

    if (ucs_status != UCS_OK) {
        log_error("Failed to munmap ACN memory %s",
                  ucs_status_string(ucs_status));
    }

    // 4. Free whatever is left over or internally allocated
    if ((*ppHndl)->pLocal) {
        free((*ppHndl)->pLocal);
    }

    free(*ppHndl);
    *ppHndl = NULL;
    return eACN_OK;
}
