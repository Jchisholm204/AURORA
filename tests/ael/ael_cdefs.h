/**
 * @file ael_cdefs.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Shared AEL packet definitions and helpers.
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_CDEFS_H_
#define _AEL_CDEFS_H_

#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8334

enum eAEL_event {
    eAEL_EVENT_CLI_CONNECT,
    eAEL_EVENT_CLI_FAILURE,
    eAEL_EVENT_CLI_DISCONNECT,
    eAEL_EVENT_START_CHECKPOINT,
    eAEL_EVENT_START_RESTORE,
    eAEL_EVENT_END_CHECKPOINT,
    eAEL_EVENT_END_RESTORE,
    eAEL_EVENT_REGION_ADD,
    eAEL_EVENT_REGION_REMOVE,
    eAEL_N_EVENT
};

enum eAEL_error {
    eAEL_OK = 0,
    eAEL_ERR_NULL,
    eAEL_ERR_INTR,
    eAEL_ERR_SEND,
    eAEL_ERR_RECV,
    eAEL_ERR_SIZE,
    eAEL_ERR_EVENT,
    eAEL_N_ERR,
};

typedef enum eAEL_error eAEL_error;

struct ael_packet {
    uint64_t rank;
    uint64_t group_id;
    uint64_t group_size;
    enum eAEL_event event;
    int64_t checkpoint_version;
    size_t checkpoint_regions;
    char checkpoint_name[16];
    char event_string[256];
};

struct ael_socket {
    int fd;
    uint16_t port;
    struct sockaddr_in addr;
};

/**
 * @brief Get the printable name of an event code
 *
 * @param code Event code to name
 * @return Constant event name string
 */
static inline const char *ael_event_name(enum eAEL_event code) {
    switch (code) {
    case eAEL_EVENT_CLI_CONNECT:
        return "eAEL_EVENT_CLI_CONNECT";
    case eAEL_EVENT_CLI_FAILURE:
        return "eAEL_EVENT_CLI_FAILURE";
    case eAEL_EVENT_CLI_DISCONNECT:
        return "eAEL_EVENT_CLI_DISCONNECT";
    case eAEL_EVENT_START_CHECKPOINT:
        return "eAEL_EVENT_START_CHECKPOINT";
    case eAEL_EVENT_START_RESTORE:
        return "eAEL_EVENT_START_RESTORE";
    case eAEL_EVENT_END_CHECKPOINT:
        return "eAEL_EVENT_END_CHECKPOINT";
    case eAEL_EVENT_END_RESTORE:
        return "eAEL_EVENT_END_RESTORE";
    case eAEL_EVENT_REGION_ADD:
        return "eAEL_EVENT_REGION_ADD";
    case eAEL_EVENT_REGION_REMOVE:
        return "eAEL_EVENT_REGION_REMOVE";
    default:
        return "unknown";
    }
}

/**
 * @brief Select an event code by sequence index
 *
 * Cycles through every event code in the enum.
 *
 * @param index Sequence index
 * @return Event code for the given index
 */
static inline enum eAEL_event ael_event_code(unsigned long index) {
    static const enum eAEL_event codes[] = {
        eAEL_EVENT_CLI_CONNECT,
        eAEL_EVENT_CLI_FAILURE,
        eAEL_EVENT_CLI_DISCONNECT,
        eAEL_EVENT_START_CHECKPOINT,
        eAEL_EVENT_START_RESTORE,
        eAEL_EVENT_END_CHECKPOINT,
        eAEL_EVENT_END_RESTORE,
        eAEL_EVENT_REGION_ADD,
        eAEL_EVENT_REGION_REMOVE,
    };
    return codes[index % (sizeof(codes) / sizeof(codes[0]))];
}

/**
 * @brief Close an AEL socket
 *
 * @param sock Socket handle to close
 */
static inline void ael_socket_close(struct ael_socket *sock) {
    if (!sock) {
        return;
    }
    if (sock->fd >= 0) {
        close(sock->fd);
    }
    sock->fd = -1;
}

/**
 * @brief Initialize a client UDP socket
 *
 * @param sock Socket handle with fd and address already initialized
 * @param dest_ip Destination IPv4 address
 * @return 0 on success, -1 on failure
 */
static inline int _ael_socket_init_client(struct ael_socket *sock,
                                          const char *dest_ip) {
    if (inet_pton(AF_INET, dest_ip, &sock->addr.sin_addr) != 1) {
        log_error("Invalid IPv4 address '%s'", dest_ip);
        return -1;
    }
    return 0;
}

/**
 * @brief Initialize a server UDP socket
 *
 * @param sock Socket handle with fd and address already initialized
 * @return 0 on success, -1 on failure
 */
static inline int _ael_socket_init_server(struct ael_socket *sock) {
    sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    if (bind(sock->fd, (struct sockaddr *) &sock->addr, sizeof(sock->addr)) <
        0) {
        log_error("Failed to bind UDP port %u: %s", (unsigned) sock->port,
                  strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief Open a UDP socket as either a client or a server
 *
 * If dest_ip is set the socket is configured to send to that destination. If
 * dest_ip is NULL the socket is bound to the given port on INADDR_ANY.
 *
 * @param sock Socket handle to initialize
 * @param dest_ip Destination IPv4 address, or NULL for a server socket
 * @param port UDP port to send to or bind
 * @return 0 on success, -1 on failure
 */
static inline int ael_socket_open(struct ael_socket *sock, const char *dest_ip,
                                  uint16_t port) {
    if (!sock) {
        log_error("AEL socket handle is NULL");
        return -1;
    }

    sock->fd = -1;
    sock->port = port;
    sock->addr = (struct sockaddr_in) {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->fd < 0) {
        log_error("Failed to create UDP socket: %s", strerror(errno));
        return -1;
    }

    int rc;
    if (dest_ip) {
        rc = _ael_socket_init_client(sock, dest_ip);
    } else {
        rc = _ael_socket_init_server(sock);
    }
    if (rc < 0) {
        ael_socket_close(sock);
        return -1;
    }

    return 0;
}

#endif
