/**
 * @file ael_recv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash
 * @brief AURORA External Logger packet receive path.
 * @version 0.1
 * @date Created: 2026-10-27
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael.h"

#include "log.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

/**
 * @brief Decode a validated wire packet into a caller-facing message
 *
 * Packet contents are untrusted: the source can be any host on the network and
 * the CRC only protects integrity, so the string fields are force terminated
 * before they are handed out.
 */
static void _ael_decode(const ael_packet_t *packet,
                        const struct sockaddr_in *from, ael_message_t *msg) {
    assert(packet != NULL);
    assert(from != NULL);
    assert(msg != NULL);

    memset(msg, 0, sizeof(*msg));

    msg->rank = packet->rank;
    msg->group_id = packet->group_id;
    msg->timestamp_ns = packet->timestamp;
    msg->event = packet->event;

    msg->from_ip = ntohl(from->sin_addr.s_addr);
    msg->from_port = ntohs(from->sin_port);

    msg->payload = packet->payload;
    msg->payload.name[AEL_NAME_LEN - 1] = '\0';
    msg->event_message[AEL_EVENT_MSG_LEN - 1] = '\0';
}

eAEL_error ael_recv_message(ael_hndl *pHndl, ael_message_t *pMsg,
                            int timeout_ms) {
    if (!pHndl || !pMsg) {
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

    ael_packet_t packet;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    ssize_t rx_bytes = recvfrom(pHndl->socket.fd, &packet, sizeof(packet),
                                MSG_TRUNC, (struct sockaddr *) &from, &from_len);
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
    if ((size_t) rx_bytes != sizeof(ael_packet_t)) {
        log_warn("Discarded AEL datagram with invalid size: %zd (expected %zu)",
                 rx_bytes, sizeof(ael_packet_t));
        return eAEL_ERR_SIZE;
    }
    if (!ael_packet_verify(&packet)) {
        log_warn("Discarded AEL packet with invalid CRC");
        return eAEL_ERR_CRC;
    }
    if ((unsigned) packet.event >= (unsigned) eAEL_N_EVENT) {
        log_warn("Discarded AEL packet with invalid event code %d",
                 (int) packet.event);
        return eAEL_ERR_EVENT;
    }

    _ael_decode(&packet, &from, pMsg);

    return eAEL_OK;
}
