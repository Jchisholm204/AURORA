/**
 * @file aci.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Connection Instance
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACI_ACI_H_
#define _ACI_ACI_H_

#include "common_types.h"

#include <stdlib.h>
#ifdef ACI_INTERNAL
#include <stdatomic.h>
#include <ucp/api/ucp.h>
#endif

typedef struct aurora_connection_instance_hndl aci_hndl;

struct aurora_connection_instance_hndl
#ifdef ACI_INTERNAL
{
    ucp_worker_h ucp_worker;
    ucp_ep_h ucp_ep;
    ucs_status_t status;
    _Atomic int worker_in_use;
}
#endif
;

#ifdef ACI_INTERNAL
struct aurora_communication_interface_context {
    ucp_context_h ucp_ctx;
    volatile atomic_int refcount;
};
extern struct aurora_communication_interface_context _aci_ctx;
extern int _aci_init_context(void);
#endif

extern aci_hndl *aci_create_instance(aurora_blob_t *conn_info);

extern int aci_connect_instance(aci_hndl *pHndl, aurora_blob_t *local_info,
                                aurora_blob_t *remote_info);

/**
 * @brief Disconnect the AC-Instance (destroy UCP EP)
 *
 * @param pHndl
 * @return
 */
extern int aci_disconnect_instance(aci_hndl *pHndl);

/**
 * @brief Destroy all memory and connections for an ac-instance (destroy UCP
 * Worker) aci_disconnect_instance must be called prior to this function
 * @param ppHndl
 * @return err or 0 (success)
 */
extern int aci_destroy_instance(aci_hndl **ppHndl);

/**
 * @brief Poll the UCP progress worker (ALLWAYS, Even on ERROR)
 *
 * @param pHndl Endpoint handle to poll
 * @return Returns the latest UCS status from the endpoint (Return Fatal if not
 * 0)
 */
extern int aci_poll(aci_hndl *pHndl);
extern int aci_wait(aci_hndl *pHndl);

extern void aci_keepalive(bool enable);

#endif
