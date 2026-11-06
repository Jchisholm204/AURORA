/**
 * @file main.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Connor Leung (https://github.com/connor-leung)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief AEL aggregate listener: logs AEL events to stdout and a CSV
 * @version 0.2
 * @date Created: 2026-11-03
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 *
 * Usage: ael_listener PORT [LEVEL] [CSV_LOCATION]
 */

#include "main.h"

#include "ael/ael.h"
#include "listener_csv.h"
#include "listener_group.h"
#include "listener_names.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LISTENER_DEFAULT_LEVEL eLISTENER_LEVEL_GROUP_ALL

static volatile sig_atomic_t g_stop = 0;

static const struct {
    const char *name;
    eLISTENER_level level;
} g_levels[] = {
    {"none", eLISTENER_LEVEL_NONE},
    {"group-light", eLISTENER_LEVEL_GROUP_LIGHT},
    {"group-all", eLISTENER_LEVEL_GROUP_ALL},
    {"light", eLISTENER_LEVEL_LIGHT},
    {"all", eLISTENER_LEVEL_ALL},
};

#define N_LEVELS (sizeof(g_levels) / sizeof(g_levels[0]))

struct listener_stats {
    uint64_t received;
    uint64_t discarded;
    uint64_t csv_rows;
};

static void _signal_handler(int signal) {
    (void) signal;
    g_stop = 1;
}

static void _print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s PORT [LEVEL] [CSV_LOCATION]\n", prog);
    fprintf(stderr, "  PORT          UDP port to receive AEL messages on "
                    "(1..65535)\n");
    fprintf(stderr, "  LEVEL         stdout logging level (default: %s):\n",
            g_levels[LISTENER_DEFAULT_LEVEL].name);
    fprintf(stderr, "                  none         internal listener events "
                    "only\n");
    fprintf(stderr, "                  group-light  group connect/disconnect\n");
    fprintf(stderr, "                  group-all    all group events\n");
    fprintf(stderr, "                  light        group-all + per-rank "
                    "connect/disconnect/failure\n");
    fprintf(stderr, "                  all          group-all + every "
                    "per-rank event\n");
    fprintf(stderr, "  CSV_LOCATION  Append every received message to this "
                    "CSV (optional; LEVEL does not filter it)\n");
}

static bool _parse_port(const char *str, uint16_t *pPort) {
    char *end = NULL;
    errno = 0;
    unsigned long val = strtoul(str, &end, 10);
    if (errno || end == str || *end != '\0' || val < 1 || val > UINT16_MAX) {
        log_error("Invalid PORT '%s' (must be 1..65535)", str);
        return false;
    }
    *pPort = (uint16_t) val;
    return true;
}

static bool _parse_level(const char *str, eLISTENER_level *pLevel) {
    for (size_t i = 0; i < N_LEVELS; i++) {
        if (strcmp(str, g_levels[i].name) == 0) {
            *pLevel = g_levels[i].level;
            return true;
        }
    }
    log_error("Invalid LEVEL '%s'", str);
    return false;
}

static bool _group_visible(eLISTENER_level level, eAEL_event event) {
    if (level >= eLISTENER_LEVEL_GROUP_ALL) {
        return true;
    }
    if (level == eLISTENER_LEVEL_GROUP_LIGHT) {
        return event == eAEL_EVENT_CLI_CONNECT ||
               event == eAEL_EVENT_CLI_DISCONNECT;
    }
    return false;
}

static bool _rank_visible(eLISTENER_level level, eAEL_event event) {
    if (level == eLISTENER_LEVEL_ALL) {
        return true;
    }
    if (level == eLISTENER_LEVEL_LIGHT) {
        return listener_event_is_lifecycle(event);
    }
    return false;
}

static void _log_group(const ael_message_t *pMsg,
                       const listener_group_result_t *pRes) {
    if (pRes->duplicate || (!pRes->first && !pRes->last)) {
        return;
    }

    char key[32] = "";
    if (pRes->keyed) {
        (void) snprintf(key, sizeof(key), " id=%" PRId64, pRes->key);
    }
    char size[24] = "?";
    if (pRes->size > 0) {
        (void) snprintf(size, sizeof(size), "%" PRIu64, pRes->size);
    }

    const char *phase = pRes->last ? "complete" : "first";
    log_info("[group %s:%" PRIu64 "] %s%s %s (%zu/%s) rank=%" PRIu64,
             listener_origin_name(pMsg->origin), pMsg->group_id,
             listener_event_name(pMsg->event), key, phase, pRes->count, size,
             pMsg->rank);
}

/**
 * @brief Copy an untrusted string for terminal output, escaping quotes,
 *        backslashes, and non-printable bytes so one message stays one line
 *
 * @param dst Output buffer; 4 * src_len + 1 bytes always fits
 */
