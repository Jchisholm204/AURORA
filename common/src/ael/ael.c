/**
 * @file ael.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AURORA External Logger endpoint implementation.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-27
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

ael_hndl *ael_create_instance(uint64_t rank, uint64_t group_id,
                              uint16_t local_port, size_t max_recipients) {
    ael_hndl *pHndl = calloc(1, sizeof(ael_hndl));
    if (!pHndl) {
        log_error("Bad Alloc??");
        return NULL;
    }

    pHndl->rank = rank;
    pHndl->group_id = group_id;
    pHndl->n_send_peers = max_recipients;
    pHndl->recipient_count = 0;
    pHndl->socket.fd = -1;
    pHndl->socket.port = local_port;

    if (max_recipients > 0) {
        pHndl->send_peers = calloc(max_recipients, sizeof(struct sockaddr_in));
        if (!pHndl->send_peers) {
            log_error("Bad Alloc??");
            ael_destroy_instance(&pHndl);
            return NULL;
        }

        memset(pHndl->send_peers, 0,
               max_recipients * sizeof(struct sockaddr_in));
    }

    pHndl->socket.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (pHndl->socket.fd < 0) {
        log_error("Failed to create AEL UDP socket: %s", strerror(errno));
        ael_destroy_instance(&pHndl);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(pHndl->socket.fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(int)) < 0) {
        log_warn("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(local_port),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
    };
    if (bind(pHndl->socket.fd, (struct sockaddr *) &local,
             sizeof(struct sockaddr_in)) < 0) {
        log_error("Failed to bind AEL UDP port %u: %s", (unsigned) local_port,
                  strerror(errno));
        ael_destroy_instance(&pHndl);
        return NULL;
    }

    return pHndl;
}

eAEL_error ael_add_recipient(ael_hndl *pHndl, const char *ip, uint16_t port) {
    if (!pHndl || !ip) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL | eAEL_ERR_FATAL;
    }
    if (port == 0) {
        log_error("Invalid AEL peer port");
        return eAEL_ERR_IP | eAEL_ERR_FATAL;
    }

    if (pHndl->recipient_count >= pHndl->n_send_peers) {
        log_error("Max recipients limit reached");
        return eAEL_ERR_COUNT | eAEL_ERR_FATAL;
    }

    struct sockaddr_in addr = {.sin_family = AF_INET};
    int rc = inet_pton(AF_INET, ip, &addr.sin_addr);
    if (rc != 1) {
        log_error("Invalid AEL peer IP '%s'", ip);
        return eAEL_ERR_IP | eAEL_ERR_FATAL;
    }

    addr.sin_port = htons(port);
    pHndl->send_peers[pHndl->recipient_count++] = addr;

    return eAEL_OK;
}

void ael_destroy_instance(ael_hndl **ppHndl) {
    if (!ppHndl) {
        log_error("NULL Parameter");
        return;
    }
    ael_hndl *pHndl = *ppHndl;
    if (!pHndl) {
        log_error("NULL Parameter");
        return;
    }

    if (pHndl->socket.fd >= 0) {
        if (close(pHndl->socket.fd) != 0) {
            log_error("Failed to close AEL UDP socket: %s", strerror(errno));
        }
        pHndl->socket.fd = -1;
    }
    if (pHndl->send_peers) {
        free(pHndl->send_peers);
        pHndl->send_peers = NULL;
    }

    free(pHndl);
    *ppHndl = NULL;
}
