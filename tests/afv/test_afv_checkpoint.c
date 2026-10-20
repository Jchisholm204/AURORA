/**
 * @file test_afv_checkpoint.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.2
 * @date Created: 2026-09-22
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
    const char checkpoint_name[AFV_CKPT_NAME_LEN] = "afv_test";
    afv_checkpoint_t *pCheckpoint =
        afv_checkpoint_create(pAFV, 1, checkpoint_name, tp->n_regions);

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
    // for (size_t i = 0; i < block_size_b / sizeof(size_t); i++) {
    //     pBlock[i] = i;
    // }

    // Write out the checkpoint data
    for (size_t i = 0; i < tp->n_regions; i++) {
        // Fill block with pesudo random hash before writing
        test_afv_fillmem(pBlock, block_size_b, RGN_ID(i));

        // Add the region to the checkpoint before writing
        eAFV_verif region_add_status =
            afv_checkpoint_add(pCheckpoint, RGN_ID(i), block_size_b, NULL);
        // Verify region add status
        if (region_add_status != eAFV_VERIF_OK) {
            log_warn("Failed to add region %d id=%d", i, RGN_ID(i));
        }

        // Perform Chunked copies (assumes size is multiple of block write)
        for (size_t j = 0; j < (block_size_b / tp->write_block_size_b); j++) {
            size_t write_start = tp->write_block_size_b * j;
            size_t write_end = write_start + tp->write_block_size_b;
            log_debug("Writing... %d id=%d start=%d end=%d", i, RGN_ID(i),
                      write_start, write_end);

            // Get the AFV expected write head
            ssize_t write_head = afv_checkpoint_head(pCheckpoint, NULL);
            if (write_head < 0) {
                log_error("Fetch Write Head Failed");
            }

            // The two heads must always match
            assert(((size_t) write_head) == write_start);

            ssize_t bytes_written =
                afv_checkpoint_write(pCheckpoint, RGN_ID(i),
                                     ((uint8_t *) pBlock) + (write_start),
                                     tp->write_block_size_b);
            // Assert actual written == expected
            assert(bytes_written == (write_end - write_start));
        }
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

    log_info("TEST SUCCESS");

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
