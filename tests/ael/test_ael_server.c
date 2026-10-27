/**
 * @file test_ael_server.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL UDP server example.
 *
 * Creates an AEL handle bound to PORT and logs every valid ael_packet_t
 * received from any sender.
 *
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#include "ael_cdefs.h"
#include "log.h"

#include <inttypes.h>
#include <signal.h>

static volatile sig_atomic_t g_stop = 0;

static void _ael_signal_handler(int signal) {
    (void) signal;
    g_stop = 1;
}

static void _ael_log_packet(const ael_packet_t *packet);

int main(void) {
    // Do not use SA_RESTART so poll wakes up on Ctrl+C
    struct sigaction sa = {
        .sa_handler = _ael_signal_handler,
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ael_hndl *pHndl = ael_create_instance(0, 0, 1, PORT, NULL, 0);
    if (!pHndl) {
        return 1;
    }

    log_info("AEL UDP server listening on port %d (Ctrl+C to stop)", PORT);

    while (!g_stop) {
        ael_packet_t packet;
        eAEL_error rx_status = ael_recv_packet(pHndl, &packet, -1);

        if (rx_status == eAEL_OK) {
            _ael_log_packet(&packet);
        } else if (rx_status == eAEL_ERR_INTR) {
            if (g_stop) {
                break;
            }
        } else if (rx_status == eAEL_ERR_TIMEOUT ||
                   rx_status == eAEL_ERR_SIZE || rx_status == eAEL_ERR_CRC ||
                   rx_status == eAEL_ERR_EVENT) {
            continue;
        } else {
            break;
        }
    }

    ael_destroy_instance(&pHndl);
    log_info("AEL UDP server stopped");
    return 0;
}

static void _ael_log_packet(const ael_packet_t *packet) {
    log_info("rx rank=%" PRIu64 " group_id=%" PRIu64 " ts=%" PRIu64
             " ev=%s",
             packet->rank, packet->group_id, packet->timestamp,
             ael_event_name(packet->event));

    switch (packet->event) {
    case eAEL_EVENT_CLI_CONNECT:
    case eAEL_EVENT_CLI_FAILURE:
    case eAEL_EVENT_CLI_DISCONNECT:
        log_info("  event=\"%.*s\"", (int) sizeof(packet->event_message),
                 packet->event_message);
        break;
    case eAEL_EVENT_CHECKPOINT_START:
    case eAEL_EVENT_CHECKPOINT_END:
    case eAEL_EVENT_RESTORE_START:
    case eAEL_EVENT_RESTORE_END:
        log_info("  version=%" PRId64 " name=\"%.*s\" regions=%zu",
                 packet->payload.checkpoint.version,
                 (int) sizeof(packet->payload.checkpoint.name),
                 packet->payload.checkpoint.name,
                 packet->payload.checkpoint.n_regions);
        break;
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        log_info("  id=%" PRId64 " size=%zu name=\"%.*s\"",
                 packet->payload.region.id, packet->payload.region.size,
                 (int) sizeof(packet->payload.region.name),
                 packet->payload.region.name);
        break;
    default:
        break;
    }
}
