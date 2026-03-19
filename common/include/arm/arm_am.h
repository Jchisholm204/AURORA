/**
 * @file arm_am.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief AURORA Region Manager Active Message Layer
 * @version 0.1
 * @date Created: 2026-03-19
 * @modified Last Modified: 2026-03-19
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _ARM_AM_H_
#define _ARM_AM_H_

#ifdef ARM_INTERNAL

#include <ucp/api/ucp.h>

#ifndef ARM_UCX_ID
#define ARM_UCX_ID
// ID for Add region handler
#define ARM_UCX_ID_ADD 43
// ID for remove region handler
#define ARM_UCX_ID_RM 44
#endif

extern ucs_status_t _arm_add_rgn_cb(void *arg, void *header, size_t header_len,
                                    void *data, size_t data_len,
                                    const ucp_am_recv_param_t *pParam);

extern ucs_status_t _arm_rm_rgn_cb(void *arg, void *header, size_t header_len,
                                   void *data, size_t data_len,
                                   const ucp_am_recv_param_t *pParam);

#endif
#endif
