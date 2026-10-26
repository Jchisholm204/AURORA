/**
 * @file ael_packet.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
 * @brief
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-26
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
typedef enum aurora_external_logger_event_e eAEL_event;

struct aurora_external_logger_payload_checkpoint {
    int64_t version;
    size_t n_regions;
    char name[AEL_NAME_LEN];
};
typedef struct aurora_external_logger_payload_checkpoint
    ael_packet_payld_checkpoint_t;

struct aurora_external_logger_payload_region {
    int64_t id;
    size_t size;
    char name[AEL_NAME_LEN];
};
typedef struct aurora_external_logger_payload_region ael_packet_payld_region_t;

struct aurora_external_logger_packet {
    uint64_t rank;
    uint64_t group_id;
    uint64_t timestamp;
    eAEL_event event;
    uint32_t crc;
    union {
        // Shared by CHECKPOINT_* and RESTORE_* events
        ael_packet_payld_checkpoint_t checkpoint;
        ael_packet_payld_region_t region;
    } payload;
    char event_message[AEL_EVENT_MSG_LEN];
};
typedef struct aurora_external_logger_packet ael_packet_t;

_Static_assert(sizeof(eAEL_event) == 4,
               "unexpected eAEL_event size");
_Static_assert(offsetof(ael_packet_t, crc) == 28,
               "unexpected crc offset");
_Static_assert(offsetof(ael_packet_t, payload) == 32,
               "unexpected payload offset");
_Static_assert(offsetof(ael_packet_t, event_message) == 64,
               "unexpected event_message offset");
_Static_assert(sizeof(ael_packet_t) == 128,
               "unexpected packet size");

// CRC functions
extern uint32_t ael_packet_crc(const ael_packet_t *packet);
extern void ael_packet_seal(ael_packet_t *packet);
extern bool ael_packet_verify(const ael_packet_t *packet);

// Packet functions
extern ael_packet_t *ael_packet_create_cli(
    eAEL_event id, uint64_t rank, uint64_t group_id,
    const char *event_message);

extern ael_packet_t *ael_packet_create_checkpoint(
    eAEL_event id, uint64_t rank, uint64_t group_id, int64_t version,
    size_t n_regions, const char *name);

extern ael_packet_t *ael_packet_create_region(
    eAEL_event id, uint64_t rank, uint64_t group_id, int64_t region_id,
    size_t size, const char *name);

extern void ael_packet_destroy(ael_packet_t **ppPacket);

#endif
