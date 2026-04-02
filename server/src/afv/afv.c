/**
 * @file afv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief
 * @version 0.1
 * @date Created: 2026-04-01
 * @modified Last Modified: 2026-04-01
 *
 * @copyright Copyright (c) 2026
 */

#include "afv/afv.h"

afv_hndl *afv_create_instance(uint64_t rank, uint64_t group_id,
                              int64_t group_size, char *persistent_path,
                              bool with_ec) {
}

void afv_destroy_instance(afv_hndl **ppHndl) {
}

afv_metadata_t *afv_get_latest(afv_hndl *pHndl, int64_t group_id,
                               uint64_t rank) {
}

int afv_set_latest(afv_hndl *pHndl, int64_t group_id, uint64_t rank,
                   afv_metadata_t *pMetadata) {
}
