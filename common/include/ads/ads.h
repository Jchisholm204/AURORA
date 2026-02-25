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
typedef struct aurora_discovery_service_exchange_data ads_exchange_data_t;
typedef struct aurora_discovery_service_conf ads_conf_t;

struct aurora_discovery_service_exchange_data {
    size_t ck_size;
    size_t ud_size;
    void *comm_key;
    void *user_data;
};

struct aurora_discovery_service_conf {
    char *opt_server_ip;
    int timeout_ms;
};

extern ads_hndl *ads_init(void);

extern int ads_finalize(ads_hndl **ppHndl);

extern int ads_accept_any(ads_hndl *pHndl);

extern ads_exchange_data_t *ads_exchange(int sock_fd,
                                         ads_exchange_data_t *pTxData);

extern ads_exchange_data_t *ads_request_exchange(
    const ads_conf_t *pConf, ads_exchange_data_t *pTxData);

#endif
