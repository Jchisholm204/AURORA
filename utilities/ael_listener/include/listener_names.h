/**
 * @file listener_names.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief Printable names for AEL event and origin codes
 * @version 0.1
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _LISTENER_NAMES_H_
#define _LISTENER_NAMES_H_

#include "ael/ael_packet.h"

#include <stdbool.h>

/**
 * @brief Get the CSV/stdout name of an event code
 *
 * @param event Event code
 * @return Constant name string, "unknown" when out of range
 */
static inline const char *listener_event_name(eAEL_event event) {
    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        return "cli_connect";
    case eAEL_EVENT_CLI_FAILURE:
        return "cli_failure";
    case eAEL_EVENT_CLI_DISCONNECT:
        return "cli_disconnect";
    case eAEL_EVENT_CHECKPOINT_START:
        return "checkpoint_start";
    case eAEL_EVENT_CHECKPOINT_END:
        return "checkpoint_end";
    case eAEL_EVENT_RESTORE_START:
        return "restore_start";
    case eAEL_EVENT_RESTORE_END:
        return "restore_end";
    case eAEL_EVENT_REGION_ADD:
        return "region_add";
    case eAEL_EVENT_REGION_REMOVE:
        return "region_remove";
    default:
        return "unknown";
    }
}

/**
 * @brief Get the CSV/stdout name of an origin code
 *
 * @param origin Origin code
 * @return Constant name string, "unknown" when out of range
 */
static inline const char *listener_origin_name(eAEL_origin origin) {
    switch (origin) {
    case eAEL_ORIGIN_ARE:
        return "ARE";
    case eAEL_ORIGIN_MPI:
        return "MPI";
    case eAEL_ORIGIN_SMARTNIC:
        return "SmartNIC";
    case eAEL_ORIGIN_HOST:
        return "Host";
    default:
        return "unknown";
    }
}

/**
 * @brief Whether an event is a client lifecycle event
 *        (connect, failure, or disconnect)
 */
static inline bool listener_event_is_lifecycle(eAEL_event event) {
    return event == eAEL_EVENT_CLI_CONNECT || event == eAEL_EVENT_CLI_FAILURE ||
           event == eAEL_EVENT_CLI_DISCONNECT;
}

#endif
