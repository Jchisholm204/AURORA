/**
 * @file listener_group.h
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
 * @brief Group level aggregation of per-rank AEL events
 * @version 0.4
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-12
 *
 * @copyright Copyright (c) 2026
 *
 * A group is identified by (origin, group_id). Every received event is
 * attributed to a group event, keyed by event code plus, for checkpoint,
 * restore, and region events, the payload id_version (checkpoint version or
 * region ID). Lifecycle events (connect, failure, disconnect) use one key per
 * group. Each member is counted once per group event, so duplicate datagrams
 * do not advance the count. A member is identified by its rank and sending
 * endpoint (IP and port): the rank alone is not unique, because every ARE
 * instance of a group reports rank 0.
 *
 * A group can run more than once. A connect from a new member starts a new run,
 * and clears the group's events, once the previous run has ended: a member
 * disconnected or failed, or every member had connected.
 *
 * The group size is learned from the payload size field of connect and
 * failure events; a size of 0 there is a sender fault and is logged and
 * ignored. Until a valid size is received, a group event can report its first
 * rank but never completes, and a run that ends without any disconnect or
 * failure is not recognised as ended.
 */

#ifndef _LISTENER_GROUP_H_
#define _LISTENER_GROUP_H_

#include "ael/ael.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct listener_groups listener_groups_t;

/* Bitwise status of listener_groups_record: zero or more state flags on
 * success, or one or more error flags (never mixed with state flags). Counts
 * and sizes are read with the listener_groups_get_* queries. */
enum listener_group_e {
    eLISTENER_GROUP_OK = 0,

    /* State: this rank is the first to report the group event */
    eLISTENER_GROUP_FIRST = BIT(0),
    /* State: this rank completed the group event (count reached group size) */
    eLISTENER_GROUP_LAST = BIT(1),
    /* State: this member (rank and sending endpoint) had already reported the
     * group event */
    eLISTENER_GROUP_DUPLICATE = BIT(2),
    /* State: the event is counted per payload.id_version (checkpoint version
     * or region ID) rather than once per group */
    eLISTENER_GROUP_KEYED = BIT(3),

    /* Error: a required parameter was NULL */
    eLISTENER_GROUP_ERR_NULL = BIT(4),
    /* Error: allocation failed; the table is unchanged for this message */
    eLISTENER_GROUP_ERR_ALLOC = BIT(5),
};

typedef enum listener_group_e eLISTENER_group;

/* Any error flag set */
#define LISTENER_GROUP_ERR_MASK                                                \
    (eLISTENER_GROUP_ERR_NULL | eLISTENER_GROUP_ERR_ALLOC)

/**
 * @brief Allocate an empty group table
 *
 * @return Table, or NULL on allocation failure
 */
extern listener_groups_t *listener_groups_create(void);

/**
 * @brief Free a group table; sets the caller's pointer to NULL
 */
extern void listener_groups_destroy(listener_groups_t **ppGroups);

/**
 * @brief Attribute a received message to its group event
 *
 * @param pGroups Group table
 * @param pMsg Received message
 * @return eLISTENER_group state flags on success (possibly
 *         eLISTENER_GROUP_OK), or error flags (LISTENER_GROUP_ERR_MASK)
 */
extern eLISTENER_group listener_groups_record(listener_groups_t *pGroups,
                                              const ael_message_t *pMsg);

/**
 * @brief Group size from the group's connect/failure payloads
 *
 * @param pGroups Group table
 * @param origin Group origin
 * @param group_id Group ID
 * @return Group size, or 0 when the group is unknown or no valid
 *         connect/failure has been received for it yet
 */
extern uint64_t listener_groups_get_size(const listener_groups_t *pGroups,
                                         eAEL_origin origin, uint64_t group_id);

/**
 * @brief Number of distinct members that have reported a group event in the
 *        group's current run
 *
 * @param pGroups Group table
 * @param origin Group origin
 * @param group_id Group ID
 * @param event Event code
 * @param id_version Checkpoint version or region ID; ignored for lifecycle
 *        events (connect, failure, disconnect)
 * @return Distinct member count, 0 when the group event has not been seen
 */
extern size_t listener_groups_get_ranks_done(const listener_groups_t *pGroups,
                                             eAEL_origin origin,
                                             uint64_t group_id,
                                             eAEL_event event,
                                             int64_t id_version);

/**
 * @brief Number of distinct groups seen
 */
extern size_t listener_groups_count(const listener_groups_t *pGroups);

#endif