static void _escape(char *dst, size_t dst_len, const char *src,
                    size_t src_len) {
    size_t o = 0;
    for (size_t i = 0; i < src_len && src[i] != '\0'; i++) {
        unsigned char c = (unsigned char) src[i];
        int n;
        if (c == '"' || c == '\\') {
            n = snprintf(dst + o, dst_len - o, "\\%c", c);
        } else if (c == '\n') {
            n = snprintf(dst + o, dst_len - o, "\\n");
        } else if (c < 0x20 || c >= 0x7f) {
            n = snprintf(dst + o, dst_len - o, "\\x%02x", c);
        } else {
            n = snprintf(dst + o, dst_len - o, "%c", c);
        }
        if (n < 0 || (size_t) n >= dst_len - o) {
            break;
        }
        o += (size_t) n;
    }
    dst[o] = '\0';
}

static void _log_rank(const ael_message_t *pMsg) {
    char ts[32];
    char ip[INET_ADDRSTRLEN];
    char name[4 * AEL_NAME_LEN + 1];
    char text[4 * AEL_EVENT_MSG_LEN + 1];
    (void) listener_format_time(pMsg->timestamp_ns, ts, sizeof(ts));
    (void) listener_format_ip(pMsg->from_ip, ip, sizeof(ip));
    _escape(name, sizeof(name), pMsg->payload.name, AEL_NAME_LEN);
    _escape(text, sizeof(text), pMsg->event_message, AEL_EVENT_MSG_LEN);

    log_info("[rank %s:%" PRIu64 ":%" PRIu64 "] %s id_version=%" PRId64
             " size=%" PRIu64 " name=\"%s\" msg=\"%s\" from=%s:%u ts=%s",
             listener_origin_name(pMsg->origin), pMsg->group_id, pMsg->rank,
             listener_event_name(pMsg->event), pMsg->payload.id_version,
             pMsg->payload.size, name, text, ip, (unsigned) pMsg->from_port,
             ts);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0) {
        _print_usage(argv[0]);
        return argc == 2 ? 0 : 1;
    }

    uint16_t port = 0;
    eLISTENER_level level = LISTENER_DEFAULT_LEVEL;
    const char *csv_path = argc > 3 ? argv[3] : NULL;
    if (!_parse_port(argv[1], &port) ||
        (argc > 2 && !_parse_level(argv[2], &level))) {
        _print_usage(argv[0]);
        return 1;
    }

    /* Do not use SA_RESTART so poll wakes up on Ctrl+C */
    struct sigaction sa = {.sa_handler = _signal_handler};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    (void) sigaction(SIGINT, &sa, NULL);
    (void) sigaction(SIGTERM, &sa, NULL);

    int ret = 1;
    FILE *csv = NULL;
    listener_groups_t *pGroups = NULL;
    ael_hndl *pAEL = NULL;
    struct listener_stats stats = {0};

    if (csv_path) {
        csv = listener_csv_open(csv_path);
        if (!csv) {
            goto cleanup;
        }
    }

    pGroups = listener_groups_create();
    if (!pGroups) {
        goto cleanup;
    }

    /* Receive-only handle: no origin, rank, group, or recipients */
    pAEL = ael_create_instance(eAEL_N_ORIGIN, 0, 0, 0, port, 0);
    if (!pAEL) {
        goto cleanup;
    }

    log_info("AEL listener on UDP port %u, level=%s, csv=%s (Ctrl+C to stop)",
             (unsigned) port, g_levels[level].name,
             csv_path ? csv_path : "(none)");

    while (!g_stop) {
        ael_message_t msg;
        eAEL_error status = ael_recv_message(pAEL, &msg, -1);

        if (status != eAEL_OK) {
            if (status & eAEL_ERR_FATAL) {
                log_error("AEL Error: %d", (int) status);
                goto cleanup;
            }
            if (status & (eAEL_ERR_SIZE | eAEL_ERR_CRC | eAEL_ERR_EVENT |
                          eAEL_ERR_ORIGIN)) {
                stats.discarded++;
            }
            continue;
        }
        stats.received++;

        if (csv) {
            if (!listener_csv_write(csv, &msg)) {
                goto cleanup;
            }
            stats.csv_rows++;
        }

        listener_group_result_t res;
        if (!listener_groups_record(pGroups, &msg, &res)) {
            goto cleanup;
        }
        if (_rank_visible(level, msg.event)) {
            _log_rank(&msg);
        }
        if (_group_visible(level, msg.event)) {
            _log_group(&msg, &res);
        }
    }

    ret = 0;

cleanup:
    if (pAEL) {
        log_info("AEL listener stopped: received=%" PRIu64
                 " discarded=%" PRIu64 " csv_rows=%" PRIu64 " groups=%zu",
                 stats.received, stats.discarded, stats.csv_rows,
                 listener_groups_count(pGroups));
        ael_destroy_instance(&pAEL);
    }
    listener_groups_destroy(&pGroups);
    if (csv && fclose(csv) != 0) {
        log_error("Failed to close CSV '%s': %s", csv_path, strerror(errno));
        ret = 1;
    }
    return ret;
}
