/**
 * @file test_ael_emit.c
 * @author Connor Leung
 * @brief Emits one of each AEL event to a relay
 * @version 0.1
 * @date Created: 2026-11-02
 * @modified Last Modified: 2026-11-02
 *
 * @copyright Copyright (c) 2026
 *
 * usage: test_ael_emit [host] [port]
 */

#include "ael/ael.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *host = argc >= 2 ? argv[1] : "127.0.0.1";
    uint16_t port = argc >= 3 ? (uint16_t) atoi(argv[2]) : AEL_DEFAULT_PORT;

    ael_hndl *pAEL = ael_init(host, port);
    if (!pAEL) {
        log_error("Failed to init AEL");
        return 1;
    }

    const uint8_t types[] = {eAEL_CONNECT, eAEL_CHECKPOINT, eAEL_RESTART,
                             eAEL_DISCONNECT};
    int failures = 0;
    for (size_t i = 0; i < sizeof(types); i++) {
        ael_packet_t packet = {0};
        packet.type = types[i];
        packet.status = eAEL_STATUS_OK;
        packet.rank = 3;
        packet.group_id = 1;
        packet.group_size = 4;
        packet.version = (types[i] == eAEL_CHECKPOINT ||
                          types[i] == eAEL_RESTART)
                             ? 200
                             : -1;
        packet.duration_ms = 1.5 * (double) (i + 1);
        memcpy(packet.name, "test_ael", 8);
        if (ael_emit(pAEL, &packet) != 0) {
            log_error("Failed to emit %s", ael_type_str(types[i]));
            failures++;
        }
        usleep(10000);
    }

    // NULL handle must be a no-op
    ael_packet_t dummy = {0};
    if (ael_emit(NULL, &dummy) != 0) {
        log_error("ael_emit(NULL) should be a no-op");
        failures++;
    }

    (void) ael_finalize(&pAEL);
    return failures ? 1 : 0;
}
