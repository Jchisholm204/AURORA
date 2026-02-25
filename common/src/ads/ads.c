/**
 * @file ads.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Active Discovery Service
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#define MDNS_BUF_SIZE 2048
#define ADS_CONN_PORT 9063
#define ADS_LISTEN_PORT 9064
#define ADS_LISTEN_N 64

#include "ads/ads.h"

#include "log.h"
#include "mdns.h"

#include <arpa/inet.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct aurora_discovery_service_hndl {
    int mdns_sock;
    int tcp_sock;
    uint8_t mdns_buf[MDNS_BUF_SIZE];
};

const char servicename[] = "_aurora._tcp.local";
static int _ads_mdns_callback(int sock, const struct sockaddr *from,
                              size_t addrlen, mdns_entry_type_t entry,
                              uint16_t query_id, uint16_t rtype,
                              uint16_t rclass, uint32_t ttl, const void *data,
                              size_t size, size_t name_offset,
                              size_t name_length, size_t record_offset,
                              size_t record_length, void *user_data);
static int _client_discovery_callback(int sock, const struct sockaddr *from,
                                      size_t addrlen, mdns_entry_type_t entry,
                                      uint16_t query_id, uint16_t rtype,
                                      uint16_t rclass, uint32_t ttl,
                                      const void *data, size_t size,
                                      size_t name_offset, size_t name_length,
                                      size_t record_offset,
                                      size_t record_length, void *user_data);

extern ads_hndl *ads_init(void) {
    ads_hndl *pHndl = malloc(sizeof(ads_hndl));
    if (!pHndl) {
        log_error("Failed to allocate ADS Handle");
        return NULL;
    }

    pHndl->mdns_sock = mdns_socket_open_ipv4(NULL);
    mdns_socket_setup_ipv4(pHndl->mdns_sock, NULL);

    int loop = 1;
    if (setsockopt(pHndl->mdns_sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
                   sizeof(loop)) < 0) {
        log_error("Failed to set IP_MULTICAST_LOOP");
    }

    pHndl->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ADS_CONN_PORT),
        .sin_addr = {INADDR_ANY},
    };
    int ec = bind(pHndl->tcp_sock, (struct sockaddr *) &addr, sizeof(addr));
    if (ec != 0) {
        log_error("Bind to port %d failed eith e=%d", ADS_CONN_PORT, ec);
        mdns_socket_close(pHndl->mdns_sock);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }
    ec = listen(pHndl->tcp_sock, ADS_LISTEN_N);
    if (ec != 0) {
        log_error("listener failed eith e=%d", ec);
        mdns_socket_close(pHndl->mdns_sock);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }

    return pHndl;
}

extern int ads_finalize(ads_hndl **ppHndl) {
    if (!ppHndl)
        return -1;
    if (!*ppHndl)
        return -1;
    mdns_socket_close((*ppHndl)->mdns_sock);
    close((*ppHndl)->tcp_sock);
    free((*ppHndl));
    *ppHndl = NULL;
    return 0;
}

extern int ads_accept_any(ads_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }
    fd_set readfds;
    int max_fd = (pHndl->mdns_sock > pHndl->tcp_sock) ? pHndl->mdns_sock
                                                      : pHndl->tcp_sock;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(pHndl->mdns_sock, &readfds);
        FD_SET(pHndl->tcp_sock, &readfds);
        select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(pHndl->mdns_sock, &readfds)) {
            mdns_socket_listen(pHndl->mdns_sock, pHndl->mdns_buf, MDNS_BUF_SIZE,
                               _ads_mdns_callback, pHndl);
        }
        if (FD_ISSET(pHndl->tcp_sock, &readfds)) {
            return accept(pHndl->tcp_sock, NULL, NULL);
        }
    }
}

extern ads_exchange_data_t *ads_exchange(int sock_fd,
                                         ads_exchange_data_t *pTxData) {
    if (!pTxData) {
        return NULL;
    }
    // Transmit Data to client side
    send(sock_fd, pTxData, sizeof(ads_exchange_data_t), 0);
    send(sock_fd, pTxData->comm_key, pTxData->ck_size, 0);
    send(sock_fd, pTxData->user_data, pTxData->ud_size, 0);

    ads_exchange_data_t *pRx = malloc(sizeof(ads_exchange_data_t));
    if (!pRx) {
        log_error("Failed to allocate RX buffer");
        return NULL;
    }
    ssize_t rx_bytes = 0;
    rx_bytes = recv(sock_fd, pRx, sizeof(ads_exchange_data_t), MSG_WAITALL);
    if (rx_bytes != sizeof(ads_exchange_data_t)) {
        log_error("Failed to correctly recv header");
    }

    // Ensure the pointers are zeroed
    pRx->comm_key = NULL;
    pRx->user_data = NULL;

    // Recv exchange data
    pRx->comm_key = malloc(pRx->ck_size);
    if (!pRx->comm_key) {
        log_error("Failed to allocate comm key recv buffer");
        free(pRx);
        return NULL;
    }
    rx_bytes = recv(sock_fd, pRx->comm_key, pRx->ck_size, MSG_WAITALL);
    if (rx_bytes != (ssize_t) pRx->ck_size) {
        log_error("Failed to correctly recv header");
        free(pRx->comm_key);
        free(pRx);
        return NULL;
    }
    pRx->user_data = malloc(pRx->ud_size);
    if (!pRx->user_data) {
        log_error("Failed to allocate user data recv buffer");
        free(pRx->comm_key);
        free(pRx);
        return NULL;
    }
    rx_bytes = recv(sock_fd, pRx->user_data, pRx->ud_size, MSG_WAITALL);
    if (rx_bytes != (ssize_t) pRx->ud_size) {
        log_error("Failed to correctly recv header");
        free(pRx->user_data);
        free(pRx->comm_key);
        free(pRx);
        return NULL;
    }

    close(sock_fd);

    return pRx;
}

extern ads_exchange_data_t *ads_request_exchange(const ads_conf_t *pConf,
                                                 ads_exchange_data_t *pTxData) {
    if (!pConf || !pTxData) {
        return NULL;
    }
    char target_ip[64];
    ads_conf_t conf;
    conf = *pConf;
    if (conf.opt_server_ip == NULL) {
        log_trace("Using Dynamic Discovery to find server");
        conf.opt_server_ip = target_ip;
        // Change NULL to a zeroed sockaddr to bind to a random port
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = 0; // The OS will pick a random port
        int sock = mdns_socket_open_ipv4(&client_addr);
        if (sock < 0) {
            log_error("Failed to open the socket");
            return NULL;
        }
        int loop = 1;
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
        mdns_query_send(sock, MDNS_RECORDTYPE_A, servicename,
                        strlen(servicename), NULL, 0, 0);
        uint8_t buffer[MDNS_BUF_SIZE];
        int found = mdns_query_recv(sock, buffer, sizeof(buffer),
                                    _client_discovery_callback, &conf,
                                    pConf->timeout_ms);
        mdns_socket_close(sock);
        if (found <= 0) {
            log_error("Failed find the server e=%d", found);
            return NULL;
        }
    }

    struct timeval tv = {
        .tv_sec = conf.timeout_ms / 1000,
        .tv_usec = conf.timeout_ms % 1000,
    };

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    // Socket timeout
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
    setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &tv, sizeof(tv));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ADS_CONN_PORT),
    };
    inet_pton(AF_INET, pConf->opt_server_ip, &addr.sin_addr);
    if (connect(sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        return NULL;
    }
    return ads_exchange(sd, pTxData);
}

static int _ads_mdns_callback(int sock, const struct sockaddr *from,
                              size_t addrlen, mdns_entry_type_t entry,
                              uint16_t query_id, uint16_t rtype,
                              uint16_t rclass, uint32_t ttl, const void *data,
                              size_t size, size_t name_offset,
                              size_t name_length, size_t record_offset,
                              size_t record_length, void *user_data) {

    (void) rtype;
    (void) rclass;
    (void) ttl;
    (void) name_length;
    (void) record_offset;
    (void) record_length;
    ads_hndl *pHndl = (ads_hndl *) user_data;
    if (entry != MDNS_ENTRYTYPE_QUESTION)
        return 0;

    char namebuffer[256];
    mdns_string_t name = mdns_string_extract(data, size, &name_offset,
                                             namebuffer, sizeof(namebuffer));

    // Match your specific service name
    if (strcmp(namebuffer, servicename) == 0) {
        // Construct the IPv4 address record
        struct sockaddr_in addr_ipv4;
        // For BF internal link, this is usually 192.168.100.2
        // addr_ipv4.sin_addr.s_addr = inet_addr("192.168.100.2");
        addr_ipv4.sin_addr.s_addr = inet_addr("127.0.0.1");

        mdns_record_t answer = {
            .name = {servicename, strlen(servicename)},
            .type = MDNS_RECORDTYPE_A,
            .data = {.a = {.addr = {addr_ipv4.sin_addr.s_addr, 0, {0}, {0}}}},
        };

        mdns_query_answer_unicast(sock, from, addrlen, pHndl->mdns_buf,
                                  MDNS_BUF_SIZE, query_id, MDNS_RECORDTYPE_A,
                                  namebuffer, name.length, answer, NULL, 0,
                                  NULL, 0);
    }
    return 0;
}

static int _client_discovery_callback(int sock, const struct sockaddr *from,
                                      size_t addrlen, mdns_entry_type_t entry,
                                      uint16_t query_id, uint16_t rtype,
                                      uint16_t rclass, uint32_t ttl,
                                      const void *data, size_t size,
                                      size_t name_offset, size_t name_length,
                                      size_t record_offset,
                                      size_t record_length, void *user_data) {

    (void) sock;
    (void) from;
    (void) addrlen;
    (void) query_id;
    (void) entry;
    (void) rclass;
    (void) ttl;
    (void) name_offset;
    (void) name_length;
    log_trace("Callback triggered: Entry Type %d, Record Type %d", entry,
              rtype);
    ads_conf_t *pConf = (ads_conf_t *) user_data;

    // We only care about the Answer (RESPONSE), not the Question
    if (entry != MDNS_ENTRYTYPE_ANSWER)
        return 0;

    // We specifically want an IPv4 address (A Record)
    if (rtype == MDNS_RECORDTYPE_A) {
        struct sockaddr_in addr;
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);

        // Copy the found IP string into our user_data buffer
        inet_ntop(AF_INET, &addr.sin_addr, pConf->opt_server_ip, 16);

        return 1; // Returning 1 tells mdns.h to stop the loop immediately
    }
    return 0;
}
