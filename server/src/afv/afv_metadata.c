/**
 * @file afv_metadata.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#include "afv/afv_metadata.h"

const afv_metadata_t *afv_create_metadata(
    uint64_t rank, uint64_t version, char *chkpt_name, size_t n_regions,
    uint64_t *region_ids, char *region_names[static AVF_RGN_NAME_LEN]) {
}

void afv_destroy_metadata(afv_metadata_t **ppHndl) {
}
