/**
 * @file aim.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Instance Manager
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-26
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AIM_H_
#define _AIM_H_

#include "aci/aci.h"
#include "acn/acn.h"
#include "afv/afv.h"
#include "arm/arm.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct aurora_instance_manager_entry aim_entry_t;
typedef struct aurora_instance_manager_hndl aim_hndl;

struct aurora_instance_manager_entry {
    aci_hndl *pACI;
    acn_hndl *pACN;
    arm_hndl *pARM;
    afv_hndl *pAFV;
    size_t error_counter;
    union {
#ifdef AIM_INTERNAL
        struct {
            size_t instance_index;
            atomic_size_t references;
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
        size_t mask;
        atomic_size_t *sequences;
        size_t max_worker_ids;
    } queue;
}
#endif
;

/**
 * @brief Initialize an AIM instance
 *
 * @param max_workers Maximum number of AIM clients
 * @return
 */
extern aim_hndl *aim_init(size_t max_workers);

/**
 * @brief Destroy an AIM instance
 *
 * @param ppHndl pointer to aim HNDL
 * @return 0 on success
 */
extern int aim_finalize(aim_hndl **ppHndl);

/**
 * @brief Get the next avaliable AIM worker slot
 * Memory returned by this function must not be freed
 *
 * @param pHndl AIM handle to add the worker to
 * @return Pre allocated storage area to place the new worker data
 */
extern aim_entry_t *aim_add_entry(aim_hndl *pHndl);

/**
 * @brief Remove an entry from the AIM instance
 *
 * @param pHndl AIM handle
 * @param pEntry Entry to remove
 * @return 0 on success
 */
extern int aim_remove_entry(aim_hndl *pHndl, aim_entry_t *pEntry);

/**
 * @brief Return an entry to Instance Handler (AIM)
 *
 * @param pHndl AIM handle
 * @param pEntry Entry handle to release to AIM
 * @return
 */
extern int aim_enqueue(aim_hndl *pHndl, aim_entry_t *pEntry);

/**
 * @brief Take ownership of the next unlocked entry.
 * Memory returned by this function can be passed to either aim_remove_entry or
 * aim_enqueue
 *
 * @param pHndl AIM Handle
 * @return worker entry (memory must not be freed)
 */
extern aim_entry_t *aim_dequeue(aim_hndl *pHndl);

#endif
