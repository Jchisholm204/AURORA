/**
 * @file test_ael_client.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL UDP client example.
 *
 * Sends ael_packet_t datagrams to the AEL UDP server through the public AEL
 * handle API. Every packet is sent as one datagram per configured peer.
 *
 * Usage: test_ael_client [server_ip] [rank] [packet_count]
 *
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#include "ael_cdefs.h"
#include "log.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define AEL_DEFAULT_PACKETS 32
#define AEL_SAMPLE_REGION_SIZE 4096

struct ael_test_params {
    const char *server_ip;
    uint64_t rank;
    unsigned long packet_count;
};

static struct ael_test_params *ael_parse_params(int argc, char **argv);
static ael_packet_t *_ael_packet_create(eAEL_event event, uint64_t rank,
                                        unsigned long index);

int main(int argc, char **argv) {
    struct ael_test_params *tp = ael_parse_params(argc, argv);
    if (!tp) {
        log_error("Failed to parse client arguments");
        return 1;
    }

    ael_peer_t peer = {
        .ip = tp->server_ip,
        .port = PORT,
    };
    ael_hndl *pHndl = ael_create_instance(tp->rank, 0, 1, 0, &peer, 1);
    if (!pHndl) {
        free(tp);
        return 1;
    }

    log_info("AEL UDP client sending %lu packet(s) to %s:%d as rank %" PRIu64,
             tp->packet_count, tp->server_ip, PORT, tp->rank);

    for (unsigned long i = 0; i < tp->packet_count; i++) {
        eAEL_event event = ael_event_code(i);
        ael_packet_t *packet = _ael_packet_create(event, tp->rank, i);
        if (!packet) {
            log_error("Failed to allocate packet %lu", i);
            ael_destroy_instance(&pHndl);
            free(tp);
            return 1;
        }

        eAEL_error tx_status = ael_send_packet(pHndl, packet);
        if (tx_status != eAEL_OK) {
            log_error("Failed to send packet %lu (eAEL_error=%d)", i,
                      (int) tx_status);
            ael_packet_destroy(&packet);
            ael_destroy_instance(&pHndl);
            free(tp);
            return 1;
        }

        log_info("tx rank=%" PRIu64 " group_id=%" PRIu64 " ts=%" PRIu64
                 " ev=%s (%zu bytes)",
                 packet->rank, packet->group_id, packet->timestamp,
                 ael_event_name(packet->event), sizeof(*packet));
        ael_packet_destroy(&packet);
        usleep(10000);
    }

    ael_destroy_instance(&pHndl);
    log_info("AEL UDP client sent %lu packet(s)", tp->packet_count);
    free(tp);
    return 0;
}

static ael_packet_t *_ael_packet_create(eAEL_event event, uint64_t rank,
                                        unsigned long index) {
    char event_message[AEL_EVENT_MSG_LEN];
    char name[AEL_NAME_LEN];

    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " connect", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CLI_FAILURE:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " failure", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CLI_DISCONNECT:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " disconnect", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CHECKPOINT_START:
        return ael_packet_create_checkpoint(event, rank, 0, 1, 4, "ckpt-start");
    case eAEL_EVENT_CHECKPOINT_END:
        return ael_packet_create_checkpoint(event, rank, 0, 2, 4, "ckpt-end");
    case eAEL_EVENT_RESTORE_START:
        return ael_packet_create_checkpoint(event, rank, 0, 1, 4,
                                            "restore-start");
    case eAEL_EVENT_RESTORE_END:
        return ael_packet_create_checkpoint(event, rank, 0, 2, 4,
                                            "restore-end");
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        snprintf(name, sizeof(name), "rgn-%lu", index);
        return ael_packet_create_region(event, rank, 0, (int64_t) index,
                                        AEL_SAMPLE_REGION_SIZE, name);
    default:
        return NULL;
    }
}

static struct ael_test_params *ael_parse_params(int argc, char **argv) {
    struct ael_test_params *tp = malloc(sizeof(*tp));
    if (!tp) {
        log_error("Failed to allocate AEL test parameters");
        return NULL;
    }

    tp->server_ip = "127.0.0.1";
    tp->rank = 0;
    tp->packet_count = AEL_DEFAULT_PACKETS;

    if (argc > 1) {
        tp->server_ip = argv[1];
    }
    if (argc > 2) {
        char *end = NULL;
        errno = 0;
        long long rank = strtoll(argv[2], &end, 10);
        if (errno || end == argv[2] || *end != '\0' || rank < 0) {
            log_error("Invalid rank '%s'", argv[2]);
            free(tp);
            return NULL;
        }
        tp->rank = (uint64_t) rank;
    }
    if (argc > 3) {
        char *end = NULL;
        errno = 0;
        unsigned long packet_count = strtoul(argv[3], &end, 10);
        if (errno || end == argv[3] || *end != '\0' || packet_count == 0) {
            log_error("Invalid packet count '%s'", argv[3]);
            free(tp);
            return NULL;
        }
        tp->packet_count = packet_count;
    }

    return tp;
}
