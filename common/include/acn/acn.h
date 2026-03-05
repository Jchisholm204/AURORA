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

#ifndef BIT
#define BIT(x) ((1) << (x))
#endif

enum aurora_completion_notification_e {
    eACN_systick = BIT(0),
    eACN_memory = BIT(1),
    eACN_checkpoint = BIT(2),
    eACN_restore = BIT(3),
    eACN_version = BIT(4),

    // Final member
    eACN_Nnotifications,
};

#include <stdint.h>
#ifdef ACN_INTERNAL
#include <ucp/api/ucp.h>
// Keep this memory cache aligned
// This is in a critical path on the server side
union aurora_completion_notifier_memory {
    struct {
        volatile uint64_t systick;
        volatile uint64_t mem_tick;
        volatile uint64_t checkpoint_tick;
        volatile uint64_t restore_tick;
        volatile int64_t checkpoint_version;
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
    union aurora_completion_notifier_memory temp_memory;
}
#endif
;

typedef enum aurora_completion_notification_e eACN_notification;
typedef struct aurora_completion_notifier acn_hndl;

extern acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info);

extern int acn_connect_instance(acn_hndl *pHndl, aurora_blob_t *local_info,
                                aurora_blob_t *remote_info);

extern int acn_destroy_instance(acn_hndl **ppHndl);

extern int acn_tick(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_await(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_aheadbehind(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_set(acn_hndl *pHndl, eACN_notification notif,
                   const uint64_t value);

extern int acn_get(acn_hndl *pHndl, eACN_notification notif, uint64_t *pValue);

#endif
