/**
 * @file aci_callbacks.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-03-12
 * @modified Last Modified: 2026-03-12
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACI_CALLBACKS_H_
#define _ACI_CALLBACKS_H_
#ifdef ACI_INTERNAL
#include <ucp/api/ucp.h>

void _aci_err_cb(void *arg, ucp_ep_h ep, ucs_status_t status);

#endif
#endif
