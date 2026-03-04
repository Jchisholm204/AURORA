/**
 * @file acn.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACN_ACN_H_
#define _ACN_ACN_H_

#include "aci/aci.h"

#include <stdint.h>
#ifdef ACN_INTERNAL
#include <ucp/api/ucp.h>
union aurora_completion_notifier_memory {
    struct {
        volatile uint64_t mem_tick;
        volatile uint64_t checkpoint_tick;
        volatile int64_t checkpoint_version;
        volatile uint64_t systick;
    };
    volatile uint64_t data[8];
};
#endif

struct aurora_completion_notifier
#ifdef ACN_INTERNAL
{
    // ACI handle
    aci_hndl *pACI;
    // Remote Notification
    ucp_rkey_h remote_rkey;
    union aurora_completion_notifier_memory *pRemote;
    // Local Notification
    ucp_mem_h local_mem_hndl;
    union aurora_completion_notifier_memory *volatile pLocal;
}
#endif
;

typedef struct aurora_completion_notifier acn_hndl;

extern acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info);

extern int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *local_info,
                                aurora_blob_t *remote_info);

extern int acn_destroy_instance(acn_hndl **ppHndl);

extern int acn_tick_systick(acn_hndl *pHndl);
extern int acn_await_systick(acn_hndl *pHndl);
extern int acn_aheadbehind_systick(acn_hndl *pHndl);

extern int acn_tick_version(acn_hndl *pHndl, int64_t version);
extern int acn_await_version(acn_hndl *pHndl, int64_t version);
extern int acn_aheadbehind_version(acn_hndl *pHndl);

extern int acn_tick_memory(acn_hndl *pHndl);
extern int acn_await_memory(acn_hndl *pHndl);
extern int acn_aheadbehind_memory(acn_hndl *pHndl);

extern int acn_tick_checkpoint(acn_hndl *pHndl);
extern int acn_await_checkpoint(acn_hndl *pHndl);
extern int acn_aheadbehind_checkpoint(acn_hndl *pHndl);

#endif
