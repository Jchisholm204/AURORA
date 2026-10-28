/**
 * @file multi_client.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @brief Multi-threaded AEL UDP client for testing concurrent connections.
 *
 * Each thread creates an AEL instance and sends events to a single server
 * recipient at (server_ip, server_port) through the AEL event send helpers.
 * All threads share the same server configuration but use unique ranks.
 * Thread i automatically uses rank i.
 *
 * Command-line arguments (all optional, positional):
 *   [server_ip]      - IP address of the AEL server (default: 127.0.0.1)
 *   [num_threads]    - Number of concurrent threads, 1..1024 (default: 4)
 *   [num_messages]   - Packets sent per thread (default: AEL_DEFAULT_PACKETS)
 *   [server_port]    - Server UDP port in host byte order, 1..65535 (default: PORT)
 *
 * Example: ./multi_client 127.0.0.1 4 64 8335
 *   - Creates 4 threads with ranks 0-3, each sending to server on port 8335
 *   - Each rank sends 64 events (cycling through all event types)
 *
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#include "ael_cdefs.h"
#include "log.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AEL_DEFAULT_PACKETS 32
#define AEL_SAMPLE_REGION_SIZE 4096
#define AEL_MAX_THREADS 1024

typedef struct {
    const char *server_ip;
    int num_threads;
    unsigned long packet_count;
    uint16_t server_port;
} test_params_t;

typedef struct {
    long rank;
    const test_params_t *tp;  // Pointer to shared configuration (const prevents accidental modification)
} thread_params_t;

static void _ael_usage(const char *prog);
static test_params_t *ael_parse_params(int argc, char **argv);
static eAEL_error _ael_send_event(ael_hndl *pHndl, uint64_t rank,
                                  eAEL_event event, unsigned long index);
static void *thread_func(void *arg);

static void _ael_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [server_ip] [num_threads] [num_messages] [server_port]\n", prog);
    fprintf(stderr, "Example: %s 127.0.0.1 4 64 8335\n", prog);
    fprintf(stderr, "  - server_ip: IP address of the AEL server (default: 127.0.0.1)\n");
    fprintf(stderr, "  - num_threads: Number of threads, 1..%d (default: 4)\n", AEL_MAX_THREADS);
    fprintf(stderr, "  - num_messages: Packets per thread (default: %d)\n", AEL_DEFAULT_PACKETS);
    fprintf(stderr, "  - server_port: Server UDP port in host byte order, 1..65535 (default: %d)\n", PORT);
}

static test_params_t *ael_parse_params(int argc, char **argv) {
    test_params_t *tp = malloc(sizeof(*tp));
    if (!tp) {
        log_error("Failed to allocate AEL test parameters");
        return NULL;
    }

    tp->server_ip = "127.0.0.1";
    tp->num_threads = 4;       // Default 4 threads
    tp->packet_count = AEL_DEFAULT_PACKETS;  // Default packets per thread
    tp->server_port = PORT;    // Default server port

    // Parse server_ip from argv[1]
    if (argc > 1) {
        if (argv[1][0] == '\0') {
            log_error("Invalid server_ip: empty string");
            goto fail;
        }
        tp->server_ip = argv[1];
    }

    // Parse num_threads from argv[2]
    if (argc > 2) {
        char *end = NULL;
        errno = 0;
        long num_threads = strtol(argv[2], &end, 10);
        if (errno || end == argv[2] || *end != '\0' ||
            num_threads < 1 || num_threads > AEL_MAX_THREADS) {
            log_error("Invalid num_threads '%s' (must be 1..%d)",
                      argv[2], AEL_MAX_THREADS);
            goto fail;
        }
        tp->num_threads = (int) num_threads;
    }

    // Parse packet_count from argv[3] (optional, defaults to AEL_DEFAULT_PACKETS)
    if (argc > 3) {
        char *end = NULL;
        errno = 0;
        unsigned long packet_count = strtoul(argv[3], &end, 10);
        if (errno || end == argv[3] || *end != '\0' || packet_count == 0) {
            log_error("Invalid num_messages '%s' (must be positive)", argv[3]);
            goto fail;
        }
        tp->packet_count = packet_count;
    }

    // Parse optional server port from argv[4]
    if (argc > 4) {
        char *end = NULL;
        errno = 0;
        unsigned long port_val = strtoul(argv[4], &end, 10);
        if (errno || end == argv[4] || *end != '\0' ||
            port_val < 1 || port_val > UINT16_MAX) {
            log_error("Invalid server_port '%s' (must be 1..65535)", argv[4]);
            goto fail;
        }
        tp->server_port = (uint16_t) port_val;
    }

    if (argc > 5) {
        log_error("Unexpected extra argument '%s'", argv[5]);
        goto fail;
    }

    return tp;

fail:
    _ael_usage(argv[0]);
    free(tp);
    return NULL;
}

static eAEL_error _ael_send_event(ael_hndl *pHndl, uint64_t rank,
                                  eAEL_event event, unsigned long index) {
    char event_message[AEL_EVENT_MSG_LEN];
    char name[AEL_NAME_LEN];

    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " connect", rank);
        return ael_send_cli(pHndl, event, event_message);
    case eAEL_EVENT_CLI_FAILURE:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " failure", rank);
        return ael_send_cli(pHndl, event, event_message);
    case eAEL_EVENT_CLI_DISCONNECT:
        snprintf(event_message, AEL_EVENT_MSG_LEN,
                 "client %" PRIu64 " disconnect", rank);
        return ael_send_cli(pHndl, event, event_message);
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
        log_error("Unsupported event code %d", (int) event);
        return eAEL_ERR_EVENT;
    }
}

static void *thread_func(void *arg) {
    thread_params_t *params = (thread_params_t *) arg;
    const test_params_t *tp = params->tp;  // Dereference config pointer
    
    // Create AEL instance with rank from params
    ael_hndl *pHndl = ael_create_instance(params->rank, 0, 0, 1);
    if (!pHndl) {
        log_error("Failed to create AEL instance for thread");
        return NULL;
    }

    // Add the server as a single recipient peer
    eAEL_error status = ael_add_recipient(pHndl, tp->server_ip, tp->server_port);
    if (status != eAEL_OK) {
        log_error("Failed to add recipient for rank %ld: eAEL_error=%d", params->rank, (int) status);
        ael_destroy_instance(&pHndl);
        return NULL;
    }

    // Send events to server
    log_info("Thread %ld sending %lu packet(s) to %s:%u",
             params->rank, tp->packet_count, tp->server_ip, tp->server_port);

    unsigned long sent = 0;
    for (unsigned long i = 0; i < tp->packet_count; i++) {
        eAEL_event event = ael_event_code(i);
        eAEL_error tx_status =
            _ael_send_event(pHndl, (uint64_t) params->rank, event, i);
        if (tx_status != eAEL_OK) {
            log_error("Failed to send %s from rank %ld: eAEL_error=%d",
                      ael_event_name(event), params->rank, (int) tx_status);
            break;
        }

        log_info("tx rank=%ld ev=%s (%zu bytes)", params->rank,
                 ael_event_name(event), sizeof(ael_packet_t));
        sent++;
    }

    // Log completion and release the instance
    log_info("Thread completed: sent %lu packet(s) from rank %ld", sent,
             params->rank);
    ael_destroy_instance(&pHndl);
    
    return NULL;
}

int main(int argc, char **argv) {
    // Support -h/--help without requiring any positional arguments
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        _ael_usage(argv[0]);
        return 0;
    }

    // Parse all arguments into consolidated params struct
    test_params_t *tp = ael_parse_params(argc, argv);
    if (!tp) {
        return 1;
    }

    pthread_t *threads = malloc(sizeof(pthread_t) * (size_t) tp->num_threads);
    thread_params_t *thread_params = malloc(sizeof(thread_params_t) * (size_t) tp->num_threads);
    
    if (!threads || !thread_params) {
        log_error("Failed to allocate thread structures");
        free(threads);
        free(thread_params);
        free(tp);
        return 1;
    }

    // Initialize and start threads
    pthread_attr_t attr;
    int status = 0;
    int created_threads = 0;
    for (int i = 0; i < tp->num_threads && status == 0; i++) {
        thread_params[i].rank = i;
        thread_params[i].tp = tp;  // Pass pointer to shared const config
        
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 8 * 1024 * 1024); // 8MB stack per thread
        
        status = pthread_create(&threads[i], &attr, thread_func, &thread_params[i]);
        pthread_attr_destroy(&attr);
        
        if (status != 0) {
            log_error("Failed to create thread %d: error=%d", i, status);
            break; // Stop creating threads
        }
        created_threads++;
    }
    
    if (created_threads < tp->num_threads) {
        log_warn("Only %d thread(s) created out of requested %d", created_threads, tp->num_threads);
    }

    // Wait for all threads to complete
    for (int i = 0; i < created_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    log_info("Multi-threaded client completed: %d thread(s) sent packets successfully",
             created_threads);

    // Cleanup
    free(threads);
    free(thread_params);
    free(tp);

    return 0;
}
