/**
 * @file ael.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AURORA External Logger endpoint implementation.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael.h"

#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

eAEL_error _ael_add_recipient(ael_hndl *pHndl, const char *ip, uint16_t port);

ael_hndl *ael_create_instance(uint64_t rank, uint64_t group_id,
                              uint64_t group_size, uint16_t local_port,
                              const ael_peer_t *send_peers,
                              size_t n_send_peers) {
    if (n_send_peers > 0 && !send_peers) {
        log_error("AEL send peer list is NULL");
        return NULL;
    }

    ael_hndl *pHndl = calloc(1, sizeof(ael_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->rank = rank;
    pHndl->group_id = group_id;
    pHndl->group_size = group_size;
    pHndl->n_send_peers = n_send_peers;
    pHndl->recipient_count = 0;
    pHndl->socket.fd = -1;
    pHndl->socket.port = local_port;

    if (n_send_peers > 0) {
        pHndl->send_peers = calloc(n_send_peers, sizeof(*pHndl->send_peers));
        if (!pHndl->send_peers) {
            log_error("Bad Alloc??");
            ael_destroy_instance(&pHndl);
            return NULL;
        }

        for (size_t i = 0; i < n_send_peers; i++) {
            const ael_peer_t *peer = &send_peers[i];

            if (!peer->ip || peer->port == 0) {
                log_error("Invalid AEL send peer %zu", i);
                ael_destroy_instance(&pHndl);
                return NULL;
            }

            eAEL_error status = _ael_add_recipient(pHndl, peer->ip, peer->port);
            if (status != eAEL_OK) {
                ael_destroy_instance(&pHndl);
                return NULL;
            }
        }
    }

    pHndl->socket.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (pHndl->socket.fd < 0) {
        log_error("Failed to create AEL UDP socket: %s", strerror(errno));
        ael_destroy_instance(&pHndl);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(pHndl->socket.fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(local_port),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
    };
    if (bind(pHndl->socket.fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
        log_error("Failed to bind AEL UDP port %u: %s", (unsigned) local_port,
                  strerror(errno));
        ael_destroy_instance(&pHndl);
        return NULL;
    }

    return pHndl;
}

/**
 * @brief Validate a single recipient peer and populate its sockaddr_in.
 *
 * Performs null checking, validates the IP address and port, then stores the
 * populated sockaddr_in structure in pHndl->send_peers[pHndl->recipient_count].
 * This function exists to encapsulate per-peer validation logic that is called
 * for each entry in the send_peers array during instance creation.
 *
 * @param pHndl AEL handle with socket already created and bound
 * @param ip Recipient peer IP address (IPv4 dotted-decimal string)
 * @param port Recipient peer port in host byte order
 * @return eAEL_OK on success, or appropriate error code on failure
 */
eAEL_error _ael_add_recipient(ael_hndl *pHndl, const char *ip, uint16_t port) {
    if (!pHndl || !ip || port == 0) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }

    struct sockaddr_in addr = {.sin_family = AF_INET};
    int rc = inet_pton(AF_INET, ip, &addr.sin_addr);
    if (rc != 1) {
        log_error("Invalid AEL peer IP '%s'", ip);
        return eAEL_ERR_SOCKET;
    }

    addr.sin_port = htons(port);
    pHndl->send_peers[pHndl->recipient_count++] =
        addr; // Store populated address and increment counter

    return eAEL_OK;
}

eAEL_error ael_destroy_instance(ael_hndl **ppHndl) {
    if (!ppHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    ael_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }

    if (pHndl->socket.fd >= 0) {
        close(pHndl->socket.fd);
        pHndl->socket.fd = -1;
    }
    if (pHndl->send_peers) {
        free(pHndl->send_peers);
        pHndl->send_peers = NULL;
    }

    free(pHndl);
    *ppHndl = NULL;
    return eAEL_OK;
}

eAEL_error ael_send_packet(ael_hndl *pHndl, const ael_packet_t *packet) {
    if (!pHndl || !packet) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (pHndl->socket.fd < 0) {
        log_error("AEL send: closed socket");
        return eAEL_ERR_SOCKET;
    }
    if (pHndl->n_send_peers == 0 || !pHndl->send_peers) {
        log_error("AEL send: no send peers configured");
        return eAEL_ERR_SEND;
    }

    eAEL_error status = eAEL_OK;
    for (size_t i = 0; i < pHndl->n_send_peers; i++) {
        const struct sockaddr_in *peer = &pHndl->send_peers[i];
        ssize_t tx_bytes =
            sendto(pHndl->socket.fd, packet, sizeof(*packet), 0,
                   (const struct sockaddr *) peer, sizeof(*peer));

        if (tx_bytes < 0) {
            log_error("AEL sendto peer %zu failed: %s", i, strerror(errno));
            if (status == eAEL_OK) {
                status = eAEL_ERR_SEND;
            }
            continue;
        }
        if ((size_t) tx_bytes != sizeof(*packet)) {
            log_error("AEL short send to peer %zu: %zd of %zu bytes", i,
                      tx_bytes, sizeof(*packet));
            if (status == eAEL_OK) {
                status = eAEL_ERR_SIZE;
            }
        }
    }

    return status;
}

eAEL_error ael_recv_packet(ael_hndl *pHndl, ael_packet_t *packet,
                           int timeout_ms) {
    if (!pHndl || !packet) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (pHndl->socket.fd < 0) {
        log_error("AEL recv: closed socket");
        return eAEL_ERR_SOCKET;
    }

    struct pollfd pfd = {
        .fd = pHndl->socket.fd,
        .events = POLLIN,
    };
    int poll_status = poll(&pfd, 1, timeout_ms);
    if (poll_status == 0) {
        return eAEL_ERR_TIMEOUT;
    }
    if (poll_status < 0) {
        if (errno == EINTR) {
            return eAEL_ERR_INTR;
        }
        log_error("AEL poll failed: %s", strerror(errno));
        return eAEL_ERR_RECV;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        log_error("AEL poll reported socket error 0x%x",
                  (unsigned) pfd.revents);
        return eAEL_ERR_RECV;
    }
    if (!(pfd.revents & POLLIN)) {
        log_info("AEL recv: no data available");
        return eAEL_ERR_TIMEOUT;
    }

    ssize_t rx_bytes = recvfrom(pHndl->socket.fd, packet, sizeof(*packet),
                                MSG_TRUNC, NULL, NULL);
    if (rx_bytes < 0) {
        if (errno == EINTR) {
            log_debug("AEL recv: interrupted");
            return eAEL_ERR_INTR;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return eAEL_ERR_TIMEOUT;
        }
        log_error("AEL recvfrom failed: %s", strerror(errno));
        return eAEL_ERR_RECV;
    }
    if ((size_t) rx_bytes != sizeof(*packet)) {
        log_warn("Discarded AEL datagram with invalid size: %zd (expected %zu)",
                 rx_bytes, sizeof(*packet));
        return eAEL_ERR_SIZE;
    }
    if (!ael_packet_verify(packet)) {
        log_warn("Discarded AEL packet with invalid CRC");
        return eAEL_ERR_CRC;
    }
    if ((unsigned) packet->event >= (unsigned) eAEL_N_EVENT) {
        log_warn("Discarded AEL packet with invalid event code %d",
                 (int) packet->event);
        return eAEL_ERR_EVENT;
    }

    return eAEL_OK;
}
