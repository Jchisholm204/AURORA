/**
 * @file test_afv_checkpoint.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-09-22
 * @modified Last Modified: 2026-09-22
 *
 * @copyright Copyright (c) 2026
 */

#include "afv/afv.h"
#include "afv/afv_checkpoint.h"
#include "afv/afv_metadata.h"
#include "log.h"

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

    size_t block_write_size_b = 512;
    if (argc < 5) {
        log_error("Fourth argument is the transfer block size in bytes");
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

    // Generate checkpoint metadata
    afv_metadata_t *pMetadata = afv_create_metadata(tp->n_regions);
    if (!pMetadata) {
        log_error("BAD ALLOC");
        free(tp);
        return 1;
    }

    pMetadata->version = 1;
    snprintf(pMetadata->chkpt_name, AFV_CKPT_NAME_LEN, "%s", "cptest");
    for (int i = 0; i < pMetadata->n_regions; i++) {
        snprintf(pMetadata->region_names[i], AFV_RGN_NAME_LEN, "tst%d", i);
        pMetadata->region_sizes[i] = 0;
        // Arbitrary ID Generation
        pMetadata->region_ids[i] = RGN_ID(i);
        pMetadata->region_sizes[i] = tp->region_sizes_mb * 1024 * 1024;
        log_info("\t %s id=%d size=%ld", pMetadata->region_names[i],
                 pMetadata->region_ids[i], pMetadata->region_sizes[i]);
    }

    // Open Checkpoint for writing
    afv_checkpoint_t *pCheckpoint = afv_checkpoint_create(pAFV, pMetadata);

    if (!pCheckpoint) {
        log_error("Failed to create a new checkpoint");
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    // Expect a failure when writing an empty checkpoint
    eAFV_verif ckpt_verification = afv_checkpoint_close(&pCheckpoint);
    if (ckpt_verification == eAFV_VERIF_OK) {
        log_error("TEST: Empty checkpoint was written successfully");
    }
    if (ckpt_verification & eAFV_VERIF_ERR_FATAL) {
        log_error("Fatal Error writing checkpoint 0x%x & 0x%x",
                  ckpt_verification, eAFV_VERIF_ERR_FATAL);
    } else {
        log_info("Successfully failed verification 0x%x == 0x%x",
                 ckpt_verification,
                 eAFV_VERIF_ERR_RGN_SIZE | eAFV_VERIF_ERR_RGN_IDS);
    }

    uint64_t *pBlock = malloc(1024 * 1024 * tp->region_sizes_mb);
    if (!pBlock) {
        log_error("Failed to allocate checkpoint memory block");
        free(tp);
        afv_checkpoint_destroy(&pCheckpoint);
        afv_destroy_instance(&pAFV);
        return 1;
    }
    for (size_t i = 0; i < 1024 * 1024 * tp->region_sizes_mb / sizeof(size_t);
         i++) {
        pBlock[i] = i;
    }

    // Write out the checkpoint data
    for (size_t i = 1; i <= tp->n_regions; i++) {
        for (size_t j = 0;
             j < 1024 * 1024 * tp->region_sizes_mb / tp->write_block_size_b;
             j++) {
        }
        afv_checkpoint_write(pCheckpoint, 0, 0, NULL, 0);
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

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
