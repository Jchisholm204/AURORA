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
#include "mdns.h"

#include <malloc.h>
#include <stdint.h>

struct aurora_discovery_service_hndl {
    int mdns_sock;
    int tcp_sock;
    uint8_t mdns_buf[MDNS_BUF_SIZE];
};

extern ads_hndl *ads_init(void) {
    ads_hndl *pHndl = malloc(sizeof(ads_hndl));
    if (!pHndl) {
        log_error("Failed to allocate ADS Handle");
        return NULL;
    }

    pHndl->mdns_sock = mdns_socket_open_ipv4(NULL);
    mdns_socket_setup_ipv4(pHndl->mdns_sock, NULL);

    pHndl->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ADS_CONN_PORT),
        .sin_addr = INADDR_ANY,
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
    if(!ppHndl) return -1;
    if(!*ppHndl) return -1;
    mdns_socket_close((*ppHndl)->mdns_sock);
    close((*ppHndl)->tcp_sock);
    free((*ppHndl));
    *ppHndl = NULL;
    return 0;
}

extern int ads_accept_any(ads_hndl *pHndl) {
}

extern struct ads_exchange_data *ads_accept_exchange(
    ads_hndl *pHndl, int sock_fd, struct ads_exchange_data *pTxData) {
}

extern struct ads_exchange_data *ads_request_exchange(
    const char *server_ip, int timeout, struct ads_exchange_data *pTxData) {
}
