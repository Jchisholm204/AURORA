/**
 * @file acl.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Connection Listener
 * @version 0.1
 * @date Created: 2026-02-26
 * @modified Last Modified: 2026-02-26
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ACL_ACL_H_
#define _ACL_ACL_H_

#include "acr/acr.h"
#include "ads/ads.h"
#include "aim.h"

#include <pthread.h>
#include <stdbool.h>
#include <threads.h>

typedef struct aurora_connection_listener acl_hndl;

struct aurora_connection_listener
#ifdef ACL_INTERNAL
{
    aim_hndl *pAIM;
    ads_hndl *pADS;
    acr_hndl *pACR;
    bool running;
    pthread_t thread_manager;
    atomic_size_t n_worker_threads;
}
#endif
;

extern acl_hndl *acl_init(aim_hndl *pAIM, acr_hndl *pACR);

extern int acl_finalize(acl_hndl **ppHndl);

#endif
