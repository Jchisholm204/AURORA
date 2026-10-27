/**
 * @file ael.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief AURORA External Logger interface
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-26
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

enum aurora_external_logger_error_e {
    eAEL_OK = 0,
    eAEL_ERR_NULL,
    eAEL_ERR_INTR,
    eAEL_ERR_TIMEOUT,
    eAEL_ERR_SOCKET,
    eAEL_ERR_SEND,
    eAEL_ERR_RECV,
    eAEL_ERR_SIZE,
    eAEL_ERR_EVENT,
    eAEL_ERR_CRC,
    eAEL_N_ERR,
};

struct aurora_external_logger_hndl
#ifdef AEL_INTERNAL
{
    uint64_t rank;
    uint64_t group_id;
    uint64_t group_size;

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

struct aurora_external_logger_peer {
    const char *ip;
    uint16_t port;
};

typedef enum aurora_external_logger_error_e eAEL_error;
typedef struct aurora_external_logger_hndl ael_hndl;
typedef struct aurora_external_logger_peer ael_peer_t;

/**
 * @brief Allocate an AEL handle, open its UDP socket, and bind it locally.
 *
 * The socket is bound to INADDR_ANY on local_port and is left unconnected so
 * that ael_recv_packet() accepts datagrams from any sender. The send_peers
 * array and its IP strings are only read during this call.
 *
 * @param rank Rank of the local process
 * @param group_id Group identifier
 * @param group_size Number of processes in the group
 * @param local_port Local UDP port in host byte order
 * @param send_peers Peers that receive every sent packet
 * @param n_send_peers Number of configured send peers
 * @return Handle, or NULL on allocation or socket setup failure
 */
extern ael_hndl *ael_create_instance(uint64_t rank, uint64_t group_id,
                                     uint64_t group_size, uint16_t local_port,
                                     const ael_peer_t *send_peers,
                                     size_t n_send_peers);

/**
 * @brief Close the socket and free the handle.
 *
 * @param ppHndl Handle pointer to destroy; set to NULL on success
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_destroy_instance(ael_hndl **ppHndl);

/*
 * Packet transport
 */

/**
 * @brief Send one sealed packet to every configured send peer.
 *
 * All configured peers are attempted before an error is returned.
 *
 * @param pHndl AEL handle
 * @param packet Packet to send
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_send_packet(ael_hndl *pHndl, const ael_packet_t *packet);

/**
 * @brief Receive one valid packet from any sender.
 *
 * Validates the datagram size, CRC, and event code.
 *
 * @param pHndl AEL handle
 * @param packet Output packet
 * @param timeout_ms Receive timeout in milliseconds:
 *                   negative blocks indefinitely,
 *                   zero polls without blocking,
 *                   positive waits up to the given time
 * @return eAEL_OK, or an eAEL_error code
 */
extern eAEL_error ael_recv_packet(ael_hndl *pHndl, ael_packet_t *packet,
                                  int timeout_ms);

#endif
