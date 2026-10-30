/**
 * @file test_ael_recv_bad.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @brief AEL receive-path rejection tests.
 *
 * Feeds malformed and hostile datagrams to ael_recv_message() on loopback and
 * checks that each one is rejected with the expected status, that the failure
 * is not marked fatal, and that the caller's message is left untouched. Also
 * checks that a valid packet is decoded correctly and that hostile string
 * fields are force terminated before they are handed back.
 *
 * @version 0.1
 * @date Created: 2026-10-28
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael.h"

#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define AEL_BAD_TEST_PORT 8335
#define AEL_BAD_TEST_TIMEOUT_MS 500

#define AEL_MSG_SENTINEL 0xA5

static int g_failures = 0;

/**
 * @brief Compare a receive status against the expected value
 *
 * On failure the message must also still be filled with the sentinel byte,
 * proving that ael_recv_message() did not publish a rejected datagram.
 */
static void _expect_status(const char *name, eAEL_error got, eAEL_error want,
                           const ael_message_t *msg) {
    if (got != want) {
        log_error("[FAIL] %s: got eAEL_error=0x%X, expected 0x%X", name,
                  (unsigned) got, (unsigned) want);
        g_failures++;
        return;
    }

    if (want != eAEL_OK) {
        const uint8_t *bytes = (const uint8_t *) msg;
        for (size_t i = 0; i < sizeof(*msg); i++) {
            if (bytes[i] != AEL_MSG_SENTINEL) {
                log_error("[FAIL] %s: message modified on failure (byte %zu)",
                          name, i);
                g_failures++;
                return;
            }
        }
    }

    log_info("[PASS] %s", name);
}

static void _check(bool condition, const char *name, const char *what) {
    if (!condition) {
        log_error("[FAIL] %s: %s", name, what);
        g_failures++;
        return;
    }
    log_info("[PASS] %s: %s", name, what);
}

/**
 * @brief Receive with the output pre-filled with the sentinel pattern
 */
static eAEL_error _recv_fresh(ael_hndl *pHndl, ael_message_t *msg) {
    memset(msg, AEL_MSG_SENTINEL, sizeof(*msg));
    return ael_recv_message(pHndl, msg, AEL_BAD_TEST_TIMEOUT_MS);
}

static int _open_sender(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        log_error("Failed to create sender socket");
    }
    return fd;
}

static bool _send_to_dut(int fd, const void *buf, size_t len) {
    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(AEL_BAD_TEST_PORT),
    };
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ssize_t sent =
        sendto(fd, buf, len, 0, (struct sockaddr *) &dst, sizeof(dst));
    if (sent < 0 || (size_t) sent != len) {
        log_error("Failed to send %zu byte datagram to DUT", len);
        return false;
    }
    return true;
}

static uint16_t _sender_port(int fd) {
    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(fd, (struct sockaddr *) &local, &local_len) != 0) {
        return 0;
    }
    return ntohs(local.sin_port);
}

/**
 * @brief Build a valid, CRC-finalized packet
 */
static void _build_packet(ael_packet_t *packet, eAEL_event event) {
    memset(packet, 0, sizeof(*packet));
    packet->rank = 7;
    packet->group_id = 3;
    packet->timestamp = 123456789ULL;
    packet->event = event;
    packet->payload.id_version = 42;
    packet->payload.size = 4096;
    (void) snprintf(packet->payload.name, AEL_NAME_LEN, "%s", "rgn-bad");
    (void) snprintf(packet->event_message, AEL_EVENT_MSG_LEN, "%s", "bad input");
    ael_packet_finalize(packet);
}

