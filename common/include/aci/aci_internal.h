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

#define ACI_INTERNAL

#ifndef _ACI_INTERNAL_H_
#define _ACI_INTERNAL_H_
#ifdef ACI_INTERNAL

#include "ucp/api/ucp.h"

extern ucp_context_h _aci_context;

struct aurora_connection_instance {
    ucp_worker_h ucp_worker;
    ucp_ep_h ucp_ep;
};

#endif
#endif
