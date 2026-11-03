/**
 * @file main.c
 * @author Connor Leung
 * @brief AURORA External Logger relay server
 * @version 0.1
 * @date Created: 2026-11-02
 * @modified Last Modified: 2026-11-02
 *
 * @copyright Copyright (c) 2026
 *
 * Receives AEL events from any number of senders on a single UDP port and
 * forwards them to every subscriber using the same socket.
 */

#include "ael/ael.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Room for oversized/garbage packets so they can be rejected
#define AEL_RX_BUF_SIZE 512

struct ael_subscriber_list {
    struct sockaddr_in *addrs;
    size_t n;
    size_t cap;
};

static bool _addr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port == b->sin_port;
}

static ssize_t _sub_find(const struct ael_subscriber_list *pList,
                         const struct sockaddr_in *pAddr) {
    for (size_t i = 0; i < pList->n; i++) {
        if (_addr_eq(&pList->addrs[i], pAddr)) {
            return (ssize_t) i;
        }
    }
    return -1;
}

static int _sub_add(struct ael_subscriber_list *pList,
                    const struct sockaddr_in *pAddr) {
    if (_sub_find(pList, pAddr) >= 0) {
        return 0;
    }
    if (pList->n == pList->cap) {
        size_t new_cap = pList->cap ? pList->cap * 2 : 8;
        struct sockaddr_in *pNew =
            realloc(pList->addrs, new_cap * sizeof(struct sockaddr_in));
        if (!pNew) {
            log_error("Failed to grow subscriber list");
            return -1;
        }
        pList->addrs = pNew;
        pList->cap = new_cap;
    }
    pList->addrs[pList->n++] = *pAddr;
    return 1;
}

static int _sub_remove(struct ael_subscriber_list *pList,
                       const struct sockaddr_in *pAddr) {
    ssize_t idx = _sub_find(pList, pAddr);
    if (idx < 0) {
        return 0;
    }
    pList->addrs[idx] = pList->addrs[pList->n - 1];
    pList->n--;
    return 1;
}

static void _print_event(FILE *pOut, const ael_packet_t *pPacket) {
    fprintf(pOut, "%lu %s %s rank=%lu group=%lu/%ld version=%ld name=%.*s "
                  "duration_ms=%.3f\n",
            (unsigned long) pPacket->timestamp_ns, ael_type_str(pPacket->type),
            pPacket->status == eAEL_STATUS_OK ? "OK" : "FAIL",
            (unsigned long) pPacket->rank, (unsigned long) pPacket->group_id,
            (long) pPacket->group_size, (long) pPacket->version,
            (int) strnlen(pPacket->name, AEL_NAME_LEN), pPacket->name,
            pPacket->duration_ms);
    fflush(pOut);
}

int main(int argc, char **argv) {
    uint16_t port = AEL_DEFAULT_PORT;
    FILE *pEventFile = NULL;

    if (argc >= 2) {
        int p = atoi(argv[1]);
        if (p <= 0 || p > 65535) {
            fprintf(stderr, "usage: %s [port] [event_log_file]\n", argv[0]);
            return 1;
        }
        port = (uint16_t) p;
    }
    if (argc >= 3) {
        pEventFile = fopen(argv[2], "a");
        if (!pEventFile) {
            log_error("Failed to open event log file %s", argv[2]);
            return 1;
        }
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_fatal("Failed to open socket");
        return 1;
    }
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {INADDR_ANY},
    };
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        log_fatal("Bind to port %d failed: %s", port, strerror(errno));
        close(sock);
        return 1;
    }

    log_info("AEL relay listening on UDP port %d", port);

    struct ael_subscriber_list subs = {0};
    uint8_t buf[AEL_RX_BUF_SIZE];

    while (true) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t rx = recvfrom(sock, buf, sizeof(buf), 0,
                              (struct sockaddr *) &from, &from_len);
        if (rx < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("recvfrom failed: %s", strerror(errno));
            continue;
        }

        char from_str[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));

        if (!ael_packet_valid(buf, (size_t) rx)) {
            log_debug("Dropped invalid packet (%ld bytes) from %s:%d", rx,
                      from_str, ntohs(from.sin_port));
            continue;
        }

        const ael_packet_t *pPacket = (const ael_packet_t *) buf;

        if (pPacket->type == eAEL_SUBSCRIBE) {
            if (_sub_add(&subs, &from) > 0) {
                log_info("Subscribed %s:%d (%zu total)", from_str,
                         ntohs(from.sin_port), subs.n);
            }
            continue;
        }
        if (pPacket->type == eAEL_UNSUBSCRIBE) {
            if (_sub_remove(&subs, &from) > 0) {
                log_info("Unsubscribed %s:%d (%zu total)", from_str,
                         ntohs(from.sin_port), subs.n);
            }
            continue;
        }

        // Event: record and fan out
        _print_event(stdout, pPacket);
        if (pEventFile) {
            _print_event(pEventFile, pPacket);
        }
        for (size_t i = 0; i < subs.n; i++) {
            ssize_t tx = sendto(sock, buf, sizeof(ael_packet_t), 0,
                                (struct sockaddr *) &subs.addrs[i],
                                sizeof(subs.addrs[i]));
            if (tx != (ssize_t) sizeof(ael_packet_t)) {
                log_debug("Forward to subscriber %zu failed", i);
            }
        }
    }

    free(subs.addrs);
    if (pEventFile) {
        fclose(pEventFile);
    }
    close(sock);
    return 0;
}
