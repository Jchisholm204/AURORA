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
    int mdns_sock;
    int tcp_sock;
    pmix_proc_t pmix_proc;
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
        log_error("Failed to allocated ADS Handle");
        return NULL;
    }

    // pHndl->mdns_sock = mdns_socket_open_ipv4(NULL);
    // mdns_socket_setup_ipv4(pHndl->mdns_sock, NULL);

    // int loop = 1;
    /*
    if (setsockopt(pHndl->mdns_sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
                   sizeof(loop)) < 0) {
        log_error("Failed to set IP_MULTICAST_LOOP");
    }
    */

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
        // mdns_socket_close(pHndl->mdns_sock);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }
    ec = listen(pHndl->tcp_sock, ADS_LISTEN_N);
    if (ec != 0) {
        log_error("listener failed eith e=%d", ec);
        // mdns_socket_close(pHndl->mdns_sock);
        close(pHndl->tcp_sock);
        free(pHndl);
        return NULL;
    }

    { // PMIx BEGIN 
        pmix_status_t pmix_status = PMIX_SUCCESS;
        if (!PMIx_Initialized()) pmix_status = PMIx_Init(&pHndl->pmix_proc, NULL, 0);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed to intialize with error %s", PMIx_Error_string(pmix_status));


        log_info("Server proc = %s:%d", pHndl->pmix_proc.nspace, pHndl->pmix_proc.rank);
        size_t ninfo = 3;
        pmix_info_t info[ninfo];
        pmix_persistence_t persistence = PMIX_PERSIST_PROC;
        bool overwrite = true;

        struct sockaddr_in pmix_addr;
        socklen_t pmix_addr_len = sizeof(pmix_addr);
        getsockname(pHndl->tcp_sock, (struct sockaddr *)&pmix_addr, &pmix_addr_len);
        pmix_byte_object_t bo;
        bo.bytes = (char *)&pmix_addr;
        bo.size = sizeof(struct sockaddr_in);
        pmix_status = PMIx_Info_load(&info[0], "server_sock_addr", &bo, PMIX_BYTE_OBJECT);
        pmix_status = PMIx_Info_load(&info[1], PMIX_PERSISTENCE, &persistence, PMIX_PERSIST);
        pmix_status = PMIx_Info_load(&info[2], "prte.pub.replace", &overwrite, PMIX_BOOL);
        pmix_status = PMIx_Publish(info, ninfo);

        if (pmix_status != PMIX_SUCCESS) log_error("PMIx Publish failed with %s", PMIx_Error_string(pmix_status));

        // pmix_proc_t wildcard_proc = PMIX_PROC_STATIC_INIT;
        // PMIx_Load_procid(&wildcard_proc, pHndl->pmix_proc.nspace, PMIX_RANK_WILDCARD);
        // pmix_status = PMIx_Get(&wildcard_proc, "comm", NULL, 0, NULL);
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

     
    // mdns_socket_close((*ppHndl)->mdns_sock);
    close((*ppHndl)->tcp_sock);
    

    free((*ppHndl));
    *ppHndl = NULL;
    return 0;
}

// This does not need to get called
extern int ads_accept_any(ads_hndl *pHndl) {
    if (!pHndl) {
        return -1;
    }

    
    fd_set readfds;
    int max_fd = (pHndl->mdns_sock > pHndl->tcp_sock) ? pHndl->mdns_sock
                                                      : pHndl->tcp_sock;
    max_fd = pHndl->tcp_sock;
    while (1) {
        FD_ZERO(&readfds);
        // FD_SET(pHndl->mdns_sock, &readfds);
        FD_SET(pHndl->tcp_sock, &readfds);
        select(max_fd + 1, &readfds, NULL, NULL, NULL);
        /*
        if (FD_ISSET(pHndl->mdns_sock, &readfds)) {
            mdns_socket_listen(pHndl->mdns_sock, pHndl->mdns_buf, MDNS_BUF_SIZE,
                               _ads_mdns_callback, pHndl);
        }
        */
        if (FD_ISSET(pHndl->tcp_sock, &readfds)) {
            return accept(pHndl->tcp_sock, NULL, NULL);
        }
    }
    

    return 0;
}

// Temporary, is there a better solution?
int prepend_index_key(const char *src, char *dest, size_t dest_len, int id) {
    if (!src || !dest || dest_len == 0)
        return -1;

    int written = snprintf(dest, dest_len, "%d-%s", id, src);

    // Check for truncation
    if (written < 0 || (size_t)written >= dest_len) {
        return -1;
    }

    return 0;
}

