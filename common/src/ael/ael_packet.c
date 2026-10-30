/**
 * @file ael_packet.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL packet construction and finalization helpers.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael_packet.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void _ael_packet_init(ael_packet_t *pPacket, eAEL_event id, uint64_t rank,
                      uint64_t group_id, const char *event_message) {
    assert(pPacket != NULL);
    assert((unsigned) id < (unsigned) eAEL_N_EVENT);

    memset(pPacket, 0, sizeof(ael_packet_t));

    uint64_t timestamp = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        timestamp =
            (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
    }

    pPacket->rank = rank;
    pPacket->group_id = group_id;
    pPacket->timestamp = timestamp;
    pPacket->event = id;

    if (event_message) {
        (void) snprintf(pPacket->event_message, AEL_EVENT_MSG_LEN, "%s",
                        event_message);
    }
}

void ael_packet_finalize(ael_packet_t *pPacket) {
    if (pPacket) {
        pPacket->crc = ael_packet_crc(pPacket);
    }
}

void _ael_packet_set_payload(ael_payload_t *pPayload, int64_t id_version,
                             uint64_t size, const char *name) {
    assert(pPayload != NULL);

    pPayload->id_version = id_version;
    pPayload->size = size;
    if (name) {
        (void) snprintf(pPayload->name, AEL_NAME_LEN, "%s", name);
    }
}
