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

/**
 * @brief Uniform AEL event payload.
 *
 * Every event uses the same three slots, so the wire layout, the decoded
 * message and any serialiser share one definition and one field order.
 */
struct aurora_external_logger_payload {
    int64_t id_version;      /* id of the event (region id / checkpoint version) */
    uint64_t size;           /* size of the event (region bytes / region count) */
    char name[AEL_NAME_LEN]; /* name of the event (region name / checkpoint name) */
};

struct aurora_external_logger_packet {
    uint64_t rank;
    uint64_t group_id;
    uint64_t timestamp;
    enum aurora_external_logger_event_e event;
    // Reserved portion may be used internally by AEL. This memory should not
    // be accessed externally.
    union {
#ifdef AEL_INTERNAL
        uint32_t crc;
#endif
        uint32_t __reserved;
    };
    struct aurora_external_logger_payload payload;
    char event_message[AEL_EVENT_MSG_LEN];
};

typedef enum aurora_external_logger_event_e eAEL_event;
typedef struct aurora_external_logger_payload ael_payload_t;
typedef struct aurora_external_logger_packet ael_packet_t;

_Static_assert(sizeof(eAEL_event) == 4, "unexpected eAEL_event size");
_Static_assert(sizeof(ael_payload_t) == 32, "unexpected payload size");
#ifdef AEL_INTERNAL
_Static_assert(offsetof(ael_packet_t, crc) == 28, "unexpected crc offset");
#endif
_Static_assert(offsetof(ael_packet_t, payload) == 32,
               "unexpected payload offset");
_Static_assert(offsetof(ael_packet_t, event_message) == 64,
               "unexpected event_message offset");
_Static_assert(sizeof(ael_packet_t) == 128, "unexpected packet size");

#ifdef AEL_INTERNAL
// Internal CRC helpers; not part of the public AEL packet API.
extern uint32_t ael_packet_crc(const ael_packet_t *packet);
extern void ael_packet_finalize(ael_packet_t *packet);
extern bool ael_packet_verify(const ael_packet_t *packet);

// Internal packet construction helpers; not part of the public AEL API.
// The caller owns the packet storage (normally a stack variable).
extern void _ael_packet_init(ael_packet_t *packet, eAEL_event id, uint64_t rank,
                             uint64_t group_id, const char *event_message);
extern void _ael_packet_set_payload(ael_payload_t *payload, int64_t id_version,
                                    uint64_t size, const char *name);
#endif

#endif
