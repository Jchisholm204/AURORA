/**
 * @file test_ael_client.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL UDP client example.
 *
 * Sends every AEL event through the public send helpers. Packet construction,
 * finalizing, and transmission are handled internally by AEL.
 *
 * Usage: test_ael_client [server_ip] [rank] [packet_count]
 *
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-28
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
static eAEL_error _ael_send_event(ael_hndl *pHndl, uint64_t rank,
                                  eAEL_event event, unsigned long index);

int main(int argc, char **argv) {
    struct ael_test_params *tp = ael_parse_params(argc, argv);
    if (!tp) {
        log_error("Failed to parse client arguments");
        return 1;
    }

    /* Create instance with max_recipients=1 */
    ael_hndl *pHndl = ael_create_instance(tp->rank, 0, 0, 1);
    if (!pHndl) {
        free(tp);
        return 1;
    }

    /* Add the server as a recipient peer */
    eAEL_error status = ael_add_recipient(pHndl, tp->server_ip, PORT);
    if (status != eAEL_OK) {
        log_error("Failed to add recipient: eAEL_error=%d", (int) status);
        ael_destroy_instance(&pHndl);
        free(tp);
        return 1;
    }

    log_info("AEL UDP client sending %lu packet(s) to %s:%d as rank %" PRIu64,
             tp->packet_count, tp->server_ip, PORT, tp->rank);

    for (unsigned long i = 0; i < tp->packet_count; i++) {
        eAEL_event event = ael_event_code(i);
        eAEL_error tx_status = _ael_send_event(pHndl, tp->rank, event, i);
        if (tx_status != eAEL_OK) {
            log_error("Failed to send packet %lu (eAEL_error=%d)", i,
                      (int) tx_status);
            ael_destroy_instance(&pHndl);
            free(tp);
            return 1;
        }

        log_info("tx rank=%" PRIu64 " ev=%s", tp->rank, ael_event_name(event));
        usleep(10000);
    }

    ael_destroy_instance(&pHndl);
    log_info("AEL UDP client sent %lu packet(s)", tp->packet_count);
    free(tp);
    return 0;
}

static eAEL_error _ael_send_event(ael_hndl *pHndl, uint64_t rank,
                                  eAEL_event event, unsigned long index) {
    char event_message[AEL_EVENT_MSG_LEN];
    char name[AEL_NAME_LEN];

    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " connect", rank);
        return ael_send_cli(pHndl, event, event_message);
    case eAEL_EVENT_CLI_FAILURE:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " failure", rank);
        return ael_send_cli(pHndl, event, event_message);
    case eAEL_EVENT_CLI_DISCONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " disconnect", rank);
        return ael_send_cli(pHndl, event, event_message);
    case eAEL_EVENT_CHECKPOINT_START:
        return ael_send_checkpoint(pHndl, event, 1, 4, "ckpt-start", NULL);
    case eAEL_EVENT_CHECKPOINT_END:
        return ael_send_checkpoint(pHndl, event, 2, 4, "ckpt-end", NULL);
    case eAEL_EVENT_RESTORE_START:
        return ael_send_checkpoint(pHndl, event, 1, 4, "restore-start", NULL);
    case eAEL_EVENT_RESTORE_END:
        return ael_send_checkpoint(pHndl, event, 2, 4, "restore-end", NULL);
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        snprintf(name, AEL_NAME_LEN, "rgn-%lu", index);
        return ael_send_region(pHndl, event, (int64_t) index,
                               AEL_SAMPLE_REGION_SIZE, name, NULL);
    default:
        log_error("Unsupported event code %d", (int) event);
        return eAEL_ERR_EVENT | eAEL_ERR_FATAL;
    }
}

static struct ael_test_params *ael_parse_params(int argc, char **argv) {
    struct ael_test_params *tp = malloc(sizeof(struct ael_test_params));
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
