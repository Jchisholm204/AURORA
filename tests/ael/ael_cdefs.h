/**
 * @file ael_cdefs.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief Shared AEL event helpers.
 * @version 0.1
 * @date Created: 2026-10-25
 * @modified Last Modified: 2026-10-26
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_CDEFS_H_
#define _AEL_CDEFS_H_

#include "ael/ael.h"
#include "ael/ael_packet.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define PORT 8334
#define AEL_SAMPLE_REGION_SIZE 4096
#define AEL_DEFAULT_PACKETS 32

#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the printable name of an event code
 *
 * @param code Event code to name
 * @return Constant event name string
 */
static inline const char *ael_event_name(eAEL_event code) {
    switch (code) {
    case eAEL_EVENT_CLI_CONNECT:
        return "eAEL_EVENT_CLI_CONNECT";
    case eAEL_EVENT_CLI_FAILURE:
        return "eAEL_EVENT_CLI_FAILURE";
    case eAEL_EVENT_CLI_DISCONNECT:
        return "eAEL_EVENT_CLI_DISCONNECT";
    case eAEL_EVENT_CHECKPOINT_START:
        return "eAEL_EVENT_CHECKPOINT_START";
    case eAEL_EVENT_CHECKPOINT_END:
        return "eAEL_EVENT_CHECKPOINT_END";
    case eAEL_EVENT_RESTORE_START:
        return "eAEL_EVENT_RESTORE_START";
    case eAEL_EVENT_RESTORE_END:
        return "eAEL_EVENT_RESTORE_END";
    case eAEL_EVENT_REGION_ADD:
        return "eAEL_EVENT_REGION_ADD";
    case eAEL_EVENT_REGION_REMOVE:
        return "eAEL_EVENT_REGION_REMOVE";
    default:
        return "unknown";
    }
}

/**
 * @brief Select an event code by sequence index
 *
 * Event codes are contiguous from zero to eAEL_N_EVENT, so the index maps
 * straight onto the enum.
 *
 * @param index Sequence index
 * @return Event code for the given index, or eAEL_N_EVENT if out of range
 */
static inline eAEL_event ael_event_code(unsigned long index) {
    if (index >= (unsigned long) eAEL_N_EVENT) {
        return eAEL_N_EVENT;
    }
    return (eAEL_event) index;
}

/**
 * @brief Send a test event to the AEL server
 *
 * @param pHndl AEL instance handle
 * @param rank Rank of the sender
 * @param event Event code to send
 * @param index Sequence index for region events
 * @return eAEL_OK on success, error code on failure
 */
static inline eAEL_error _send_test_event(ael_hndl *pHndl, uint64_t rank,
                                         eAEL_event event, unsigned long index) {
    char event_message[AEL_EVENT_MSG_LEN];
    char name[AEL_NAME_LEN];

    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " connect", rank);
        return ael_send_connect(pHndl, event_message);
    case eAEL_EVENT_CLI_FAILURE:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " failure", rank);
        return ael_send_failure(pHndl, -1, "failure occurred", event_message);
    case eAEL_EVENT_CLI_DISCONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " disconnect", rank);
        return ael_send_disconnect(pHndl, event_message);
    case eAEL_EVENT_CHECKPOINT_START:
        return ael_send_checkpoint(pHndl, event, 1, 4, "ckpt-start", NULL);
    case eAEL_EVENT_CHECKPOINT_END:
        return ael_send_checkpoint(pHndl, event, 2, 4, "ckpt-end", NULL);
    case eAEL_EVENT_RESTORE_START:
        return ael_send_checkpoint(pHndl, event, 1, 4, "restore-start", NULL);
    case eAEL_EVENT_RESTORE_END:
        return ael_send_checkpoint(pHndl, event, 2, 4, "restore-end", NULL);
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        snprintf(name, AEL_NAME_LEN, "rgn-%lu", index);
        return ael_send_region(pHndl, event, (int64_t) index,
                               AEL_SAMPLE_REGION_SIZE, name, NULL);
    default:
        return eAEL_ERR_EVENT | eAEL_ERR_FATAL;
    }
}

#endif
