/**
 * @file test_afv_metadata.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-09-22
 * @modified Last Modified: 2026-09-22
 *
 * @copyright Copyright (c) 2026
 */

#include "afv/afv.h"
#include "afv/afv_metadata.h"
// #include "afv_test_fillmem.h"
#include "afv_test_parser.h"
#include "log.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

// Arbitrary ID Generation
#define RGN_ID(i) ((i + 1) * 2)

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

    afv_metadata_t *pMetadata =
        (afv_metadata_t *) afv_read_metadata(pAFV, -1, NULL);

    if (pMetadata) {
        log_warn("Found Existing Metadata");
        log_info("%s : %d -> %d regions", pMetadata->chkpt_name,
                 pMetadata->version, pMetadata->n_regions);
        for (int i = 0; i < pMetadata->n_regions; i++) {
            log_info("\t %s id=%d size=%ld", pMetadata->region_names[i],
                     pMetadata->region_ids[i], pMetadata->region_sizes[i]);
        }
        log_trace("Destroying internal pointer before continuing");
        afv_metadata_destroy(&pMetadata);
    } else {
        log_info("Found No Existing Metadata");
    }

    pMetadata = afv_metadata_create(tp->n_regions);

    if (!pMetadata) {
        log_error("Failed to create test metadata");
        log_trace("Destroying AFV Instanace");
        afv_destroy_instance(&pAFV);
        return 1;
    }

    // Expect failure on no version
    log_info("Expect Failure");
    eAFV_verif afv_write_status = afv_write_metadata(pAFV, pMetadata);

    if (afv_write_status != eAFV_VERIF_OK) {
        log_info("TEST: AFV refuesed to write corrputed metadata to disk "
                 "with error "
                 "eAFV_verif=%d, eAFV_VERIF_ERR_VERSION=%d",
                 afv_write_status, eAFV_VERIF_ERR_VERSION);
    } else {
        log_error(
            "TEST: AFV wrote corrupted metadata to disk with eAFV_verif=%d",
            afv_write_status);
        log_error("Test Failed");
    }

    pMetadata->version = 1;
    snprintf(pMetadata->chkpt_name, AFV_CKPT_NAME_LEN, "%s", "Test");
    for (int i = 0; i < pMetadata->n_regions; i++) {
        snprintf(pMetadata->region_names[i], AFV_RGN_NAME_LEN, "tst%d", i);
        // Arbitrary ID Generation
        pMetadata->region_ids[i] = RGN_ID(i);
        pMetadata->region_sizes[i] = tp->region_sizes_mb * 1024 * 1024;
        log_info("\t %s id=%d size=%ld", pMetadata->region_names[i],
                 pMetadata->region_ids[i], pMetadata->region_sizes[i]);
    }

    afv_write_status = afv_write_metadata(pAFV, pMetadata);

    if (afv_write_status != eAFV_VERIF_OK) {
        log_error("TEST: AFV failed to write metadata to disk with error "
                  "eAFV_verif=%d",
                  afv_write_status);
        log_error("Test Failed");
        log_trace("Destroying AFV Instanace");
        afv_destroy_instance(&pAFV);
        return 1;
    } else {
        log_info("TEST: AFV wrote metadata to disk with eAFV_verif=%d",
                 afv_write_status);
    }

    const afv_metadata_t *pCurrentMetadata =
        afv_read_metadata(pAFV, -1, pMetadata->chkpt_name);

    if (pCurrentMetadata->n_regions != tp->n_regions) {
        log_error("Current Metadata did not match the saved metadata");
    }

    if (pCurrentMetadata->version != pMetadata->version) {
        log_error("Current Metadata did not match the saved metadata");
    }

    afv_metadata_destroy(&pMetadata);
    afv_metadata_destroy((afv_metadata_t **) &pCurrentMetadata);

    log_trace("Destroying AFV Instanace");
    afv_destroy_instance(&pAFV);

    return 0;
}
