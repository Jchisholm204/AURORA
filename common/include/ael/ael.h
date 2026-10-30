/**
 * @file ael.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AURORA External Logger interface
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_AEL_H_
#define _AEL_AEL_H_

#include "ael_packet.h"

#include <stddef.h>
#include <stdint.h>

#ifdef AEL_INTERNAL
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#ifndef BIT
#define BIT(x) ((1) << (x))
#endif

enum aurora_external_logger_error_e {
    eAEL_OK = 0,

    /* Transient: retry, or discard the datagram and continue */
    eAEL_ERR_INTR = BIT(0),
    eAEL_ERR_TIMEOUT = BIT(1),
    eAEL_ERR_SIZE = BIT(2),
    eAEL_ERR_CRC = BIT(3),
    eAEL_ERR_EVENT = BIT(4),
    eAEL_ERR_SEND = BIT(5),

    /* Caller errors: fatal, the handle itself remains valid */
    eAEL_ERR_NULL = BIT(6),
    eAEL_ERR_COUNT = BIT(7),
    eAEL_ERR_IP = BIT(8),

    /* Handle errors: fatal */
    eAEL_ERR_SOCKET = BIT(9),
    eAEL_ERR_RECV = BIT(10),

    /* Retrying the same call cannot succeed */
    eAEL_ERR_FATAL = BIT(11),
};

struct aurora_external_logger_hndl
#ifdef AEL_INTERNAL
{
    uint64_t rank;
    uint64_t group_id;

    size_t n_send_peers;
    size_t recipient_count;
    struct sockaddr_in *send_peers;

    struct aurora_external_logger_socket {
        int fd;
        uint16_t port;
    } socket;
}
#endif
;

typedef enum aurora_external_logger_error_e eAEL_error;
typedef struct aurora_external_logger_hndl ael_hndl;

struct aurora_external_logger_message {
    uint64_t rank;
    uint64_t group_id;
    uint64_t timestamp_ns;
    eAEL_event event;

    uint32_t from_ip;
    uint16_t from_port;

    ael_payload_t payload;
    char event_message[AEL_EVENT_MSG_LEN];
};

typedef struct aurora_external_logger_message ael_message_t;

/**
 * @brief Allocate an AEL handle, open its UDP socket, and bind it locally.
 *
 * @param rank Rank of the local process
 * @param group_id Group identifier
 * @param local_port Local UDP port in host byte order
 * @param max_recipients Maximum number of recipients that can be added
 * @return Handle, or NULL on allocation or socket setup failure
 */
extern ael_hndl *ael_create_instance(uint64_t rank, uint64_t group_id,
                                     uint16_t local_port,
                                     size_t max_recipients);

/**
 * @brief Close the socket and free the handle.
 *
 * @param ppHndl Handle pointer to destroy; set to NULL
 */
extern void ael_destroy_instance(ael_hndl **ppHndl);

/**
 * @brief Add a recipient peer for outbound packet delivery.
 *
 * @param pHndl AEL handle
 * @param ip Recipient peer IP address (IPv4 dotted-decimal string)
 * @param port Recipient peer port in host byte order
 * @return eAEL_OK on success, or an eAEL_error code on failure
 */
extern eAEL_error ael_add_recipient(ael_hndl *pHndl, const char *ip,
                                    uint16_t port);

/**
 * @brief Send a client lifecycle event.
 *
 * @param pHndl AEL handle
 * @param id Event code; one of eAEL_EVENT_CLI_CONNECT,
 *           eAEL_EVENT_CLI_FAILURE, or eAEL_EVENT_CLI_DISCONNECT
 * @param event_message Optional message copied into the packet
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_send_cli(ael_hndl *pHndl, eAEL_event id,
                               const char *event_message);

/**
 * @brief Send a checkpoint or restore event.
 *
 * @param pHndl AEL handle
 * @param id Event code; one of eAEL_EVENT_CHECKPOINT_START,
 *           eAEL_EVENT_CHECKPOINT_END, eAEL_EVENT_RESTORE_START, or
 *           eAEL_EVENT_RESTORE_END
 * @param version Checkpoint version
 * @param n_regions Number of regions in the checkpoint
 * @param name Optional checkpoint name copied into the payload
 * @param event_message Optional message copied into the packet
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_send_checkpoint(ael_hndl *pHndl, eAEL_event id,
                                      int64_t version, uint64_t n_regions,
                                      const char *name,
                                      const char *event_message);

/**
 * @brief Send a region event.
 *
 * @param pHndl AEL handle
 * @param id Event code; one of eAEL_EVENT_REGION_ADD or
 *           eAEL_EVENT_REGION_REMOVE
 * @param region_id Region identifier
 * @param size Region size in bytes
 * @param name Optional region name copied into the payload
 * @param event_message Optional message copied into the packet
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_send_region(ael_hndl *pHndl, eAEL_event id,
                                  int64_t region_id, uint64_t size,
                                  const char *name, const char *event_message);

/**
 * @brief Receive and decode one valid AEL message from any sender.
 *
 * The datagram size, CRC, and event code are validated before the record is
 * decoded. *pMsg is only written when eAEL_OK is returned; on any other return
 * it is left untouched.
 *
 * @param pHndl AEL handle
 * @param pMsg Decoded message output
 * @param timeout_ms Receive timeout in milliseconds:
 *                   negative blocks indefinitely,
 *                   zero polls without blocking,
 *                   positive waits up to the given time
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_recv_message(ael_hndl *pHndl, ael_message_t *pMsg,
                                   int timeout_ms);

#endif
