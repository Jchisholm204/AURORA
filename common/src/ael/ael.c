/**
 * @file ael.c
 * @author Connor Leung
 * @brief AURORA External Logger
 * @version 0.1
 * @date Created: 2026-11-02
 * @modified Last Modified: 2026-11-02
 *
 * @copyright Copyright (c) 2026
 */

#include "ael/ael.h"

#include "log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct aurora_external_logger_hndl {
    int sock;
    struct sockaddr_storage target;
    socklen_t target_len;
};

extern ael_hndl *ael_init(const char *host, uint16_t port) {
    if (!host) {
        log_error("NULL Parameter");
        return NULL;
    }

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    struct addrinfo *pRes = NULL;
    int ec = getaddrinfo(host, port_str, &hints, &pRes);
    if (ec != 0 || !pRes) {
        log_error("AEL failed to resolve %s:%u (%s)", host, port,
                  gai_strerror(ec));
        return NULL;
    }

    ael_hndl *pHndl = malloc(sizeof(ael_hndl));
    if (!pHndl) {
        log_error("Failed to allocate AEL Handle");
        freeaddrinfo(pRes);
        return NULL;
    }

    pHndl->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (pHndl->sock < 0) {
        log_error("AEL failed to open socket");
        freeaddrinfo(pRes);
        free(pHndl);
        return NULL;
    }

    memcpy(&pHndl->target, pRes->ai_addr, pRes->ai_addrlen);
    pHndl->target_len = pRes->ai_addrlen;
    freeaddrinfo(pRes);

    log_info("AEL emitting to %s:%u", host, port);

    return pHndl;
}

extern ael_hndl *ael_init_from_env(void) {
    const char *host = getenv(AEL_ENV_HOST);
    if (!host || host[0] == '\0') {
        return NULL;
    }
    uint16_t port = AEL_DEFAULT_PORT;
    const char *port_str = getenv(AEL_ENV_PORT);
    if (port_str && port_str[0] != '\0') {
        int p = atoi(port_str);
        if (p > 0 && p <= 65535) {
            port = (uint16_t) p;
        } else {
            log_warn("Invalid %s=%s, using %d", AEL_ENV_PORT, port_str,
                     AEL_DEFAULT_PORT);
        }
    }
    return ael_init(host, port);
}

extern int ael_finalize(ael_hndl **ppHndl) {
    if (!ppHndl)
        return -1;
    if (!*ppHndl)
        return -1;
    close((*ppHndl)->sock);
    free(*ppHndl);
    *ppHndl = NULL;
    return 0;
}

extern int ael_emit(ael_hndl *pHndl, ael_packet_t *pPacket) {
    if (!pHndl) {
        // Disabled
        return 0;
    }
    if (!pPacket) {
        log_warn("NULL Parameter");
        return -1;
    }

    memcpy(pPacket->magic, AEL_MAGIC, AEL_MAGIC_LEN);
    pPacket->_reserved = 0;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    pPacket->timestamp_ns =
        (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;

    ssize_t tx = sendto(pHndl->sock, pPacket, sizeof(ael_packet_t), 0,
                        (struct sockaddr *) &pHndl->target, pHndl->target_len);
    if (tx != (ssize_t) sizeof(ael_packet_t)) {
        log_debug("AEL send failed");
        return -1;
    }
    return 0;
}

extern bool ael_packet_valid(const void *pData, size_t size) {
    if (!pData || size < AEL_CTRL_SIZE) {
        return false;
    }
    const ael_packet_t *pPacket = pData;
    if (memcmp(pPacket->magic, AEL_MAGIC, AEL_MAGIC_LEN) != 0) {
        return false;
    }
    switch (pPacket->type) {
    case eAEL_SUBSCRIBE:
    case eAEL_UNSUBSCRIBE:
        return true;
    case eAEL_CONNECT:
    case eAEL_CHECKPOINT:
    case eAEL_RESTART:
    case eAEL_DISCONNECT:
        return size == sizeof(ael_packet_t);
    default:
        return false;
    }
}

extern const char *ael_type_str(uint8_t type) {
    switch (type) {
    case eAEL_CONNECT:
        return "CONNECT";
    case eAEL_CHECKPOINT:
        return "CHECKPOINT";
    case eAEL_RESTART:
        return "RESTART";
    case eAEL_DISCONNECT:
        return "DISCONNECT";
    case eAEL_SUBSCRIBE:
        return "SUBSCRIBE";
    case eAEL_UNSUBSCRIBE:
        return "UNSUBSCRIBE";
    default:
        return "UNKNOWN";
    }
}
