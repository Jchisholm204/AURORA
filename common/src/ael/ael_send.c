/**
 * @file ael_send.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
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
#include <stdio.h>
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

eAEL_error ael_send_cli(ael_hndl *pHndl, eAEL_event id,
                        const char *event_message) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (id != eAEL_EVENT_CLI_CONNECT && id != eAEL_EVENT_CLI_FAILURE &&
        id != eAEL_EVENT_CLI_DISCONNECT) {
        log_error("AEL send: invalid client event %d", (int) id);
        return eAEL_ERR_EVENT;
    }

    ael_packet_t *packet =
        ael_packet_create_cli(id, pHndl->rank, pHndl->group_id, event_message);
    if (!packet) {
        return eAEL_ERR_MEM;
    }

    eAEL_error status = ael_send_packet(pHndl, packet);
    ael_packet_destroy(&packet);
    return status;
}

eAEL_error ael_send_checkpoint(ael_hndl *pHndl, eAEL_event id,
                               int64_t version, size_t n_regions,
                               const char *name, const char *event_message) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (id != eAEL_EVENT_CHECKPOINT_START && id != eAEL_EVENT_CHECKPOINT_END &&
        id != eAEL_EVENT_RESTORE_START && id != eAEL_EVENT_RESTORE_END) {
        log_error("AEL send: invalid checkpoint event %d", (int) id);
        return eAEL_ERR_EVENT;
    }

    ael_packet_t *packet = ael_packet_create_checkpoint(
        id, pHndl->rank, pHndl->group_id, version, n_regions, name);
    if (!packet) {
        return eAEL_ERR_MEM;
    }

    if (event_message) {
        snprintf(packet->event_message, sizeof(packet->event_message), "%s",
                 event_message);
        ael_packet_seal(packet);
    }

    eAEL_error status = ael_send_packet(pHndl, packet);
    ael_packet_destroy(&packet);
    return status;
}

eAEL_error ael_send_region(ael_hndl *pHndl, eAEL_event id, int64_t region_id,
                           size_t size, const char *name,
                           const char *event_message) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL;
    }
    if (id != eAEL_EVENT_REGION_ADD && id != eAEL_EVENT_REGION_REMOVE) {
        log_error("AEL send: invalid region event %d", (int) id);
        return eAEL_ERR_EVENT;
    }

    ael_packet_t *packet = ael_packet_create_region(
        id, pHndl->rank, pHndl->group_id, region_id, size, name);
    if (!packet) {
        return eAEL_ERR_MEM;
    }

    if (event_message) {
        snprintf(packet->event_message, sizeof(packet->event_message), "%s",
                 event_message);
        ael_packet_seal(packet);
    }

    eAEL_error status = ael_send_packet(pHndl, packet);
    ael_packet_destroy(&packet);
    return status;
}
