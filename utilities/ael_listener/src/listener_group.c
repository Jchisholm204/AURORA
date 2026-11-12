/**
 * @file listener_group.c
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
 * @brief Group level aggregation of per-rank AEL events
 * @version 0.4
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-12
 *
 * @copyright Copyright (c) 2026
 */

#include "listener_group.h"

#include "listener_names.h"
#include "log.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* One sender of a group event. The rank alone is not unique (every ARE
 * instance reports rank 0), so the sending endpoint is part of the identity */
struct listener_member {
    uint64_t rank;
    uint32_t ip;
    uint16_t port;
};

/* One group level event: (event, key) and the distinct members that sent it */
struct listener_tracker {
    eAEL_event event;
    int64_t key;
    bool completed;
    size_t n_members;
    size_t cap_members;
    struct listener_member *members;
};

struct listener_group {
    eAEL_origin origin;
    uint64_t group_id;
    uint64_t size;
    size_t n_trackers;
    size_t cap_trackers;
    struct listener_tracker *trackers;
};

struct listener_groups {
    size_t n_groups;
    size_t cap_groups;
    struct listener_group *groups;
};

/**
 * @brief Ensure room for one more element in a growable array
 */
static bool _grow(void **ppArr, size_t *pCap, size_t count, size_t elem) {
    assert(ppArr != NULL);
    assert(pCap != NULL);

    if (count < *pCap) {
        return true;
    }
    size_t cap = *pCap ? *pCap * 2 : 8;
    void *pNew = realloc(*ppArr, cap * elem);
    if (!pNew) {
        log_error("Bad Alloc??");
        return false;
    }
    *ppArr = pNew;
    *pCap = cap;
    return true;
}

listener_groups_t *listener_groups_create(void) {
    listener_groups_t *pGroups = calloc(1, sizeof(listener_groups_t));
    if (!pGroups) {
        log_error("Bad Alloc??");
    }
    return pGroups;
}

void listener_groups_destroy(listener_groups_t **ppGroups) {
    if (!ppGroups || !*ppGroups) {
        return;
    }
    listener_groups_t *pGroups = *ppGroups;
    for (size_t g = 0; g < pGroups->n_groups; g++) {
        struct listener_group *pGroup = &pGroups->groups[g];
        for (size_t t = 0; t < pGroup->n_trackers; t++) {
            free(pGroup->trackers[t].members);
        }
        free(pGroup->trackers);
    }
    free(pGroups->groups);
    free(pGroups);
    *ppGroups = NULL;
}

size_t listener_groups_count(const listener_groups_t *pGroups) {
    if (!pGroups) {
        log_error("NULL Parameter");
        return 0;
    }
    return pGroups->n_groups;
}

/**
 * @brief Tracker key for an event: lifecycle events use one key per group,
 *        all others are counted per payload id_version
 */
static int64_t _event_key(eAEL_event event, int64_t id_version) {
    return listener_event_is_lifecycle(event) ? 0 : id_version;
}

static struct listener_group *_lookup_group(const listener_groups_t *pGroups,
                                            eAEL_origin origin,
                                            uint64_t group_id) {
    assert(pGroups != NULL);
    assert(pGroups->n_groups == 0 || pGroups->groups != NULL);

    for (size_t g = 0; g < pGroups->n_groups; g++) {
        struct listener_group *pGroup = &pGroups->groups[g];
        if (pGroup->origin == origin && pGroup->group_id == group_id) {
            return pGroup;
        }
    }
    return NULL;
}

static struct listener_tracker *_lookup_tracker(
    const struct listener_group *pGroup, eAEL_event event, int64_t key) {
    assert(pGroup != NULL);
    assert(pGroup->n_trackers == 0 || pGroup->trackers != NULL);

    for (size_t t = 0; t < pGroup->n_trackers; t++) {
        struct listener_tracker *pTracker = &pGroup->trackers[t];
        if (pTracker->event == event && pTracker->key == key) {
            return pTracker;
        }
    }
    return NULL;
}

