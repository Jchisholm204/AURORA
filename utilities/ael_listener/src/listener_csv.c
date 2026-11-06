/**
 * @file listener_csv.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief CSV sink for received AEL messages
 * @version 0.1
 * @date Created: 2026-11-06
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 */

#include "listener_csv.h"

#include "listener_names.h"
#include "log.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#define CSV_HEADER                                                             \
    "origin,event_id,gid,rank,timestamp,id_version,size,name,from_ip,"         \
    "from_port,event_message\n"

FILE *listener_csv_open(const char *path) {
    if (!path) {
        log_error("NULL Parameter");
        return NULL;
    }

    FILE *fp = fopen(path, "a");
    if (!fp) {
        log_error("Failed to open CSV '%s': %s", path, strerror(errno));
        return NULL;
    }

    /* Append mode: the position after seeking to the end is the file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        log_error("Failed to seek CSV '%s': %s", path, strerror(errno));
        goto fail;
    }
    long size = ftell(fp);
    if (size < 0) {
        log_error("Failed to size CSV '%s': %s", path, strerror(errno));
        goto fail;
    }
    if (size == 0) {
        if (fputs(CSV_HEADER, fp) == EOF || fflush(fp) != 0) {
            log_error("Failed to write CSV header to '%s': %s", path,
                      strerror(errno));
            goto fail;
        }
    }

    return fp;

fail:
    (void) fclose(fp);
    return NULL;
}

/**
 * @brief Write a string field, quoting it when it holds CSV metacharacters
 *
 * Message strings come off the network and are untrusted, so any comma,
 * quote, or line break must not be able to break the row structure.
 */
static int _csv_write_string(FILE *fp, const char *str, size_t max_len) {
    assert(fp != NULL);
    assert(str != NULL);

    size_t len = strnlen(str, max_len);
    bool quote = strcspn(str, ",\"\r\n") < len;

    if (!quote) {
        return fwrite(str, 1, len, fp) == len ? 0 : EOF;
    }

    if (fputc('"', fp) == EOF) {
        return EOF;
    }
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '"' && fputc('"', fp) == EOF) {
            return EOF;
        }
        if (fputc(str[i], fp) == EOF) {
            return EOF;
        }
    }
    return fputc('"', fp) == EOF ? EOF : 0;
}

bool listener_csv_write(FILE *fp, const ael_message_t *pMsg) {
    if (!fp || !pMsg) {
        log_error("NULL Parameter");
        return false;
    }

    char ts[32];
    char ip[INET_ADDRSTRLEN];
    (void) listener_format_time(pMsg->timestamp_ns, ts, sizeof(ts));
    (void) listener_format_ip(pMsg->from_ip, ip, sizeof(ip));

    int status = fprintf(fp, "%s,%s,%" PRIu64 ",%" PRIu64 ",%s,%" PRId64
                             ",%" PRIu64 ",",
                         listener_origin_name(pMsg->origin),
                         listener_event_name(pMsg->event), pMsg->group_id,
                         pMsg->rank, ts, pMsg->payload.id_version,
                         pMsg->payload.size);
    if (status < 0 ||
        _csv_write_string(fp, pMsg->payload.name, AEL_NAME_LEN) == EOF ||
        fprintf(fp, ",%s,%u,", ip, (unsigned) pMsg->from_port) < 0 ||
        _csv_write_string(fp, pMsg->event_message, AEL_EVENT_MSG_LEN) == EOF ||
        fputc('\n', fp) == EOF || fflush(fp) != 0) {
        log_error("Failed to write CSV row: %s", strerror(errno));
        return false;
    }
    return true;
}

char *listener_format_time(uint64_t timestamp_ns, char *buf, size_t len) {
    assert(buf != NULL);
    assert(len > 0);

    time_t secs = (time_t) (timestamp_ns / 1000000000ull);
    unsigned ms = (unsigned) ((timestamp_ns % 1000000000ull) / 1000000ull);
    struct tm tm;

    if (!gmtime_r(&secs, &tm)) {
        (void) snprintf(buf, len, "%" PRIu64, timestamp_ns);
        return buf;
    }

    size_t n = strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &tm);
    if (n == 0) {
        (void) snprintf(buf, len, "%" PRIu64, timestamp_ns);
        return buf;
    }
    (void) snprintf(buf + n, len - n, ".%03uZ", ms);
    return buf;
}

char *listener_format_ip(uint32_t ip, char *buf, size_t len) {
    assert(buf != NULL);

    struct in_addr addr = {.s_addr = htonl(ip)};
    if (!inet_ntop(AF_INET, &addr, buf, (socklen_t) len)) {
        (void) snprintf(buf, len, "0x%08x", ip);
    }
    return buf;
}