extern ads_exchange_data_t *ads_exchange(int sock_fd,
                                         ads_exchange_data_t *pTxData) {
    if (!pTxData) {
        return NULL;
    }

    // ads_exchange_data_t *pRx = malloc(sizeof(ads_exchange_data_t));
    
    { // PMIx BEGIN
      /*
        pmix_rank_t pmix_rank = (pmix_rank_t) sock_fd;
        size_t ads_exchange_size = sizeof(ads_exchange_data_t);
        pmix_status_t pmix_status = PMIX_SUCCESS;

                 
        size_t ninfo = 5;
        pmix_info_t info[ninfo];
        PMIx_Info_load(&info[0], "header", &ads_exchange_size, PMIX_SIZE);
        pmix_byte_object_t bos[3];
        pmix_value_t put_vals[3];
        // this takes a char *
        
        // PMIx_Byte_object_load neq PMIx 4.2
        // PMIx_Byte_object_load(&bos[0], pTxData->comm.data, pTxData->comm.size);
        bos[0].bytes = pTxData->comm.data;
        bos[0].size = pTxData->comm.size;
        // PMIx_Byte_object_load(&bos[1], pTxData->notif.data, pTxData->notif.size);
        bos[1].bytes = pTxData->notif.data;
        bos[1].size = pTxData->notif.size;
        // PMIx_Byte_object_load(&bos[2], pTxData->config.data, pTxData->config.size);
        bos[2].bytes = pTxData->config.data;
        bos[2].size = pTxData->config.size;

        // 7 is just the max length among strs
        char comm_key[7];
        char notif_key[7];
        char config_key[7];

        // change to operate on who owns it right now
        prepend_index_key("comm", comm_key, 7, pmix_rank);
        prepend_index_key("notif", notif_key, 7, pmix_rank);
        prepend_index_key("config", config_key, 7, pmix_rank);
        PMIx_Info_load(&info[1], comm_key, &bos[0], PMIX_BYTE_OBJECT);
        PMIx_Info_load(&info[2], notif_key, &bos[1], PMIX_BYTE_OBJECT);
        PMIx_Info_load(&info[3], config_key, &bos[2], PMIX_BYTE_OBJECT);
        PMIx_Value_load(&put_vals[0], &bos[0], PMIX_BYTE_OBJECT);
        PMIx_Value_load(&put_vals[1], &bos[1], PMIX_BYTE_OBJECT);
        PMIx_Value_load(&put_vals[2], &bos[2], PMIX_BYTE_OBJECT);

        pmix_persistence_t persistence = PMIX_PERSIST_FIRST_READ;
        PMIx_Info_load(&info[4], PMIX_PERSISTENCE, &persistence, PMIX_PERSIST);

        pmix_pdata_t mpdata = PMIX_LOOKUP_STATIC_INIT;
        PMIx_Load_key(mpdata.key, "remote_engine");
        PMIx_Lookup(&mpdata, 1, NULL, 0);

        if (mpdata.proc.rank == pmix_rank) {
            pmix_pdata_t ppdata = PMIX_LOOKUP_STATIC_INIT;
            PMIx_Load_key(ppdata.key, comm_key);
            pmix_status = PMIx_Lookup(&ppdata, 1, NULL, 0);
            // also should check partial_success
            if (pmix_status == PMIX_ERR_NOT_FOUND) {
                pmix_status = PMIx_Publish(info, ninfo);
            }

            size_t nqueries = 1;
            pmix_query_t *queries = PMIx_Query_create(nqueries);
            char *keys[] = {PMIX_QUERY_PROC_TABLE, NULL};
            queries->keys = keys;

            size_t nresults = 0;
            pmix_info_t *results = NULL;
            PMIx_Query_info(queries, nqueries, &results, &nresults);

            for (size_t i = 0; i < nresults; i++) log_info("%s", PMIx_Info_string(&results[i]));
        }

        if (pmix_status != PMIX_SUCCESS) log_error("PMIx Lookup failed %s.", PMIx_Error_string(pmix_status));
        size_t npdata = 3;
        ninfo = 1;
        pmix_pdata_t pdata[npdata];
        pmix_pdata_t dummy = PMIX_LOOKUP_STATIC_INIT;
        for (size_t i = 0; i < npdata; i++) {
            pdata[i] = dummy; // for some reason complains here about STATIC_INIT
        }

        prepend_index_key("comm", comm_key, 7, sock_fd);
        prepend_index_key("notif", notif_key, 7, sock_fd);
        prepend_index_key("config", config_key, 7, sock_fd);
        PMIx_Load_key(pdata[0].key, comm_key);
        PMIx_Load_key(pdata[1].key, notif_key);
        PMIx_Load_key(pdata[2].key, config_key);

        pmix_info_t linfo[ninfo];
        pmix_data_range_t range = PMIX_RANGE_SESSION;
        PMIx_Info_load(&linfo[0], PMIX_RANGE, &range, PMIX_DATA_RANGE);
        pmix_status = PMIx_Lookup(pdata, npdata, linfo, ninfo);
        */
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

        pmix_status_t pmix_status = PMIX_SUCCESS;
        pmix_proc_t client_proc;
        if (!PMIx_Initialized()) pmix_status = PMIx_Init(&client_proc, NULL, 0);
        log_info("Client proc = %s:%d", client_proc.nspace, client_proc.rank);
        if (pmix_status != PMIX_SUCCESS) log_error("PMIx failed to initialize with error %s", PMIx_Error_string(pmix_status));

        pmix_pdata_t pdata = PMIX_LOOKUP_STATIC_INIT;
        PMIx_Load_key(pdata.key, "server_sock_addr");
        size_t ndata = 1;
        size_t ninfo = 1;
        pmix_info_t info[ninfo];
        pmix_data_range_t range = PMIX_RANGE_SESSION;
        PMIx_Info_load(&info[0], PMIX_RANGE, &range, PMIX_DATA_RANGE);
        pmix_status = PMIx_Lookup(&pdata, ndata, info, ninfo);

        if (pmix_status != PMIX_SUCCESS) log_error("PMIx Lookup failed with %s.", PMIx_Error_string(pmix_status));

        log_info("Got proc %d/%s with key %s and val %s", pdata.proc.rank, pdata.proc.nspace, pdata.key, PMIx_Value_string(&pdata.value));

        // return ads_exchange(pdata.proc.rank, pTxData);

    log_trace("Connecting to %s", pConf->opt_server_ip);

    char target_ip[64];
    ads_conf_t conf;
    conf = *pConf;
    /*
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
            log_warn("Failed find the server e=%d", found);
            return NULL;
        }
    }
    */

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
    // inet_pton(AF_INET, pConf->opt_server_ip, &addr.sin_addr);
    if (connect(sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        log_warn("Failed to connect to server socket");
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
