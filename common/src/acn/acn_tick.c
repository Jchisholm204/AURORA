/**
 * @file acn_tick.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-03-03
 * @modified Last Modified: 2026-03-03
 *
 * @copyright Copyright (c) 2026
 */

#define ACN_INTERNAL
#include "aci/aci_internal.h"
#include "acn/acn.h"
#include "log.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>

#define CHECK_NOTIF(notif)                                                     \
    (__builtin_clzll(notif) < __builtin_clzll(eACN_Nnotifications))

#define ACN_POLL_TIMEOUT_COUNT 5

eACN_error _acn_loadmem(acn_hndl *pHndl) {
    if (!pHndl) {
        return eACN_ERR_NULL;
    }
    ucp_request_param_t rparam;
    rparam.op_attr_mask = UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
    ucs_status_t ucs_status = UCS_INPROGRESS;

    // Operation lock the request pointer (one pending at a time)
    // All operations are the same -> reuse old operation if present
    // assert(pHndl->ucs_pRequest == NULL, Initialization)
    // -> Only send new requests when one is not presently active
    if (pHndl->ucs_pRequest == NULL) {
        pHndl->ucs_pRequest =
            aci_get(pHndl->pACI, (void *) &pHndl->temp_memory,
                    sizeof(pHndl->temp_memory), (uint64_t) pHndl->pRemote,
                    pHndl->remote_rkey, &rparam);
    }

    if (UCS_PTR_IS_ERR(pHndl->ucs_pRequest)) {
        log_error("Remote Read Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(pHndl->ucs_pRequest)));
        pHndl->ucs_pRequest = NULL;
        return eACN_ERR_UCS;
    } else if (UCS_PTR_IS_PTR(pHndl->ucs_pRequest)) {
        size_t poll_count = 0;
        while (ucs_status == UCS_INPROGRESS) {
            ucs_status = ucp_request_check_status(pHndl->ucs_pRequest);
            int aci_status = 0;
            aci_status = aci_poll(pHndl->pACI);
            if (aci_status != 0) {
                return eACN_ERR_FATAL;
            }
            poll_count++;
            if (poll_count > ACN_POLL_TIMEOUT_COUNT) {
                return eACN_ERR_TIMEOUT;
            }
            usleep(10);
        }
        ucp_request_free(pHndl->ucs_pRequest);
        pHndl->ucs_pRequest = NULL;
    }

    if (ucs_status != UCS_OK) {
        log_error("Failed remote read: %s", ucs_status_string(ucs_status));
        return eACN_ERR_UCS;
    }

    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    return eACN_OK;
}

int acn_tick(acn_hndl *pHndl, eACN_notification notifs) {
    if (!pHndl || CHECK_NOTIF(notifs)) {
        return -1;
    }
    for (size_t i = 0; notifs != 0;) {
        // Get number of lowest bit
        i = __builtin_ctzll(notifs);

        // Increment tick i
        pHndl->pLocal->data[i]++;

        // Clear the lowest bit
        notifs &= (notifs - 1);
    }
    return 0;
}

eACN_error acn_await(acn_hndl *pHndl, eACN_notification notifs) {
    if (!pHndl || CHECK_NOTIF(notifs)) {
        return eACN_ERR_NULL;
    }
    for (size_t i = 0; notifs != 0;) {
        // Load the latest memory chunk
        eACN_error mem_err;
        if ((mem_err = _acn_loadmem(pHndl)) != eACN_OK) {
            return mem_err;
        }

        // Get number of lowest bit
        i = __builtin_ctzll(notifs);

        // Clear lowest bit when remote equals local
        if (pHndl->temp_memory.data[i] == pHndl->pLocal->data[i]) {
            notifs &= ~BIT(i);
        }

        // Don't flood the PCIe bus
        (void) usleep(1000);
    }
    return eACN_OK;
}

int acn_aheadbehind(acn_hndl *pHndl, eACN_notification notifs) {
    if (!pHndl || CHECK_NOTIF(notifs)) {
        return INT_MIN;
    }
    // Load the latest memory chunk
    int mem_err = 0;
    if ((mem_err = _acn_loadmem(pHndl)) != 0) {
        // Do not return on error, use the last one if needed
    }

    int diff = 0;

    for (size_t i = 0; notifs != 0;) {
        // Get number of lowest bit
        i = __builtin_ctzll(notifs);

        diff += pHndl->temp_memory.data[i] - pHndl->pLocal->data[i];

        // Clear the lowest bit
        notifs &= ~(notifs - 1);
    }
    return diff;
}

int acn_set(acn_hndl *pHndl, eACN_notification notif, const uint64_t value) {
    if (!pHndl || CHECK_NOTIF(notif)) {
        return -1;
    }
    uint i = __builtin_ctzll(notif);

    pHndl->pLocal->data[i] = value;
    return 0;
}

eACN_error acn_get(acn_hndl *pHndl, eACN_notification notif, uint64_t *pValue) {
    if (!pHndl || CHECK_NOTIF(notif)) {
        return 0;
    }
    // Load the latest memory chunk
    eACN_error mem_err;
    if ((mem_err = _acn_loadmem(pHndl)) != 0) {
        return mem_err;
    }
    uint i = __builtin_ctzll(notif);

    *pValue = pHndl->temp_memory.data[i];

    return 0;
}

eACN_error acn_set_name(acn_hndl *pHndl, const char name[static ACN_NAME_LEN]) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eACN_ERR_NULL;
    }
    if (!pHndl->pLocal) {
        log_fatal("NULL Parameter");
        return eACN_ERR_NULL;
    }
    memcpy((char *) &pHndl->pLocal->name, name, ACN_NAME_LEN);
    return eACN_OK;
}

eACN_error acn_get_name(acn_hndl *pHndl, char name[static ACN_NAME_LEN]) {
    // Load the latest memory chunk
    eACN_error mem_err;
    if ((mem_err = _acn_loadmem(pHndl)) != 0) {
        return mem_err;
    }
    memcpy(name, (char *) pHndl->temp_memory.name, ACN_NAME_LEN);
    return eACN_OK;
    return eACN_ERR_NULL;
}

eACN_error acn_check(acn_hndl *pHndl, eACN_notification *pNotifs) {
    if (!pHndl || !pNotifs) {
        return 0;
    }
    // Check the UCS Connection Status
    int aci_status = aci_poll(pHndl->pACI);
    if (aci_status == UCS_ERR_CONNECTION_RESET) {
        return eACN_ERR_FATAL;
    } else if (aci_status != UCS_OK) {
        return eACN_ERR_UCS;
    }

    // Load the latest memory chunk
    eACN_error mem_err;
    if ((mem_err = _acn_loadmem(pHndl)) != 0) {
        return mem_err;
    }
    *pNotifs = 0;
    uint notifs = (eACN_Nnotifications - 1);
    for (size_t i = 0; notifs != 0;) {
        // Get number of lowest bit
        i = __builtin_ctzll(notifs);

        if (pHndl->temp_memory.data[i] != pHndl->pLocal->data[i]) {
            *pNotifs |= BIT(i);
        }

        notifs = notifs >> 1;
    }

    return eACN_OK;
}
