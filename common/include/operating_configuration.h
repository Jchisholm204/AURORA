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
    uint64_t rank;
    struct {
        uint64_t id;
        int64_t size;
    } group;

    struct {
        char *persistent_path;
        bool use_error_correction;
    } chkpt_opts;
};

typedef struct aurora_operating_configuration opconf_t;

static inline opconf_t *aoc_alloc(char *persistent_path) {
    if (!persistent_path) {
        return NULL;
    }

    size_t strsize = strlen(persistent_path) + 1;
    size_t structsize = sizeof(opconf_t);
    uint8_t *structbuf = malloc(strsize + structsize);

    if (!structbuf) {
        // ERROR: Bad Alloc??
        return NULL;
    }

    opconf_t *pConfig = (void *) structbuf;
    pConfig->chkpt_opts.persistent_path = (void *) (structbuf + structsize);

    pConfig->group.id = 0;
    pConfig->group.size = -1;
    pConfig->rank = 0;

    strcpy(pConfig->chkpt_opts.persistent_path, persistent_path);

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

static inline size_t aoc_size(
    const struct aurora_operating_configuration *pConfig) {
    if (!pConfig) {
        return 0;
    }
    size_t size = 0;
    size += sizeof(struct aurora_operating_configuration);
    if (pConfig->chkpt_opts.persistent_path) {
        size += strlen(pConfig->chkpt_opts.persistent_path) + 1;
    }
    return size;
}

#endif
