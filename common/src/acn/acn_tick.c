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
#include <unistd.h>

int _acn_aheadbehind(acn_hndl *pHndl, volatile uint64_t *ltick,
                     volatile uint64_t *rtick) {
    if (!pHndl || !ltick || !rtick) {
        return 0;
    }
    ucp_request_param_t rparam;
    rparam.op_attr_mask = 0;
    uint64_t remote_tick_read = 0;
    ucs_status_ptr_t ucs_pStatus = NULL;
    ucs_status_t ucs_status = UCS_INPROGRESS;
    ucs_pStatus =
        aci_get(pHndl->pACI, &remote_tick_read, sizeof(remote_tick_read),
                (uint64_t) rtick, pHndl->remote_rkey, &rparam);
    if (UCS_PTR_IS_ERR(ucs_pStatus)) {
        log_error("Remote Read Error: %s",
                  ucs_status_string(UCS_PTR_STATUS(ucs_pStatus)));
        return INT_MIN;
    } else if (UCS_PTR_IS_PTR(ucs_pStatus)) {
        while (ucs_status == UCS_INPROGRESS) {
            ucs_status = ucp_request_check_status(ucs_pStatus);
            aci_poll(pHndl->pACI);
        }
        ucp_request_free(ucs_pStatus);
    }
    if (ucs_status != UCS_OK) {
        log_error("Failed remote read: %s", ucs_status_string(ucs_status));
        return INT_MIN;
    }
    return (remote_tick_read - *ltick);
}

int _acn_await(acn_hndl *pHndl, volatile uint64_t *ltick,
               volatile uint64_t *rtick) {
    if (!pHndl || !ltick || !rtick) {
        return -1;
    }
    int aheadbehind = 0;
    int aheadbehind_last = 0;
    while ((aheadbehind = _acn_aheadbehind(pHndl, ltick, rtick)) != 0) {
        if (aheadbehind != aheadbehind_last) {
            log_debug("AheadBehind: %d", aheadbehind);
            aheadbehind_last = aheadbehind;
        }
        if (aheadbehind == INT_MIN) {
            return INT_MIN;
        }
        usleep(50000);
    }
    return 0;
}

int acn_tick_systick(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    pHndl->pLocal->systick++;
    return 0;
}

int acn_await_systick(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_await(pHndl, &pHndl->pLocal->systick, &pHndl->pRemote->systick);
}

int acn_aheadbehind_systick(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_aheadbehind(pHndl, &pHndl->pLocal->systick,
                            &pHndl->pRemote->systick);
}

int acn_tick_version(acn_hndl *pHndl, int64_t version) {
    if (!pHndl) {
        return -1;
    }
    pHndl->pLocal->checkpoint_version = version;
    return 0;
}

int acn_await_version(acn_hndl *pHndl, int64_t version) {
    if (!pHndl) {
        return -1;
    }
    pHndl->pLocal->checkpoint_version = version;
    return _acn_await(pHndl, (uint64_t *) &pHndl->pLocal->checkpoint_version,
                      (uint64_t *) &pHndl->pRemote->checkpoint_version);
}

int acn_aheadbehind_version(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_aheadbehind(pHndl,
                            (uint64_t *) &pHndl->pLocal->checkpoint_version,
                            (uint64_t *) &pHndl->pRemote->checkpoint_version);
}

int acn_tick_memory(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    pHndl->pLocal->mem_tick++;
    acn_tick_systick(pHndl);
    return 0;
}

int acn_await_memory(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_await(pHndl, (uint64_t *) &pHndl->pLocal->mem_tick,
                      (uint64_t *) &pHndl->pRemote->mem_tick);
}

int acn_aheadbehind_memory(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_aheadbehind(pHndl, (uint64_t *) &pHndl->pLocal->mem_tick,
                            (uint64_t *) &pHndl->pRemote->mem_tick);
}

int acn_tick_checkpoint(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    pHndl->pLocal->checkpoint_tick++;
    acn_tick_systick(pHndl);
    return 0;
}

int acn_await_checkpoint(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_await(pHndl, (uint64_t *) &pHndl->pLocal->checkpoint_tick,
                      (uint64_t *) &pHndl->pRemote->checkpoint_tick);
}

int acn_aheadbehind_checkpoint(acn_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    return _acn_aheadbehind(pHndl, (uint64_t *) &pHndl->pLocal->checkpoint_tick,
                            (uint64_t *) &pHndl->pRemote->checkpoint_tick);
}
