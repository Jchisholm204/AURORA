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

#define ACN_INTERNAL
#include <ucs/type/status.h>
#include "aci/aci_internal.h"
#include "acn/acn.h"
#include "log.h"

#include <memory.h>

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
        memset(&pHndl->local_private, 0,
               sizeof(union aurora_completion_notifier_memory));
        pHndl->local_private.tick = 1;
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

    log_debug("Releasing RKEY");

    // Release the UCP rkey
    ucp_rkey_buffer_release(rkey_buf);

    // Adjust size of rkey buffer to include address
    conn_info->size += sizeof(uint64_t);
    log_debug("Total Exchange size = %ld", conn_info->size);

    aci_poll(pACI);

    return pHndl;
}

int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *local_info,
                         aurora_blob_t *remote_info) {
    if (!pHndl || !local_info || !remote_info) {
        log_error("Connect instance called with NULL params");
        return -1;
    }

    ucs_status_t ucs_status = UCS_OK;

    // Free memory allocated by create instance
    if (!local_info->data || local_info->size == 0) {
        log_error("Local Info NULL.. Unable to connect ACN");
        return -1;
    }

    // Setup the remote notification instance
    if (!remote_info->data || remote_info->size == 0) {
        log_error("Remote Info NULL.. Unable to connect ACN");
        return -1;
    }

    memcpy(&pHndl->pRemote, remote_info->data, sizeof(uint64_t));

    ucs_status =
        aci_rkey_unpack(pHndl->pACI,
                        (uint8_t *) remote_info->data + sizeof(uint64_t),
                        &pHndl->remote_rkey);
    if (ucs_status != UCS_OK) {
        log_error("Error unpacking ACN RKEY: %s",
                  ucs_status_string(ucs_status));
        return -1;
    }

    // Free Remote Data
    free(remote_info->data);
    remote_info->data = NULL;

    // Free Local Data
    free(local_info->data);
    local_info->data = NULL;

    log_debug("ACN Connected");

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

    ucs_status_t ucs_status = UCS_OK;
    if ((*ppHndl)->local_mem_hndl) {
        ucs_status = aci_mem_unmap((*ppHndl)->pACI, (*ppHndl)->local_mem_hndl);
    }
    if (ucs_status != UCS_OK) {
        log_error("Failed to munmap ACN memory %s",
                  ucs_status_string(ucs_status));
    }

    if ((*ppHndl)->pLocal) {
        free((*ppHndl)->pLocal);
    }

    if ((*ppHndl)->remote_rkey) {
        ucp_rkey_destroy((*ppHndl)->remote_rkey);
    }

    free(*ppHndl);
    *ppHndl = NULL;
    return 0;
}
