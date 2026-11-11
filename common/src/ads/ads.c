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
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pmix.h>

struct aurora_discovery_service_hndl {
    int tcp_sock;
    pmix_proc_t pmix_proc;
};

extern ads_hndl *ads_init(void) {
    
    ads_hndl *pHndl = malloc(sizeof(ads_hndl));
    if (!pHndl) {
        log_error("Failed to allocated ADS Handle");
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

    struct sockaddr_in pmix_addr;
    socklen_t pmix_addr_len = sizeof(pmix_addr);
    ec = getsockname(pHndl->tcp_sock, (struct sockaddr *)&pmix_addr, &pmix_addr_len);
    if (ec != 0) {
        log_error("getsockname failed with e=%d", ec);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }

    { // PMIx BEGIN 
        pmix_status_t pmix_status = PMIX_SUCCESS;
        size_t ninfo = 3;
        pmix_persistence_t persistence = PMIX_PERSIST_PROC;
        bool overwrite = true;
        pmix_info_t info[ninfo];
        pmix_byte_object_t bo;
        if (!PMIx_Initialized()) pmix_status = PMIx_Init(&pHndl->pmix_proc, NULL, 0);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed to intialize with error %s", PMIx_Error_string(pmix_status));


        log_info("Server proc = %s:%d", pHndl->pmix_proc.nspace, pHndl->pmix_proc.rank);

        bo.bytes = (char *)&pmix_addr;
        bo.size = sizeof(struct sockaddr_in);

        pmix_status = PMIx_Info_load(&info[0], "server_sock_addr", &bo, PMIX_BYTE_OBJECT);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed with %s", PMIx_Error_string(pmix_status));

        pmix_status = PMIx_Info_load(&info[1], PMIX_PERSISTENCE, &persistence, PMIX_PERSIST);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed with %s", PMIx_Error_string(pmix_status));

        pmix_status = PMIx_Info_load(&info[2], "prte.pub.replace", &overwrite, PMIX_BOOL);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed with %s", PMIx_Error_string(pmix_status));

        pmix_status = PMIx_Publish(info, ninfo);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx Publish failed with %s", PMIx_Error_string(pmix_status));

    }
     
    return pHndl;
}

extern int ads_finalize(ads_hndl **ppHndl) {
    if (!ppHndl)
        return -1;
    if (!*ppHndl)
        return -1;

    { // PMIx BEGIN
        pmix_status_t pmix_status = PMIX_SUCCESS;
        char *keys[] = {"server_sock_addr"};
        pmix_status = PMIx_Unpublish(keys, NULL, 0);
        if (pmix_status != PMIX_SUCCESS) log_error("Failed to unpublish addr %s", PMIx_Error_string(pmix_status));
        pmix_status = PMIx_Finalize(NULL, 0);
        if (pmix_status != PMIX_SUCCESS) log_error("Failed to finalize the PMIx Client");
    }

     
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
    

    return 0;
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
        return NULL;
    }

    size_t ndata = 1;
    size_t ninfo = 1;

    pmix_info_t info[ninfo];
    pmix_pdata_t pdata = PMIX_LOOKUP_STATIC_INIT;
    pmix_data_range_t range = PMIX_RANGE_SESSION;
    pmix_status_t pmix_status = PMIX_SUCCESS;
    pmix_proc_t client_proc = PMIX_PROC_STATIC_INIT;

    if (!PMIx_Initialized()) pmix_status = PMIx_Init(&client_proc, NULL, 0);
    log_info("Client proc = %s:%d", client_proc.nspace, client_proc.rank);
    
    if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed to initialize with error %s", PMIx_Error_string(pmix_status));

    PMIx_Load_key(pdata.key, "server_sock_addr");
    PMIx_Info_load(&info[0], PMIX_RANGE, &range, PMIX_DATA_RANGE);
    pmix_status = PMIx_Lookup(&pdata, ndata, info, ninfo);

    if (pmix_status != PMIX_SUCCESS) log_error("PMIx Lookup failed with %s.", PMIx_Error_string(pmix_status));

    log_info("Got proc %d/%s with key %s and val %s", pdata.proc.rank, pdata.proc.nspace, pdata.key, PMIx_Value_string(&pdata.value));

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
    memcpy(&addr, pdata.value.data.bo.bytes, sizeof(struct sockaddr_in));
    if (connect(sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        log_warn("Failed to connect to server socket");
        return NULL;
    }
    
    return ads_exchange(sd, pTxData);
}
