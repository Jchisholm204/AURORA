/**
 * @file ael_send.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash
 * @brief AURORA External Logger packet send path.
 * @version 0.1
 * @date Created: 2026-10-27
 * @modified Last Modified: 2026-10-27
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael.h"

#include "log.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

eAEL_error ael_send_packet(ael_hndl *pHndl, const ael_packet_t *packet) {
    if (!pHndl || !packet) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (pHndl->socket.fd < 0) {
        log_error("AEL send: closed socket");
        return eAEL_ERR_SOCKET;
    }
    if (pHndl->n_send_peers == 0 || !pHndl->send_peers) {
        log_error("AEL send: no send peers configured");
        return eAEL_ERR_SEND;
    }

    eAEL_error status = eAEL_OK;
    for (size_t i = 0; i < pHndl->n_send_peers; i++) {
        const struct sockaddr_in *peer = &pHndl->send_peers[i];
        ssize_t tx_bytes =
            sendto(pHndl->socket.fd, packet, sizeof(*packet), 0,
                   (const struct sockaddr *) peer, sizeof(*peer));

        if (tx_bytes < 0) {
            log_error("AEL sendto peer %zu failed: %s", i, strerror(errno));
            if (status == eAEL_OK) {
                status = eAEL_ERR_SEND;
            }
            continue;
        }
        if ((size_t) tx_bytes != sizeof(*packet)) {
            log_error("AEL short send to peer %zu: %zd of %zu bytes", i,
                      tx_bytes, sizeof(*packet));
            if (status == eAEL_OK) {
                status = eAEL_ERR_SIZE;
            }
        }
    }

    return status;
}
