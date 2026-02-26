/**
 * @file aim.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-26
 * @modified Last Modified: 2026-02-26
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AIM_H_
#define _AIM_H_

#include "aci/aci.h"
#include "acn/acn.h"

#include <stdatomic.h>
#include <stdalign.h>

#define AIM_INTERNAL

typedef struct aurora_instance_manager_entry aim_entry_t;
typedef struct aurora_instance_manager_hndl aim_hndl;

struct aurora_instance_manager_entry {
    aci_hndl *pACI;
    acn_hndl *pACN;
    union {
#ifdef AIM_INTERNAL
        struct {
            size_t instance_index;
        };
#endif
        uint64_t __reserved[2];
    };
};

struct aurora_instance_manager_hndl
#ifdef AIM_INTERNAL
{
    aim_entry_t *workers;
    atomic_size_t n_workers;
    size_t max_workers;

    struct {
        alignas(64) atomic_size_t enqueue_pos;
        alignas(64) atomic_size_t dequeue_pos;
        size_t *worker_ids;
        size_t max_worker_ids;
    } queue;
}
#endif
;

extern aim_hndl *aim_init(size_t n_workers);

extern int aim_finalize(aim_hndl **ppHndl);

extern aim_entry_t *aim_add_entry(aim_hndl *pHndl);

extern int aim_remove_entry(aim_hndl *pHndl, aim_entry_t *pEntry);

extern int aim_enqueue(aim_hndl *pHndl, aim_entry_t *pEntry);

extern aim_entry_t *aim_dequeue(aim_hndl *pHndl);

#endif
