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

#include <stdatomic.h>
#include <ucp/api/ucp.h>

struct aurora_communication_interface_context {
    ucp_context_h ucp_ctx;
    volatile atomic_int refcount;
};

extern struct aurora_communication_interface_context _aci_ctx;

struct aurora_connection_instance {
    ucp_worker_h ucp_worker;
    ucp_ep_h ucp_ep;
};

#endif
#endif
