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
    size_t region_sizes;
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
    log_trace("Using %d test regions", test_n_regions);

    if (argc < 4) {
        log_error("Third argument must be size of regions to write");
        return NULL;
    }
    size_t test_region_size = atoi(argv[3]);
    log_trace("Using test regions of size %d", test_region_size);
    struct test_params *tp = malloc(sizeof(struct test_params));
    if (!tp) {
        log_error("Could not allocate memory for test params");
        return NULL;
    }

    tp->test_dir = filepath;
    tp->n_regions = test_n_regions;
    tp->region_sizes = test_region_size;
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

    afv_metadata_t *pMetadata = afv_create_metadata(tp->n_regions);

    if (!pMetadata) {
        log_error("Failed to create test metadata");
        log_trace("Destroying AFV Instanace");
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    pMetadata->rank = afv_get_rank(pAFV);
    pMetadata->n_regions = tp->n_regions;
    pMetadata->version = 1;

    // Open Checkpoint for writing
    afv_checkpoint_t *pCheckpoint =
        afv_checkpoint_open(pAFV, 1, "test", tp->n_regions, eAFVC_MODE_W);

    if (!pCheckpoint) {
        log_error("Failed to create a new checkpoint");
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    int checkpoint_close_status = afv_checkpoint_close(&pCheckpoint);
    if (checkpoint_close_status != 0){
        log_error("Failed to close the checkpoint with error %d", checkpoint_close_status);
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
