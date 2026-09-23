/**
 * @file test_afv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-09-22
 * @modified Last Modified: 2026-09-22
 *
 * @copyright Copyright (c) 2026
 */

#include "afv//afv_checkpoint.h"
#include "afv//afv_metadata.h"
#include "afv/afv.h"
#include "log.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

// Arbitrary ID Generation
#define RGN_ID(i) ((i + 1) * 2)

int main(int argc, char **argv) {

    log_info("Starting AFV Tests");

    if (argc < 2) {
        log_error("First argument must be test directory");
        return 1;
    }
    char *filepath = argv[1];

    if (argc < 3) {
        log_warn("Second argument must be number of regions to write");
    }
    size_t test_n_regions = atoi(argv[2]);
    log_trace("Using %d test regions", test_n_regions);

    if (argc < 4) {
        log_error("Third argument must be size of regions to write");
        return 1;
    }
    size_t test_region_size = atoi(argv[3]);
    log_trace("Using test regions of size %d", test_region_size);

    afv_hndl *pAFV = afv_create_instance(0, 0, 1, filepath, false);

    if (!pAFV) {
        log_error("AFV Handle NULL");
        return 1;
    }

    afv_metadata_t *pMetadata = afv_get_metadata_versioned(pAFV, -1, NULL);

    if (pMetadata) {
        log_warn("Found Existing Metadata");
        log_info("%s : %d -> %d regions", pMetadata->chkpt_name,
                 pMetadata->version, pMetadata->n_regions);
        for (int i = 0; i < pMetadata->n_regions; i++) {
            log_info("\t %s id=%d size=%ld", pMetadata->region_names[i],
                     pMetadata->region_ids[i], pMetadata->region_sizes[i]);
        }
        log_trace("Destroying internal pointer before continuing");
        afv_destroy_metadata(&pMetadata);
    } else {
        log_info("Found No Existing Metadata");
    }

    pMetadata = afv_create_metadata(test_n_regions);

    if (!pMetadata) {
        log_error("Failed to create test metadata");
        log_trace("Destroying AFV Instanace");
        afv_destroy_instance(&pAFV);
        return 1;
    }

    // Expect failure on no version
    eAFV_verif afv_write_status = afv_write_metadata(pAFV, pMetadata);

    if (afv_write_status != eAFV_VERIF_OK) {
        log_info("AFV refuesed to write corrputed metadata to disk with error "
                 "eAFV_verif=%d, eAFV_VERIF_ERR_VERSION=%d",
                 afv_write_status, eAFV_VERIF_ERR_VERSION);
    } else {
        log_error("AFV wrote corrupted metadata to disk with eAFV_verif=%d",
                  afv_write_status);
    }

    pMetadata->version = 1;
    snprintf(pMetadata->chkpt_name, AFV_CKPT_NAME_LEN, "%s", "Test");
    for (int i = 0; i < pMetadata->n_regions; i++) {
        snprintf(pMetadata->region_names[i], AFV_RGN_NAME_LEN, "tst%d", i);
        pMetadata->region_sizes[i] = 0;
        // Arbitrary ID Generation
        pMetadata->region_ids[i] = RGN_ID(i);
        log_info("\t %s id=%d size=%ld", pMetadata->region_names[i],
                 pMetadata->region_ids[i], pMetadata->region_sizes[i]);
    }

    afv_write_status = afv_write_metadata(pAFV, pMetadata);

    if (afv_write_status != eAFV_VERIF_OK) {
        log_error("AFV failed to write metadata to disk with error "
                  "eAFV_verif=%d",
                  afv_write_status);
        log_trace("Destroying AFV Instanace");
        afv_destroy_instance(&pAFV);
        return 1;
    } else {
        log_info("AFV wrote metadata to disk with eAFV_verif=%d",
                 afv_write_status);
    }

    const afv_metadata_t *pCurrentMetadata = afv_get_metadata(pAFV);
    if (pCurrentMetadata != pMetadata) {
        log_warn("The Metadata pointer may have been lost (saved to disk?)");
    }
    if (pCurrentMetadata->n_regions != test_n_regions) {
        log_error("Current Metadata did not match the saved metadata");
    }

    if (pCurrentMetadata->version != 1) {
        log_error("Current Metadata did not match the saved metadata");
    }

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
