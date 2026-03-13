/**
 * @file aci_callbacks.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-03-12
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#define ACI_INTERNAL
#include "aci/aci_callbacks.h"

#include "aci/aci.h"
#include "log.h"

void _aci_err_cb(void *arg, ucp_ep_h ep, ucs_status_t status) {
    aci_hndl *pHndl = arg;
    if (!pHndl) {
        log_fatal("ACI Err Handler NULL Handle");
    }
    pHndl->status = status;
    log_error("%s", ucs_status_string(status));
}
