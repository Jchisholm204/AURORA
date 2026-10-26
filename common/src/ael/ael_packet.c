/**
 * @file ael_packet.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief AEL packet construction and destruction.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#include "ael/ael_packet.h"
#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static ael_packet_t *_ael_packet_alloc(eAEL_event id, uint64_t rank,
                                       uint64_t group_id) {
    assert((unsigned) id < (unsigned) eAEL_N_EVENT);

    ael_packet_t *packet = calloc(1, sizeof(*packet));
    if (!packet) {
        log_error("Bad Alloc??");
        return NULL;
    }

    uint64_t timestamp = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        timestamp =
            (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
    }

    packet->rank = rank;
    packet->group_id = group_id;
    packet->timestamp = timestamp;
    packet->event = id;

    return packet;
}

ael_packet_t *ael_packet_create_cli(
    eAEL_event id, uint64_t rank, uint64_t group_id,
    const char *event_message) {
    assert(id == eAEL_EVENT_CLI_CONNECT ||
           id == eAEL_EVENT_CLI_FAILURE ||
           id == eAEL_EVENT_CLI_DISCONNECT);

    ael_packet_t *packet = _ael_packet_alloc(id, rank, group_id);
    if (!packet) {
        return NULL;
    }

    if (event_message) {
        snprintf(packet->event_message, sizeof(packet->event_message), "%s",
                 event_message);
    }

    ael_packet_seal(packet);
    return packet;
}

ael_packet_t *ael_packet_create_checkpoint(
    eAEL_event id, uint64_t rank, uint64_t group_id, int64_t version,
    size_t n_regions, const char *name) {
    assert(id == eAEL_EVENT_CHECKPOINT_START ||
           id == eAEL_EVENT_CHECKPOINT_END ||
           id == eAEL_EVENT_RESTORE_START ||
           id == eAEL_EVENT_RESTORE_END);

    ael_packet_t *packet = _ael_packet_alloc(id, rank, group_id);
    if (!packet) {
        return NULL;
    }

    packet->payload.checkpoint.version = version;
    packet->payload.checkpoint.n_regions = n_regions;
    if (name) {
        snprintf(packet->payload.checkpoint.name,
                 sizeof(packet->payload.checkpoint.name), "%s", name);
    }

    ael_packet_seal(packet);
    return packet;
}

ael_packet_t *ael_packet_create_region(
    eAEL_event id, uint64_t rank, uint64_t group_id, int64_t region_id,
    size_t size, const char *name) {
    assert(id == eAEL_EVENT_REGION_ADD ||
           id == eAEL_EVENT_REGION_REMOVE);

    ael_packet_t *packet = _ael_packet_alloc(id, rank, group_id);
    if (!packet) {
        return NULL;
    }

    packet->payload.region.id = region_id;
    packet->payload.region.size = size;
    if (name) {
        snprintf(packet->payload.region.name,
                 sizeof(packet->payload.region.name), "%s", name);
    }

    ael_packet_seal(packet);
    return packet;
}

void ael_packet_destroy(ael_packet_t **ppPacket) {
    if (ppPacket && *ppPacket) {
        free(*ppPacket);
        *ppPacket = NULL;
    }
}
