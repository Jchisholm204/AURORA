/**
 * @file test_afv_restore.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-10-14
 * @modified Last Modified: 2026-10-14
 *
 * @copyright Copyright (c) 2026
 */

#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "afv/afv_metadata.h"
#include "log.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

// Arbitrary ID Generation
#define RGN_ID(i) ((i + 1) * 2)

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

int main(int argc, char **argv) {

    log_info("Starting AFV Tests");

    struct test_params *tp = parse_params(argc, argv);
    if (!tp) {
        log_error("Failed to initialize test params");
        return 1;
    }

    afv_hndl *pAFV = NULL;
    {
        int rank = 0;
        int group_size = 1;
        int group_id = 0;
        pAFV = afv_create_instance(rank, group_id, group_size, tp->test_dir,
                                   false);
    }
    if (!pAFV) {
        log_error("Failed to initialize the AFV");
        free(tp);
        return 1;
    }

    // Open Checkpoint for writing
    afv_checkpoint_t *pCheckpoint = afv_checkpoint_open(pAFV, 1, "cptest");

    if (!pCheckpoint) {
        log_error("Failed to create a new checkpoint");
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    const afv_metadata_t *pMetadata = afv_checkpoint_metadata(pCheckpoint);
    eAFV_verif checkpoint_verification =
        afv_metadata_check(pMetadata, 1, "cptest", tp->n_regions);
    if (checkpoint_verification != eAFV_VERIF_OK) {
        log_error("Checkpoint Verification Failure 0x%x",
                  checkpoint_verification);
        free(tp);
        afv_checkpoint_destroy(&pCheckpoint);
        afv_destroy_instance(&pAFV);
        return 1;
    } else {
        log_info("Metadata Verification Succeeded");
    }

    size_t block_size_b = tp->region_sizes_mb * 1024 * 1024;
    uint64_t *pBlock = malloc(block_size_b);
    if (!pBlock) {
        log_error("Failed to allocate checkpoint memory block");
        free(tp);
        afv_checkpoint_destroy(&pCheckpoint);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    // Write out the checkpoint data
    for (size_t i = 0; i < tp->n_regions; i++) {
        size_t region_index;
        eAFV_verif region_status =
            afv_metadata_get_region(pMetadata, NULL, RGN_ID(i), block_size_b,
                                    &region_index);
        if (region_status != eAFV_VERIF_OK) {
            log_error("Failed to find region %d", RGN_ID(i));
        }
        else{
            log_info("Found Region %d at index %d (%d)", RGN_ID(i), region_index, i);
        }
        for (size_t j = 0; j < block_size_b / sizeof(size_t); j++) {
            pBlock[j] = 0;
        }
        for (size_t j = 0; j < (block_size_b / tp->write_block_size_b); j++) {
            size_t read_start = tp->write_block_size_b * j;
            size_t read_end = read_start + tp->write_block_size_b;
            log_debug("Reading... %d id=%d start=%d end=%d", i, RGN_ID(i),
                      read_start, read_end);
            afv_checkpoint_read(pCheckpoint, RGN_ID(i), read_start,
                                ((uint8_t *) pBlock) + (read_start),
                                tp->write_block_size_b);
        }
        for (size_t j = 0; j < block_size_b / sizeof(size_t); j++) {
            assert(pBlock[j] == j);
        }
        log_info("Region %d id=%d OK", i, RGN_ID(i));
    }

    int checkpoint_close_status = afv_checkpoint_close(&pCheckpoint);
    if (checkpoint_close_status != eAFV_VERIF_OK) {
        log_error("Failed to close the checkpoint with error %d",
                  checkpoint_close_status);
        free(tp);
        afv_checkpoint_close(&pCheckpoint);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    log_info("Checkpoint Verification Succeeded");

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
