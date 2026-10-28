/**
 * @file ael_packet.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL packet layout and private construction helpers.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_PACKET_H_
#define _AEL_PACKET_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AEL_NAME_LEN 16
#define AEL_EVENT_MSG_LEN 64

enum aurora_external_logger_event_e {
    eAEL_EVENT_CLI_CONNECT,
    eAEL_EVENT_CLI_FAILURE,
    eAEL_EVENT_CLI_DISCONNECT,
    eAEL_EVENT_CHECKPOINT_START,
    eAEL_EVENT_CHECKPOINT_END,
    eAEL_EVENT_RESTORE_START,
    eAEL_EVENT_RESTORE_END,
    eAEL_EVENT_REGION_ADD,
    eAEL_EVENT_REGION_REMOVE,
    eAEL_N_EVENT
};

struct aurora_external_logger_payload {
    int64_t id_version;
    uint64_t size;
    char name[AEL_NAME_LEN];
};

typedef enum aurora_external_logger_event_e eAEL_event;
typedef struct aurora_external_logger_payload ael_payload_t;

_Static_assert(sizeof(eAEL_event) == 4, "unexpected eAEL_event size");
_Static_assert(sizeof(ael_payload_t) == 32, "unexpected payload size");

#ifdef AEL_INTERNAL
struct aurora_external_logger_packet {
    uint64_t rank;
    uint64_t group_id;
    uint64_t timestamp;
    enum aurora_external_logger_event_e event;
    union {
        uint32_t crc;
        uint32_t __reserved;
    };
    struct aurora_external_logger_payload payload;
    char event_message[AEL_EVENT_MSG_LEN];
};

typedef struct aurora_external_logger_packet ael_packet_t;

_Static_assert(offsetof(ael_packet_t, crc) == 28, "unexpected crc offset");
_Static_assert(offsetof(ael_packet_t, payload) == 32,
               "unexpected payload offset");
_Static_assert(offsetof(ael_packet_t, event_message) == 64,
               "unexpected event_message offset");
_Static_assert(sizeof(ael_packet_t) == 128, "unexpected packet size");

extern uint32_t ael_packet_crc(const ael_packet_t *pPacket);
extern void ael_packet_finalize(ael_packet_t *pPacket);
extern bool ael_packet_verify(const ael_packet_t *pPacket);

/* The caller owned storage. */
extern void _ael_packet_init(ael_packet_t *pPacket, eAEL_event id,
                             uint64_t rank, uint64_t group_id,
                             const char *event_message);
extern void _ael_packet_set_payload(ael_payload_t *pPayload, int64_t id_version,
                                    uint64_t size, const char *name);
#endif

#endif
