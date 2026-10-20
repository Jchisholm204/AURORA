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
#include "afv_test_fillmem.h"
#include "afv_test_parser.h"
#include "log.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

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
    afv_checkpoint_t *pCheckpoint = afv_checkpoint_open(pAFV, 1, "afv_test");

    if (!pCheckpoint) {
        log_error("Failed to create a new checkpoint");
        free(tp);
        afv_destroy_instance(&pAFV);
        return 1;
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

    if(tp->n_regions != afv_checkpoint_n_regions(pCheckpoint)){
        log_error("Region Count Mismatch");
        free(tp);
        afv_checkpoint_destroy(&pCheckpoint);
        afv_destroy_instance(&pAFV);
        return 1;
    }

    // Loop through regions to restore
    for (size_t i = 0; i < tp->n_regions; i++) {

        size_t region_index = 0;
        eAFV_verif restore_status =
            afv_checkpoint_find_index(pCheckpoint, RGN_ID(i), block_size_b,
                                      NULL, &region_index);

        if (restore_status != eAFV_VERIF_OK) {
            log_warn("Failed to lookup region id=%d (%d) verification=0x%x",
                     RGN_ID(i), i, restore_status);
            continue;
        } else {
            log_info("Found Region %d at index %d (%d)", RGN_ID(i),
                     region_index, i);
        }

        uint64_t region_id = 0;

        test_afv_cleanmem(pBlock, block_size_b);

        for (size_t j = 0; j < (block_size_b / tp->write_block_size_b); j++) {
            size_t read_start = tp->write_block_size_b * j;
            size_t read_end = read_start + tp->write_block_size_b;
            log_debug("Reading... %d id=%d start=%d end=%d", i, RGN_ID(i),
                      read_start, read_end);
            ssize_t bytes_read =
                afv_checkpoint_read(pCheckpoint, RGN_ID(i), read_start,
                                    ((uint8_t *) pBlock) + (read_start),
                                    tp->write_block_size_b);
            assert((size_t) (bytes_read) == (read_end - read_start));
        }

        test_afv_testmem(pBlock, block_size_b, RGN_ID(i));

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
