/**
 * @file ads.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Discovery Service
 * @version 0.1
 * @date Created: 2026-02-25
 * @modified Last Modified: 2026-02-25
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ADS_ADS_H_
#define _ADS_ADS_H_

#include <stddef.h>

struct aurora_discovery_service_hndl;
typedef struct aurora_discovery_service_hndl ads_hndl;

struct ads_exchange_data {
    size_t ck_size;
    size_t ud_size;
    void *comm_key;
    void *user_data;
};

extern ads_hndl *ads_init(void);

extern int ads_finalize(ads_hndl **ppHndl);

extern int ads_accept_any(ads_hndl *pHndl);

extern struct ads_exchange_data *ads_accept_exchange(
    ads_hndl *pHndl, int sock_fd, struct ads_exchange_data *pTxData);

extern struct ads_exchange_data *ads_request_exchange(
    const char *server_ip, int timeout, struct ads_exchange_data *pTxData);

#endif
