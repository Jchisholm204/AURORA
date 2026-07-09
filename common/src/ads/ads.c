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
#define ADS_LISTEN_N 64

#include "ads/ads.h"

#include "log.h"

#include <arpa/inet.h>
#include <malloc.h>
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct aurora_discovery_service_hndl {
    int tcp_sock;
};

extern ads_hndl *ads_init(void) {
    ads_hndl *pHndl = malloc(sizeof(ads_hndl));
    if (!pHndl) {
        log_error("Failed to allocate ADS Handle");
        return NULL;
    }

    pHndl->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ADS_CONN_PORT),
        .sin_addr = {INADDR_ANY},
    };
    int opt = 1;
    if (setsockopt(pHndl->tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR");
    }
    int ec = bind(pHndl->tcp_sock, (struct sockaddr *) &addr, sizeof(addr));
    if (ec != 0) {
        log_error("Bind to port %d failed eith e=%d", ADS_CONN_PORT, ec);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }
    ec = listen(pHndl->tcp_sock, ADS_LISTEN_N);
    if (ec != 0) {
        log_error("listener failed eith e=%d", ec);
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
    int max_fd = pHndl->tcp_sock;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(pHndl->tcp_sock, &readfds);
        select(max_fd + 1, &readfds, NULL, NULL, NULL);
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
    send(sock_fd, pTxData->comm.data, pTxData->comm.size, 0);
    send(sock_fd, pTxData->notif.data, pTxData->notif.size, 0);
    send(sock_fd, pTxData->config.data, pTxData->config.size, 0);

    ads_exchange_data_t *pRx = malloc(sizeof(ads_exchange_data_t));
    if (!pRx) {
        log_error("Failed to allocate RX buffer");
        return NULL;
    }

    { // BEGIN Exchange header struct
        ssize_t rx_bytes = 0;
        rx_bytes = recv(sock_fd, pRx, sizeof(ads_exchange_data_t), MSG_WAITALL);
        if (rx_bytes != sizeof(ads_exchange_data_t)) {
            log_error("Failed to correctly recv header");
        }
    } // END Exchange header struct

    // Ensure the pointers are zeroed
    pRx->comm.data = NULL;
    pRx->notif.data = NULL;

    // Recv exchange data
    { // BEGIN Comms Exchange
        pRx->comm.data = malloc(pRx->comm.size);
        if (!pRx->comm.data) {
            log_error("Failed to allocate comm key recv buffer");
            free(pRx);
            return NULL;
        }
        ssize_t rx_bytes =
            recv(sock_fd, pRx->comm.data, pRx->comm.size, MSG_WAITALL);
        if (rx_bytes != (ssize_t) pRx->comm.size) {
            log_error("Failed to correctly recv header");
            free(pRx->comm.data);
            free(pRx);
            return NULL;
        }
    } // END Comms Exchange

    { // BEGIN Notification keys exchange
        pRx->notif.data = malloc(pRx->notif.size);
        if (!pRx->notif.data) {
            log_error("Failed to allocate user data recv buffer");
            free(pRx->comm.data);
            free(pRx);
            return NULL;
        }

        ssize_t rx_bytes =
            recv(sock_fd, pRx->notif.data, pRx->notif.size, MSG_WAITALL);
        if (rx_bytes != (ssize_t) pRx->notif.size) {
            log_error("Failed to correctly recv header");
            free(pRx->comm.data);
            free(pRx->notif.data);
            free(pRx);
            return NULL;
        }
    } // END Notification keys exchange

    { // BEGIN Config Exchange
        pRx->config.data = malloc(pRx->config.size);
        if (!pRx->config.data) {
            log_error("Failed to allocate user data recv buffer");
            free(pRx->comm.data);
            free(pRx->notif.data);
            free(pRx);
            return NULL;
        }

        ssize_t rx_bytes =
            recv(sock_fd, pRx->config.data, pRx->config.size, MSG_WAITALL);
        if (rx_bytes != (ssize_t) pRx->config.size) {
            log_error("Failed to correctly recv header");
            free(pRx->comm.data);
            free(pRx->notif.data);
            free(pRx->config.data);
            free(pRx);
            return NULL;
        }
    } // END Notification keys exchange

    close(sock_fd);

    return pRx;
}

extern ads_exchange_data_t *ads_request_exchange(const ads_conf_t *pConf,
                                                 ads_exchange_data_t *pTxData) {
    if (!pConf || !pTxData) {
        log_warn("NULL Parameter");
        return NULL;
    }

    if (!pConf->opt_server_ip) {
        log_warn("NULL Parameter");
        return NULL;
    }

    log_trace("Connecting to %s", pConf->opt_server_ip);

    ads_conf_t conf;
    conf = *pConf;

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
        log_warn("Failed to connect to server socket");
        return NULL;
    }
    return ads_exchange(sd, pTxData);
}
