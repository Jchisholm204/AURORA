/**
 * @file test_ael_server.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AEL UDP server example.
 *
 * Binds a UDP socket to PORT and logs every struct ael_packet received.
 * Datagrams are read directly into struct ael_packet, so no application level
 * buffering or reassembly is performed.
 *
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-25
 *
 * @copyright Copyright (c) 2026
 */

#include "ael_cdefs.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void _ael_signal_handler(int signal) {
    (void) signal;
    g_stop = 1;
}

static eAEL_error _ael_server_recv(const struct ael_socket *sock,
                                   struct ael_packet *packet,
                                   struct sockaddr_in *peer);
static void _ael_log_packet(const struct ael_packet *packet,
                            const struct sockaddr_in *peer);

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    // Do not use SA_RESTART so recvfrom wakes up on Ctrl+C
    struct sigaction sa = {
        .sa_handler = _ael_signal_handler,
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct ael_socket sock;
    if (ael_socket_open(&sock, NULL, PORT) < 0) {
        return 1;
    }

    log_info("AEL UDP server listening on port %d (Ctrl+C to stop)", PORT);

    while (!g_stop) {
        struct ael_packet packet;
        struct sockaddr_in peer;

        eAEL_error rx_status = _ael_server_recv(&sock, &packet, &peer);
        if (rx_status == eAEL_OK) {
            _ael_log_packet(&packet, &peer);
        } else if (rx_status == eAEL_ERR_NULL || rx_status == eAEL_ERR_RECV) {
            break;
        }
    }

    ael_socket_close(&sock);
    log_info("AEL UDP server stopped");
    return 0;
}

static eAEL_error _ael_server_recv(const struct ael_socket *sock,
                                   struct ael_packet *packet,
                                   struct sockaddr_in *peer) {
    if (!sock || !packet || !peer) {
        log_error("AEL server recv: NULL parameter");
        return eAEL_ERR_NULL;
    }

    socklen_t peer_len = sizeof(*peer);
    ssize_t rx_bytes = recvfrom(sock->fd, packet, sizeof(*packet), MSG_TRUNC,
                                (struct sockaddr *) peer, &peer_len);
    if (rx_bytes < 0) {
        if (errno == EINTR) {
            return eAEL_ERR_INTR;
        }
        log_error("recvfrom failed: %s", strerror(errno));
        return eAEL_ERR_RECV;
    }
    if ((size_t) rx_bytes != sizeof(*packet)) {
        log_warn("Discarded %zd byte datagram (expected %zu bytes)", rx_bytes,
                 sizeof(*packet));
        return eAEL_ERR_SIZE;
    }
    if ((unsigned) packet->event >= (unsigned) eAEL_N_EVENT) {
        log_warn("Discarded packet with invalid event code %d",
                 (int) packet->event);
        return eAEL_ERR_EVENT;
    }

    return eAEL_OK;
}

static void _ael_log_packet(const struct ael_packet *packet,
                            const struct sockaddr_in *peer) {
    char peer_ip[INET_ADDRSTRLEN] = "unknown";
    if (!inet_ntop(AF_INET, &peer->sin_addr, peer_ip, sizeof(peer_ip))) {
        log_warn("Failed to format peer address: %s", strerror(errno));
    }

    log_info("rx rank=%" PRIu64 " group_id=%" PRIu64
             " group_size=%" PRIu64 " ev=%s from %s:%u",
             packet->rank, packet->group_id, packet->group_size,
             ael_event_name(packet->event), peer_ip,
             (unsigned) ntohs(peer->sin_port));

    switch (packet->event) {
    case eAEL_EVENT_CLI_CONNECT:
    case eAEL_EVENT_CLI_FAILURE:
    case eAEL_EVENT_CLI_DISCONNECT:
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        log_info("  event=\"%.*s\"", (int) sizeof(packet->event_string),
                 packet->event_string);
        break;
    case eAEL_EVENT_START_CHECKPOINT:
    case eAEL_EVENT_START_RESTORE:
    case eAEL_EVENT_END_CHECKPOINT:
    case eAEL_EVENT_END_RESTORE:
        log_info("  version=%" PRId64 " name=\"%.*s\" regions=%zu",
                 packet->checkpoint_version,
                 (int) sizeof(packet->checkpoint_name),
                 packet->checkpoint_name, packet->checkpoint_regions);
        break;
    default:
        break;
    }
}
