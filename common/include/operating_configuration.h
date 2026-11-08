/**
 * @file operating_configuration.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Defines the Operation Mode Configuration for AURORA
 * @version 0.2
 * @date Created: 2026-03-31
 * @modified Last Modified: 2026-11-08
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _OPERATING_CONFIGURATION_H_
#define _OPERATING_CONFIGURATION_H_
#include "log.h"

#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// String size of an IPv4 address (including null terminator)
#define AOC_IP_LEN (sizeof("xxx.xxx.xxx.xxx"))
#ifndef AOC_MAX_CLIENTS
#define AOC_MAX_CLIENTS 256
#endif

struct aurora_operating_configuration {
    uint64_t rank;
    struct {
        uint64_t id;
        int64_t size;
    } group;

    struct {
        char *persistent_path;
        bool use_error_correction;
    } chkpt_opts;

    struct {
        size_t n_clients;
        struct aurora_operating_configuration_external_logger_client {
            char ip[AOC_IP_LEN];
            uint16_t port;
        } *cli_list;
    } log_opts;
};

typedef struct aurora_operating_configuration opconf_t;

/**
 * @brief Initialize AOC pointer offsets: sizes must be initialized first
 *
 * @param ptr Raw block pointer
 * @return Operating Configuration with correctly configured pointers.
 */
static inline opconf_t *aoc_ptr_init(void *ptr) {
    opconf_t *pConfig = (opconf_t *) ptr;
    if (!pConfig) {
        return NULL;
    }

    size_t n_clients = pConfig->log_opts.n_clients;
    const size_t elclient_offset = sizeof(opconf_t);
    const size_t elclient_size =
        sizeof(struct aurora_operating_configuration_external_logger_client) *
        n_clients;

    if (n_clients > AOC_MAX_CLIENTS) {
        // Overflow Error
        log_warn("OOB");
        n_clients = AOC_MAX_CLIENTS;
        pConfig->log_opts.n_clients = AOC_MAX_CLIENTS;
    }

    const size_t str_offset = elclient_offset + elclient_size;

    if (pConfig->log_opts.n_clients == 0) {
        pConfig->log_opts.cli_list = NULL;
    } else {
        pConfig->log_opts.cli_list = (void *) ((char *) ptr + elclient_offset);
    }

    pConfig->chkpt_opts.persistent_path = ((char *) pConfig + str_offset);

    return pConfig;
}

static inline opconf_t *aoc_alloc(char *persistent_path, size_t n_el_clients) {
    if (!persistent_path) {
        return NULL;
    }

    if (n_el_clients > AOC_MAX_CLIENTS) {
        log_warn("Requested %ld exceeds maximum %d", n_el_clients,
                 AOC_MAX_CLIENTS);
        n_el_clients = AOC_MAX_CLIENTS;
    }

    const size_t str_size = strlen(persistent_path) + 1;
    const size_t elclient_size =
        n_el_clients *
        sizeof(struct aurora_operating_configuration_external_logger_client);

    const uint8_t *structbuf =
        malloc(sizeof(opconf_t) + elclient_size + str_size);

    if (!structbuf) {
        // ERROR: Bad Alloc??
        return NULL;
    }

    opconf_t *pConfig = (void *) structbuf;

    pConfig->group.id = 0;
    pConfig->group.size = -1;
    pConfig->rank = 0;
    pConfig->log_opts.n_clients = n_el_clients;
    pConfig->chkpt_opts.use_error_correction = false;

    // Initialize internal pointers (el_n_clients must be set prior)
    pConfig = aoc_ptr_init((void *) structbuf);
    if (!pConfig) {
        // Should never happen
        log_fatal("Bad Alloc??");
        free((void *) structbuf);
        structbuf = NULL;
        return NULL;
    }

    strcpy(pConfig->chkpt_opts.persistent_path, persistent_path);

    if (pConfig->log_opts.cli_list) {
        (void) memset(pConfig->log_opts.cli_list, 0, elclient_size);
    }

    return pConfig;
}

static inline void aoc_free(struct aurora_operating_configuration **ppConfig) {
    if (ppConfig) {
        if (*ppConfig) {
            (*ppConfig)->chkpt_opts.persistent_path = NULL;
            free(*ppConfig);
            *ppConfig = NULL;
        }
    }
}

/**
 * @brief Gets the block pointer size of an AOC object
 *
 * @param pConfig AOC object to get the block size of
 * @return
 */
static inline size_t aoc_size(
    const struct aurora_operating_configuration *pConfig) {
    if (!pConfig) {
        log_error("NULL Parameter");
        return 0;
    }

    size_t n_clients = pConfig->log_opts.n_clients;
    if (n_clients > AOC_MAX_CLIENTS) {
        log_error("Overflow");
        n_clients = AOC_MAX_CLIENTS;
    }

    size_t size = 0;
    size += sizeof(struct aurora_operating_configuration);
    size +=
        n_clients *
        sizeof(struct aurora_operating_configuration_external_logger_client);
    if (pConfig->chkpt_opts.persistent_path) {
        size += strlen(pConfig->chkpt_opts.persistent_path) + 1;
    }
    return size;
}

#endif
