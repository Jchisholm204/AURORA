/**
 * @file listener_group.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief Group level aggregation of per-rank AEL events
 * @version 0.1
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 *
 * A group is identified by (origin, group_id). Every received event is
 * attributed to a group event, keyed by event code plus, for checkpoint,
 * restore, and region events, the payload id_version (checkpoint version or
 * region ID). Lifecycle events (connect, failure, disconnect) use one key per
 * group. Each rank is counted once per group event, so duplicate datagrams do
 * not advance the count.
 *
 * The group size is learned from the payload size field of connect and
 * failure events. Until it is known, a group event can report its first rank
 * but never completes.
 */

#ifndef _LISTENER_GROUP_H_
#define _LISTENER_GROUP_H_

#include "ael/ael.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct listener_groups listener_groups_t;

typedef struct listener_group_result {
    /* This rank is the first to report the group event */
    bool first;
    /* This rank completed the group event (count reached group size) */
    bool last;
    /* This rank had already reported the group event */
    bool duplicate;
    /* Group event key carries a version/region ID */
    bool keyed;
    int64_t key;
    /* Distinct ranks that have reported the group event */
    size_t count;
    /* Known group size, 0 when not yet learned */
    uint64_t size;
} listener_group_result_t;

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
 * @param pResult Output; only valid when true is returned
 * @return true on success, false on allocation failure
 */
extern bool listener_groups_record(listener_groups_t *pGroups,
                                   const ael_message_t *pMsg,
                                   listener_group_result_t *pResult);

/**
 * @brief Number of distinct groups seen
 */
extern size_t listener_groups_count(const listener_groups_t *pGroups);

#endif
