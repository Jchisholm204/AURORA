/**
 * @file test_ael_server.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AEL UDP server example.
 *
 * Creates an AEL handle bound to PORT and logs every valid ael_message_t
 * received from any sender.
 *
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-27
 *
 * @copyright Copyright (c) 2026
 */

#include "ael_cdefs.h"
#include "log.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_stop = 0;

static void _ael_signal_handler(int signal) {
    (void) signal;
    g_stop = 1;
}

static void _ael_log_message(const ael_message_t *msg);

int main(void) {
    /* Do not use SA_RESTART so poll wakes up on Ctrl+C */
    struct sigaction sa = {
        .sa_handler = _ael_signal_handler,
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Create instance with max_recipients=0 (no outbound sends) */
    ael_hndl *pHndl = ael_create_instance(0, 0, PORT, 0);
    if (!pHndl) {
        return 1;
    }

    log_info("AEL UDP server listening on port %d (Ctrl+C to stop)", PORT);

    while (!g_stop) {
        ael_message_t msg;
        eAEL_error rx_status = ael_recv_message(pHndl, &msg, -1);

        if (rx_status == eAEL_OK) {
            _ael_log_message(&msg);
        } else if (rx_status & eAEL_ERR_FATAL) {
            log_error("AEL recv failed (eAEL_error=%d)", (int) rx_status);
            break;
        }
        /* Any non-fatal status is a transient receive condition */
    }

    ael_destroy_instance(&pHndl);
    log_info("AEL UDP server stopped");
    return 0;
}

static void _ael_log_message(const ael_message_t *msg) {
    char from[INET_ADDRSTRLEN] = "0.0.0.0";
    struct in_addr addr = {.s_addr = htonl(msg->from_ip)};
    if (!inet_ntop(AF_INET, &addr, from, sizeof(from))) {
        (void) snprintf(from, sizeof(from), "0x%08x", msg->from_ip);
    }

    log_info("rx from=%s:%u rank=%" PRIu64 " group_id=%" PRIu64
             " ts=%" PRIu64 " ev=%s",
             from, (unsigned) msg->from_port, msg->rank, msg->group_id,
             msg->timestamp_ns, ael_event_name(msg->event));
    log_info("  id_version=%" PRId64 " size=%" PRIu64 " name=\"%.*s\"",
             msg->payload.id_version, msg->payload.size, AEL_NAME_LEN,
             msg->payload.name);
    log_info("  event=\"%.*s\"", AEL_EVENT_MSG_LEN, msg->event_message);
}
