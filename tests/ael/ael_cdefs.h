/**
 * @file ael_cdefs.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek V4.1 Flash Free
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
 * Cycles through every event code in the enum.
 *
 * @param index Sequence index
 * @return Event code for the given index
 */
static inline eAEL_event ael_event_code(unsigned long index) {
    static const eAEL_event codes[] = {
        eAEL_EVENT_CLI_CONNECT,    eAEL_EVENT_CLI_FAILURE,
        eAEL_EVENT_CLI_DISCONNECT, eAEL_EVENT_CHECKPOINT_START,
        eAEL_EVENT_CHECKPOINT_END, eAEL_EVENT_RESTORE_START,
        eAEL_EVENT_RESTORE_END,    eAEL_EVENT_REGION_ADD,
        eAEL_EVENT_REGION_REMOVE,
    };
    return codes[index % (sizeof(codes) / sizeof(codes[0]))];
}

#endif
