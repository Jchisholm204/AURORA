/**
 * @file arm_am.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief ARM Active Messages Layer
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * @copyright Copyright (c) 2026
 */

#define ARM_INTERNAL

#include "arm/arm.h"
#include "arm/arm_am.h"

ucs_status_t _arm_add_rgn_cb(void *arg, void *header, size_t header_len,
                                    void *data, size_t data_len,
                                    const ucp_am_recv_param_t *pParam){

}

ucs_status_t _arm_rm_rgn_cb(void *arg, void *header, size_t header_len,
                                   void *data, size_t data_len,
                                   const ucp_am_recv_param_t *pParam){

}
