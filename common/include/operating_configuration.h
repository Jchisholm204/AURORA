/**
 * @file operating_configuration.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Defines the Operation Mode Configuration for AURORA
 * @version 0.1
 * @date Created: 2026-03-31
 * @modified Last Modified: 2026-03-31
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _OPERATING_CONFIGURATION_H_
#define _OPERATING_CONFIGURATION_H_
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct aurora_operating_configuration {
    struct {
        int64_t group;
        uint64_t rank;
    } id;

    struct {
        char *persistent_path;
        bool use_error_correction;
    } chkpt_opts;
};

static inline size_t aoc_size(
    const struct aurora_operating_configuration *pConfig) {
    if (!pConfig) {
        return 0;
    }
    size_t size = 0;
    size += sizeof(struct aurora_operating_configuration);
    if (pConfig->chkpt_opts.persistent_path) {
        size += strlen(pConfig->chkpt_opts.persistent_path);
    }
    return size;
}

static inline void *aoc_pack(
    const struct aurora_operating_configuration *pConfig) {
    if (!pConfig) {
        return NULL;
    }
    uint8_t *buffer = malloc(aoc_size(pConfig));
    if (!buffer) {
        return NULL;
    }
    (void) memcpy(buffer, pConfig,
                  sizeof(struct aurora_operating_configuration));
    if (pConfig->chkpt_opts.persistent_path) {
        strcpy((char *) buffer + sizeof(struct aurora_operating_configuration),
               pConfig->chkpt_opts.persistent_path);
    }
    return buffer;
}

static inline struct aurora_operating_configuration *aoc_unpack(
    const void *buffer, size_t size) {
    if (!buffer || size == 0) {
        return NULL;
    }
    struct aurora_operating_configuration *pConfig =
        malloc(sizeof(struct aurora_operating_configuration));

    if (!pConfig) {
        return NULL;
    }

    (void) memcpy(pConfig, buffer,
                  sizeof(struct aurora_operating_configuration));

    pConfig->chkpt_opts.persistent_path = NULL;

    if (size > sizeof(struct aurora_operating_configuration)) {
        size_t str_len = size - sizeof(struct aurora_operating_configuration);
        pConfig->chkpt_opts.persistent_path = malloc(str_len);
        if (!pConfig->chkpt_opts.persistent_path) {
            free(pConfig);
            return NULL;
        }
        (void)memcpy(pConfig->chkpt_opts.persistent_path,
               (char *) buffer + sizeof(struct aurora_operating_configuration),
               str_len);
    }

    return pConfig;
}

#endif
