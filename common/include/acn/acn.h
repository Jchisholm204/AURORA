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

#ifndef ACN_NAME_LEN
#define ACN_NAME_LEN 16
#endif

#ifndef BIT
#define BIT(x) ((1) << (x))
#endif

// Bitflagged enum
enum aurora_completion_notification_e {
    eACN_systick = BIT(0),
    eACN_memory = BIT(1),
    eACN_checkpoint = BIT(2),
    eACN_restore = BIT(2),
    eACN_version = BIT(3),

    // Final member (bitflagged increment)
    eACN_Nnotifications,
};

enum aurora_completion_notifier_error_e {
    eACN_OK = 0,
    eACN_ERR_NULL,
    eACN_ERR_INPROGRESS,
    eACN_ERR_UCS,
    eACN_ERR_FATAL,
    eACN_N_ERR,
};

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
        volatile int64_t version_tick;
        // Must be final element in struct
        volatile char name[ACN_NAME_LEN];
    };
    volatile uint64_t data[8];
};
#endif

struct aurora_completion_notifier_hndl
#ifdef ACN_INTERNAL
{
    // ACI handle
    aci_hndl *pACI;
    // Remote Notification
    ucp_rkey_h remote_rkey;
    uint64_t pRemote;
    // Local Notification
    ucp_mem_h local_mem_hndl;
    union aurora_completion_notifier_memory *volatile pLocal;
    union aurora_completion_notifier_memory temp_memory;
    // Request Pointer (Single thread access only, Not locked)
    ucs_status_ptr_t ucs_pRequest;
}
#endif
;

typedef enum aurora_completion_notification_e eACN_notification;
typedef enum aurora_completion_notifier_error_e eACN_error;
typedef struct aurora_completion_notifier_hndl acn_hndl;

extern acn_hndl *acn_create_instance(aci_hndl *pACI, aurora_blob_t *conn_info);

extern eACN_error acn_connect_instance(acn_hndl *pHndl,
                                       aurora_blob_t *local_info,
                                       aurora_blob_t *remote_info);

extern eACN_error acn_destroy_instance(acn_hndl **ppHndl);

extern int acn_tick(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_await(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_aheadbehind(acn_hndl *pHndl, eACN_notification notifs);

extern int acn_set(acn_hndl *pHndl, eACN_notification notif,
                   const uint64_t value);

extern eACN_error acn_get(acn_hndl *pHndl, eACN_notification notif,
                          uint64_t *pValue);

extern eACN_error acn_set_name(acn_hndl *pHndl,
                               const char name[static ACN_NAME_LEN]);
extern eACN_error acn_get_name(acn_hndl *pHndl, char name[static ACN_NAME_LEN]);

extern eACN_error acn_check(acn_hndl *pHndl, eACN_notification *pNotifs);

#endif
