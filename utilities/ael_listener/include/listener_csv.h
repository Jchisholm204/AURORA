/**
 * @file listener_csv.h
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief CSV sink for received AEL messages
 * @version 0.1
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _LISTENER_CSV_H_
#define _LISTENER_CSV_H_

#include "ael/ael.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Open a CSV file for appending
 *
 * The header row is written only when the file is new or empty, so repeated
 * runs append to one table.
 *
 * @param path File path
 * @return Open stream, or NULL on failure
 */
extern FILE *listener_csv_open(const char *path);

/**
 * @brief Append one received message as a CSV row and flush it
 *
 * @param fp Stream from listener_csv_open
 * @param pMsg Received message
 * @return true on success
 */
extern bool listener_csv_write(FILE *fp, const ael_message_t *pMsg);

/**
 * @brief Format a nanosecond epoch timestamp as ISO 8601 UTC
 *        (e.g. 2026-11-04T15:30:45.123Z)
 *
 * @param timestamp_ns Nanoseconds since the Unix epoch
 * @param buf Output buffer
 * @param len Output buffer length (32 bytes is sufficient)
 * @return buf
 */
extern char *listener_format_time(uint64_t timestamp_ns, char *buf, size_t len);

/**
 * @brief Format a host byte order IPv4 address as dotted decimal
 *
 * @param ip Address in host byte order
 * @param buf Output buffer of at least INET_ADDRSTRLEN bytes
 * @param len Output buffer length
 * @return buf
 */
extern char *listener_format_ip(uint32_t ip, char *buf, size_t len);

#endif
