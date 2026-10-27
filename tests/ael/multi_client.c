/**
 * @file multi_client.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @brief Multi-threaded AEL UDP client for testing concurrent connections.
 *
 * Each thread creates an AEL instance and sends packets to a single server
 * recipient at (server_ip, server_port). All threads share the same server
 * configuration but use unique ranks. Thread i automatically uses rank i.
 *
 * Command-line arguments:
 *   <server_ip>      - IP address of the AEL server (default: 127.0.0.1)
 *   <num_threads>    - Number of concurrent threads (default: 4)
 *   [num_messages]   - Packets sent per thread (default: AEL_DEFAULT_PACKETS)
 *   [server_port]    - Server UDP port in host byte order (default: PORT)
 *
 * Example: ./multi_client 127.0.0.1 4 64 8335
 *   - Creates 4 threads with ranks 0-3, each sending to server on port 8335
 *   - Each rank sends 64 events (cycling through all event types)
 *
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-27
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
#include <unistd.h>

#define AEL_DEFAULT_PACKETS 32
#define AEL_SAMPLE_REGION_SIZE 4096

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

static test_params_t *ael_parse_params(int argc, char **argv);
static ael_packet_t *_ael_packet_create(eAEL_event event, uint64_t rank,
                                        unsigned long index);
static void *thread_func(void *arg);

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

    if (argc < 3) {
        free(tp);
        fprintf(stderr, "Usage: %s <server_ip> <num_threads> [num_messages] [server_port]\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 4 64 8335\n", argv[0]);
        fprintf(stderr, "  - server_ip: IP address of the AEL server\n");
        fprintf(stderr, "  - num_threads: Number of threads (default: 4)\n");
        fprintf(stderr, "  - num_messages: Packets per thread (default: AEL_DEFAULT_PACKETS)\n");
        fprintf(stderr, "  - server_port: Server UDP port in host byte order (default: PORT)\n");
        return NULL;
    }

    // Parse server_ip from argv[1]
    if (argv[1] && argv[1][0] != '\0') {
        tp->server_ip = argv[1];
    }

    // Parse num_threads from argv[2]
    tp->num_threads = atoi(argv[2]);

    // Parse packet_count from argv[3] (optional, defaults to AEL_DEFAULT_PACKETS)
    if (argc >= 5 && argv[4] && argv[4][0] != '\0') {
        char *end = NULL;
        errno = 0;
        unsigned long packet_count_val = strtoul(argv[4], &end, 10);
        if (!errno && end != argv[4] && packet_count_val > 0) {
            tp->packet_count = packet_count_val;
        } else {
            log_error("Invalid packet_count '%s' (must be positive)", argv[4]);
            free(tp);
            return NULL;
        }
    }

    // Parse optional server port from argv[5]
    if (argc >= 6 && argv[5] && argv[5][0] != '\0') {
        char *end = NULL;
        errno = 0;
        unsigned long port_val = strtoull(argv[5], &end, 10);
        if (errno == 0 && end != argv[5] && port_val <= UINT16_MAX) {
            tp->server_port = (uint16_t) port_val;
        } else {
            log_warn("Invalid server port '%s' (defaulting to %d)", argv[5], PORT);
        }
    }

    return tp;
}

static ael_packet_t *_ael_packet_create(eAEL_event event, uint64_t rank,
                                        unsigned long index) {
    char event_message[AEL_EVENT_MSG_LEN];
    char name[AEL_NAME_LEN];

    switch (event) {
    case eAEL_EVENT_CLI_CONNECT:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " connect", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CLI_FAILURE:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " failure", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CLI_DISCONNECT:
        snprintf(event_message, sizeof(event_message),
                 "client %" PRIu64 " disconnect", rank);
        return ael_packet_create_cli(event, rank, 0, event_message);
    case eAEL_EVENT_CHECKPOINT_START:
        return ael_packet_create_checkpoint(event, rank, 0, 1, 4, "ckpt-start");
    case eAEL_EVENT_CHECKPOINT_END:
        return ael_packet_create_checkpoint(event, rank, 0, 2, 4, "ckpt-end");
    case eAEL_EVENT_RESTORE_START:
        return ael_packet_create_checkpoint(event, rank, 0, 1, 4,
                                            "restore-start");
    case eAEL_EVENT_RESTORE_END:
        return ael_packet_create_checkpoint(event, rank, 0, 2, 4,
                                            "restore-end");
    case eAEL_EVENT_REGION_ADD:
    case eAEL_EVENT_REGION_REMOVE:
        snprintf(name, sizeof(name), "rgn-%lu", index);
        return ael_packet_create_region(event, rank, 0, (int64_t) index,
                                        AEL_SAMPLE_REGION_SIZE, name);
    default:
        return NULL;
    }
}

static void *thread_func(void *arg) {
    thread_params_t *params = (thread_params_t *) arg;
    const test_params_t *tp = params->tp;  // Dereference config pointer
    
    // Create AEL instance with rank from params
    ael_hndl *pHndl = ael_create_instance(params->rank, 0, 1, 0, 1);
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

    // Send packets to server
    log_info("Thread %" PRId64 " sending %" PRIu64 " packet(s) to %s:%u",
             params->rank, tp->packet_count, tp->server_ip, tp->server_port);
    
    for (unsigned long i = 0; i < tp->packet_count && pHndl; i++) {
        eAEL_event event = ael_event_code(i);
        ael_packet_t *packet = _ael_packet_create(event, params->rank, i);
        if (!packet) {
            log_error("Failed to allocate packet for rank %ld", params->rank);
            break;
        }

        eAEL_error tx_status = ael_send_packet(pHndl, packet);
        if (tx_status != eAEL_OK) {
            log_error("Failed to send packet from rank %ld: eAEL_error=%d",
                      params->rank, (int) tx_status);
            ael_packet_destroy(&packet);
            break;
        }

        log_info("tx rank=%" PRIu64 " ev=%s (%zu bytes)",
                 packet->rank, ael_event_name(packet->event), sizeof(*packet));
        ael_packet_destroy(&packet);
    }

    // Log completion if we sent any packets
    if (pHndl) {
        log_info("Thread completed: sent %" PRIu64 " packet(s) from rank %ld",
                 tp->packet_count, params->rank);  // Total packets sent by this thread
        ael_destroy_instance(&pHndl);
    }
    
    return NULL;
}

int main(int argc, char **argv) {
    // Parse all arguments into consolidated params struct
    test_params_t *tp = ael_parse_params(argc, argv);
    if (!tp) {
        return 1;
    }

    pthread_t *threads = malloc(sizeof(pthread_t) * tp->num_threads);
    thread_params_t *thread_params = malloc(sizeof(thread_params_t) * tp->num_threads);
    
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
