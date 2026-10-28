/**
 * @file ael_send.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @brief AURORA External Logger packet send path.
 * @version 0.1
 * @date Created: 2026-10-27
 * @modified Last Modified: 2026-10-28
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael.h"

#include "log.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

static eAEL_error _ael_send(ael_hndl *pHndl, ael_packet_t *packet) {
    assert(pHndl != NULL);
    assert(packet != NULL);

    if (pHndl->socket.fd < 0) {
        log_error("AEL send: closed socket");
        return eAEL_ERR_SOCKET | eAEL_ERR_FATAL;
    }
    if (pHndl->n_send_peers == 0 || !pHndl->send_peers) {
        log_error("AEL send: no send peers configured");
        return eAEL_ERR_SEND | eAEL_ERR_FATAL;
    }

    ael_packet_finalize(packet);

    eAEL_error status = eAEL_OK;
    for (size_t i = 0; i < pHndl->n_send_peers; i++) {
        const struct sockaddr_in *peer = &pHndl->send_peers[i];
        ssize_t tx_bytes =
            sendto(pHndl->socket.fd, packet, sizeof(ael_packet_t), 0,
                   (const struct sockaddr *) peer, sizeof(struct sockaddr_in));

        if (tx_bytes < 0) {
            log_error("AEL sendto peer %zu failed: %s", i, strerror(errno));
            if (status == eAEL_OK) {
                status = eAEL_ERR_SEND;
            }
            continue;
        }
        if ((size_t) tx_bytes != sizeof(ael_packet_t)) {
            log_error("AEL short send to peer %zu: %zd of %zu bytes", i,
                      tx_bytes, sizeof(ael_packet_t));
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
        return eAEL_ERR_NULL | eAEL_ERR_FATAL;
    }
    if (id != eAEL_EVENT_CLI_CONNECT && id != eAEL_EVENT_CLI_FAILURE &&
        id != eAEL_EVENT_CLI_DISCONNECT) {
        log_error("AEL send: invalid client event %d", (int) id);
        return eAEL_ERR_EVENT | eAEL_ERR_FATAL;
    }

    ael_packet_t packet;
    _ael_packet_init(&packet, id, pHndl->rank, pHndl->group_id,
                     event_message);

    return _ael_send(pHndl, &packet);
}

eAEL_error ael_send_checkpoint(ael_hndl *pHndl, eAEL_event id,
                               int64_t version, uint64_t n_regions,
                               const char *name, const char *event_message) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL | eAEL_ERR_FATAL;
    }
    if (id != eAEL_EVENT_CHECKPOINT_START && id != eAEL_EVENT_CHECKPOINT_END &&
        id != eAEL_EVENT_RESTORE_START && id != eAEL_EVENT_RESTORE_END) {
        log_error("AEL send: invalid checkpoint event %d", (int) id);
        return eAEL_ERR_EVENT | eAEL_ERR_FATAL;
    }

    ael_packet_t packet;
    _ael_packet_init(&packet, id, pHndl->rank, pHndl->group_id,
                     event_message);
    _ael_packet_set_payload(&packet.payload, version, n_regions, name);

    return _ael_send(pHndl, &packet);
}

eAEL_error ael_send_region(ael_hndl *pHndl, eAEL_event id, int64_t region_id,
                           uint64_t size, const char *name,
                           const char *event_message) {
    if (!pHndl) {
        log_error("NULL Parameter");
        return eAEL_ERR_NULL | eAEL_ERR_FATAL;
    }
    if (id != eAEL_EVENT_REGION_ADD && id != eAEL_EVENT_REGION_REMOVE) {
        log_error("AEL send: invalid region event %d", (int) id);
        return eAEL_ERR_EVENT | eAEL_ERR_FATAL;
    }

    ael_packet_t packet;
    _ael_packet_init(&packet, id, pHndl->rank, pHndl->group_id,
                     event_message);
    _ael_packet_set_payload(&packet.payload, region_id, size, name);

    return _ael_send(pHndl, &packet);
}
