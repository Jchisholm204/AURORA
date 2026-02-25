/**
 * @file main.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-02-24
 * @modified Last Modified: 2026-02-24
 *
 * @copyright Copyright (c) 2026
 */

#include "ads/ads.h"
#include "log.h"

#include <stdio.h>
#include <malloc.h>

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    ads_exchange_data_t ads_data = {
        .ck_size = 12,
        .ud_size = 12,
        .comm_key = "HelloWorld",
        .user_data = "Yeetskeet",
    };
    if (argc == 1) {
        log_info("Server");
        ads_hndl *ads = ads_init();
        if (!ads) {
            log_error("Fained to init ADS");
            return 0;
        }
        while (1) {
            int sd = ads_accept_any(ads);
            if (sd >= 0) {
                ads_exchange_data_t *rx = ads_exchange(sd, &ads_data);
                if (rx) {
                    printf("%s\n", (char*)rx->comm_key);
                    printf("%s\n", (char*)rx->user_data);
                    free(rx);
                }
            }
        }
    } else {
        log_info("Client");
        ads_conf_t conf = {
            .opt_server_ip = "127.0.0.1",
            .timeout_ms = 1000,
        };
        ads_exchange_data_t *rx = ads_request_exchange(&conf, &ads_data);
        if (rx) {
            printf("%s\n", (char*)rx->comm_key);
            printf("%s\n", (char*)rx->user_data);
            free(rx);
        }
    }
    // printf("Server Hello World");
    return 0;
}