int main(void) {
    ael_hndl *pHndl = ael_create_instance(0, 0, AEL_BAD_TEST_PORT, 0);
    if (!pHndl) {
        log_error("Failed to bind AEL test receiver on port %d",
                  AEL_BAD_TEST_PORT);
        return 1;
    }

    int tx = _open_sender();
    if (tx < 0) {
        ael_destroy_instance(&pHndl);
        return 1;
    }

    ael_message_t msg;
    ael_packet_t packet;

    /* 1. Nothing queued: timeout, nothing written */
    _expect_status("timeout", _recv_fresh(pHndl, &msg), eAEL_ERR_TIMEOUT, &msg);

    /* 2. Undersized datagram */
    const uint32_t short_dgram = 0;
    if (_send_to_dut(tx, &short_dgram, sizeof(short_dgram))) {
        _expect_status("short datagram", _recv_fresh(pHndl, &msg),
                       eAEL_ERR_SIZE, &msg);
    } else {
        g_failures++;
    }

    /* 3. Oversized datagram, discarded by the MSG_TRUNC size check */
    uint8_t oversized[256] = {0};
    if (_send_to_dut(tx, oversized, sizeof(oversized))) {
        _expect_status("oversized datagram", _recv_fresh(pHndl, &msg),
                       eAEL_ERR_SIZE, &msg);
    } else {
        g_failures++;
    }

    /* 4. Correct size, corrupted payload after the CRC was stamped */
    _build_packet(&packet, eAEL_EVENT_REGION_ADD);
    packet.event_message[0] ^= 0xFF;
    if (_send_to_dut(tx, &packet, sizeof(packet))) {
        _expect_status("bad CRC", _recv_fresh(pHndl, &msg), eAEL_ERR_CRC, &msg);
    } else {
        g_failures++;
    }

    /* 5. Valid CRC but the event code is outside the enum */
    _build_packet(&packet, (eAEL_event) eAEL_N_EVENT);
    if (_send_to_dut(tx, &packet, sizeof(packet))) {
        _expect_status("bad event code", _recv_fresh(pHndl, &msg),
                       eAEL_ERR_EVENT, &msg);
    } else {
        g_failures++;
    }

    /* 6. Valid packet with string fields that fill every byte */
    _build_packet(&packet, eAEL_EVENT_REGION_ADD);
    memset(packet.payload.name, 'A', AEL_NAME_LEN);
    memset(packet.event_message, 'A', AEL_EVENT_MSG_LEN);
    ael_packet_finalize(&packet);
    if (_send_to_dut(tx, &packet, sizeof(packet))) {
        eAEL_error status = _recv_fresh(pHndl, &msg);
        _expect_status("unterminated strings", status, eAEL_OK, &msg);
        if (status == eAEL_OK) {
            uint16_t sender_port = _sender_port(tx);
            _check(msg.event == eAEL_EVENT_REGION_ADD, "unterminated strings",
                   "event decoded");
            _check(msg.rank == 7 && msg.group_id == 3 &&
                       msg.timestamp_ns == 123456789ULL,
                   "unterminated strings", "envelope decoded");
            _check(msg.payload.id_version == 42 && msg.payload.size == 4096,
                   "unterminated strings", "payload slots decoded");
            _check(msg.from_ip == INADDR_LOOPBACK &&
                       msg.from_port == sender_port,
                   "unterminated strings", "sender address decoded");
            _check(strnlen(msg.payload.name, AEL_NAME_LEN) ==
                           AEL_NAME_LEN - 1 &&
                       msg.payload.name[AEL_NAME_LEN - 1] == '\0',
                   "unterminated strings", "name terminated");
            _check(strnlen(msg.event_message, AEL_EVENT_MSG_LEN) ==
                           AEL_EVENT_MSG_LEN - 1 &&
                       msg.event_message[AEL_EVENT_MSG_LEN - 1] == '\0',
                   "unterminated strings", "event_message terminated");
        }
    } else {
        g_failures++;
    }

    /* 7. Valid packet with no optional strings: empty, not garbage */
    _build_packet(&packet, eAEL_EVENT_CLI_CONNECT);
    memset(packet.payload.name, 0, AEL_NAME_LEN);
    memset(packet.event_message, 0, AEL_EVENT_MSG_LEN);
    ael_packet_finalize(&packet);
    if (_send_to_dut(tx, &packet, sizeof(packet))) {
        eAEL_error status = _recv_fresh(pHndl, &msg);
        _expect_status("empty strings", status, eAEL_OK, &msg);
        if (status == eAEL_OK) {
            _check(msg.payload.name[0] == '\0' && msg.event_message[0] == '\0',
                   "empty strings", "strings decoded as empty");
        }
    } else {
        g_failures++;
    }

    close(tx);
    ael_destroy_instance(&pHndl);

    if (g_failures > 0) {
        log_error("%d AEL receive check(s) failed", g_failures);
        return 1;
    }

    log_info("All AEL receive validation checks passed");
    return 0;
}
