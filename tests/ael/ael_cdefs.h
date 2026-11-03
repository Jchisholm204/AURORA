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

#define PORT 8334

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

#endif