/**
 * @brief Find a group, appending an empty one if it does not exist
 */
static struct listener_group *_find_group(listener_groups_t *pGroups,
                                          eAEL_origin origin,
                                          uint64_t group_id) {
    assert(pGroups != NULL);

    struct listener_group *pGroup = _lookup_group(pGroups, origin, group_id);
    if (pGroup) {
        return pGroup;
    }

    if (!_grow((void **) &pGroups->groups, &pGroups->cap_groups,
               pGroups->n_groups, sizeof(*pGroups->groups))) {
        return NULL;
    }
    assert(pGroups->groups != NULL);
    pGroup = &pGroups->groups[pGroups->n_groups++];
    memset(pGroup, 0, sizeof(*pGroup));
    pGroup->origin = origin;
    pGroup->group_id = group_id;
    return pGroup;
}

/**
 * @brief Find a group event tracker, appending an empty one if it does not
 *        exist
 */
static struct listener_tracker *_find_tracker(struct listener_group *pGroup,
                                              eAEL_event event, int64_t key) {
    assert(pGroup != NULL);

    struct listener_tracker *pTracker = _lookup_tracker(pGroup, event, key);
    if (pTracker) {
        return pTracker;
    }

    if (!_grow((void **) &pGroup->trackers, &pGroup->cap_trackers,
               pGroup->n_trackers, sizeof(*pGroup->trackers))) {
        return NULL;
    }
    assert(pGroup->trackers != NULL);
    pTracker = &pGroup->trackers[pGroup->n_trackers++];
    memset(pTracker, 0, sizeof(*pTracker));
    pTracker->event = event;
    pTracker->key = key;
    return pTracker;
}

