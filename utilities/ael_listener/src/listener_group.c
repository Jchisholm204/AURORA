/**
 * @file listener_group.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief Group level aggregation of per-rank AEL events
 * @version 0.1
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-06
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

/* One group level event: (event, key) and the distinct ranks that sent it */
struct listener_tracker {
    eAEL_event event;
    int64_t key;
    bool completed;
    size_t n_ranks;
    size_t cap_ranks;
    uint64_t *ranks;
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
    listener_groups_t *pGroups = calloc(1, sizeof(*pGroups));
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
            free(pGroup->trackers[t].ranks);
        }
        free(pGroup->trackers);
    }
    free(pGroups->groups);
    free(pGroups);
    *ppGroups = NULL;
}

size_t listener_groups_count(const listener_groups_t *pGroups) {
    return pGroups ? pGroups->n_groups : 0;
}

static struct listener_group *_find_group(listener_groups_t *pGroups,
                                          eAEL_origin origin,
                                          uint64_t group_id) {
    assert(pGroups != NULL);

    for (size_t g = 0; g < pGroups->n_groups; g++) {
        struct listener_group *pGroup = &pGroups->groups[g];
        if (pGroup->origin == origin && pGroup->group_id == group_id) {
            return pGroup;
        }
    }

    if (!_grow((void **) &pGroups->groups, &pGroups->cap_groups,
               pGroups->n_groups, sizeof(*pGroups->groups))) {
        return NULL;
    }
    struct listener_group *pGroup = &pGroups->groups[pGroups->n_groups++];
    memset(pGroup, 0, sizeof(*pGroup));
    pGroup->origin = origin;
    pGroup->group_id = group_id;
    return pGroup;
}

static struct listener_tracker *_find_tracker(struct listener_group *pGroup,
                                              eAEL_event event, int64_t key) {
    assert(pGroup != NULL);

    for (size_t t = 0; t < pGroup->n_trackers; t++) {
        struct listener_tracker *pTracker = &pGroup->trackers[t];
        if (pTracker->event == event && pTracker->key == key) {
            return pTracker;
        }
    }

    if (!_grow((void **) &pGroup->trackers, &pGroup->cap_trackers,
               pGroup->n_trackers, sizeof(*pGroup->trackers))) {
        return NULL;
    }
    struct listener_tracker *pTracker = &pGroup->trackers[pGroup->n_trackers++];
    memset(pTracker, 0, sizeof(*pTracker));
    pTracker->event = event;
    pTracker->key = key;
    return pTracker;
}

bool listener_groups_record(listener_groups_t *pGroups,
                            const ael_message_t *pMsg,
                            listener_group_result_t *pResult) {
    if (!pGroups || !pMsg || !pResult) {
        log_error("NULL Parameter");
        return false;
    }

    struct listener_group *pGroup =
        _find_group(pGroups, pMsg->origin, pMsg->group_id);
    if (!pGroup) {
        return false;
    }

    /* Connect and failure payloads carry the group size */
    if ((pMsg->event == eAEL_EVENT_CLI_CONNECT ||
         pMsg->event == eAEL_EVENT_CLI_FAILURE) &&
        pMsg->payload.size > 0 && pMsg->payload.size != pGroup->size) {
        if (pGroup->size != 0) {
            log_warn("Group %s:%" PRIu64 " size changed %" PRIu64
                     " -> %" PRIu64 " (rank %" PRIu64 ")",
                     listener_origin_name(pGroup->origin), pGroup->group_id,
                     pGroup->size, pMsg->payload.size, pMsg->rank);
        }
        pGroup->size = pMsg->payload.size;
    }

    bool keyed = !listener_event_is_lifecycle(pMsg->event);
    int64_t key = keyed ? pMsg->payload.id_version : 0;

    struct listener_tracker *pTracker = _find_tracker(pGroup, pMsg->event, key);
    if (!pTracker) {
        return false;
    }

    memset(pResult, 0, sizeof(*pResult));
    pResult->keyed = keyed;
    pResult->key = key;
    pResult->size = pGroup->size;

    for (size_t r = 0; r < pTracker->n_ranks; r++) {
        if (pTracker->ranks[r] == pMsg->rank) {
            pResult->duplicate = true;
            pResult->count = pTracker->n_ranks;
            return true;
        }
    }

    if (!_grow((void **) &pTracker->ranks, &pTracker->cap_ranks,
               pTracker->n_ranks, sizeof(*pTracker->ranks))) {
        return false;
    }
    pTracker->ranks[pTracker->n_ranks++] = pMsg->rank;

    pResult->count = pTracker->n_ranks;
    pResult->first = pTracker->n_ranks == 1;
    if (!pTracker->completed && pGroup->size > 0 &&
        pTracker->n_ranks >= pGroup->size) {
        pTracker->completed = true;
        pResult->last = true;
    }
    return true;
}
