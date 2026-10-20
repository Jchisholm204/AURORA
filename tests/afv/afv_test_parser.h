/**
 * @file afv_test_parser.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-10-20
 * @modified Last Modified: 2026-10-20
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AFV_TEST_PARSER_H_
#define _AFV_TEST_PARSER_H_
#include "log.h"

#include <stdlib.h>

struct test_params {
    char *test_dir;
    size_t n_regions;
    size_t region_sizes_mb;
    size_t write_block_size_b;
};

struct test_params *parse_params(int argc, char **argv) {
    if (argc < 2) {
        log_error("First argument must be test directory");
        return NULL;
    }
    char *filepath = argv[1];

    if (argc < 3) {
        log_warn("Second argument must be number of regions to write");
        return NULL;
    }
    size_t test_n_regions = atoi(argv[2]);
    if (test_n_regions == 0) {
        log_error("Must use at least one test region");
        return NULL;
    }
    log_trace("Using %d test regions", test_n_regions);

    if (argc < 4) {
        log_error("Third argument must be size of regions to write in mb");
        return NULL;
    }
    size_t test_region_size = atoi(argv[3]);
    if (test_region_size == 0) {
        log_error("Test region size must be greater than or equal to 1 (mb)");
        return NULL;
    }
    log_trace("Using test regions of size %d mb", test_region_size);

    // Default is 512k
    size_t block_write_size_b = 512 * 1024;
    if (argc < 5) {
        log_warn("Fourth argument is the transfer block size in bytes");
    } else {
        block_write_size_b = atoi(argv[4]);
    }
    if (block_write_size_b == 0) {
        log_error("Block write size cannot be 0");
    }
    log_trace("Using block write size %d bytes", block_write_size_b);

    struct test_params *tp = malloc(sizeof(struct test_params));
    if (!tp) {
        log_error("Could not allocate memory for test params");
        return NULL;
    }

    tp->test_dir = filepath;
    tp->n_regions = test_n_regions;
    tp->region_sizes_mb = test_region_size;
    tp->write_block_size_b = block_write_size_b;
    return tp;
}

#endif