static bool _has_member(const struct listener_tracker *pTracker,
                        const ael_message_t *pMsg) {
    assert(pMsg != NULL);

    if (!pTracker) {
        return false;
    }
    assert(pTracker->n_members == 0 || pTracker->members != NULL);

    for (size_t m = 0; m < pTracker->n_members; m++) {
        const struct listener_member *pMember = &pTracker->members[m];
        if (pMember->rank == pMsg->rank && pMember->ip == pMsg->from_ip &&
            pMember->port == pMsg->from_port) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Whether a connect message starts a new run of its group
 *
 * True when the sender is new to the current run and that run has already
 * ended: a member disconnected or failed, or every member had connected.
 */
static bool _starts_new_run(const struct listener_group *pGroup,
                            const ael_message_t *pMsg) {
    assert(pGroup != NULL);
    assert(pMsg != NULL);

    if (pMsg->event != eAEL_EVENT_CLI_CONNECT) {
        return false;
    }

    const struct listener_tracker *pConnect =
        _lookup_tracker(pGroup, eAEL_EVENT_CLI_CONNECT, 0);
    if (_has_member(pConnect, pMsg)) {
        return false;
    }
    if (pConnect && pConnect->completed) {
        return true;
    }

    const struct listener_tracker *pEnd =
        _lookup_tracker(pGroup, eAEL_EVENT_CLI_DISCONNECT, 0);
    if (pEnd && pEnd->n_members > 0) {
        return true;
    }
    pEnd = _lookup_tracker(pGroup, eAEL_EVENT_CLI_FAILURE, 0);
    return pEnd && pEnd->n_members > 0;
}

/**
 * @brief Drop every group event of a group so a new run counts from zero;
 *        the group identity and size are kept
 */
static void _reset_group(struct listener_group *pGroup) {
    assert(pGroup != NULL);
    assert(pGroup->n_trackers == 0 || pGroup->trackers != NULL);

    for (size_t t = 0; t < pGroup->n_trackers; t++) {
        free(pGroup->trackers[t].members);
        pGroup->trackers[t].members = NULL;
    }
    pGroup->n_trackers = 0;
}

eLISTENER_group listener_groups_record(listener_groups_t *pGroups,
                                       const ael_message_t *pMsg) {
    if (!pGroups || !pMsg) {
        log_error("NULL Parameter");
        return eLISTENER_GROUP_ERR_NULL;
    }

    struct listener_group *pGroup =
        _find_group(pGroups, pMsg->origin, pMsg->group_id);
    if (!pGroup) {
        return eLISTENER_GROUP_ERR_ALLOC;
    }

    if (_starts_new_run(pGroup, pMsg)) {
        log_debug("Group %s:%" PRIu64 " new run (rank %" PRIu64 ")",
                  listener_origin_name(pGroup->origin), pGroup->group_id,
                  pMsg->rank);
        _reset_group(pGroup);
    }

    /* Connect and failure payloads carry the group size; 0 is a sender fault */
    if (pMsg->event == eAEL_EVENT_CLI_CONNECT ||
        pMsg->event == eAEL_EVENT_CLI_FAILURE) {
        if (pMsg->payload.size == 0) {
            log_warn("Group %s:%" PRIu64 " %s with size 0 (rank %" PRIu64 ")",
                     listener_origin_name(pGroup->origin), pGroup->group_id,
                     listener_event_name(pMsg->event), pMsg->rank);
        } else if (pMsg->payload.size != pGroup->size) {
            if (pGroup->size != 0) {
                log_warn("Group %s:%" PRIu64 " size changed %" PRIu64
                         " -> %" PRIu64 " (rank %" PRIu64 ")",
                         listener_origin_name(pGroup->origin), pGroup->group_id,
                         pGroup->size, pMsg->payload.size, pMsg->rank);
            }
            pGroup->size = pMsg->payload.size;
        }
    }

    bool keyed = !listener_event_is_lifecycle(pMsg->event);
    int64_t key = _event_key(pMsg->event, pMsg->payload.id_version);

    struct listener_tracker *pTracker = _find_tracker(pGroup, pMsg->event, key);
    if (!pTracker) {
        return eLISTENER_GROUP_ERR_ALLOC;
    }

    eLISTENER_group state = keyed ? eLISTENER_GROUP_KEYED : eLISTENER_GROUP_OK;

    if (_has_member(pTracker, pMsg)) {
        return state | eLISTENER_GROUP_DUPLICATE;
    }

    if (!_grow((void **) &pTracker->members, &pTracker->cap_members,
               pTracker->n_members, sizeof(*pTracker->members))) {
        return eLISTENER_GROUP_ERR_ALLOC;
    }
    assert(pTracker->members != NULL);
    pTracker->members[pTracker->n_members++] = (struct listener_member){
        .rank = pMsg->rank,
        .ip = pMsg->from_ip,
        .port = pMsg->from_port,
    };

    if (pTracker->n_members == 1) {
        state |= eLISTENER_GROUP_FIRST;
    }
    if (!pTracker->completed && pGroup->size > 0 &&
        pTracker->n_members >= pGroup->size) {
        pTracker->completed = true;
        state |= eLISTENER_GROUP_LAST;
    }
    return state;
}

uint64_t listener_groups_get_size(const listener_groups_t *pGroups,
                                  eAEL_origin origin, uint64_t group_id) {
    if (!pGroups) {
        log_error("NULL Parameter");
        return 0;
    }

    const struct listener_group *pGroup =
        _lookup_group(pGroups, origin, group_id);
    return pGroup ? pGroup->size : 0;
}

size_t listener_groups_get_ranks_done(const listener_groups_t *pGroups,
                                      eAEL_origin origin, uint64_t group_id,
                                      eAEL_event event, int64_t id_version) {
    if (!pGroups) {
        log_error("NULL Parameter");
        return 0;
    }

    const struct listener_group *pGroup =
        _lookup_group(pGroups, origin, group_id);
    if (!pGroup) {
        return 0;
    }
    const struct listener_tracker *pTracker =
        _lookup_tracker(pGroup, event, _event_key(event, id_version));
    return pTracker ? pTracker->n_members : 0;
}
