/**
 * @file ael_packet_crc.c
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author DeepSeek-V4.1-Flash OpenCode Free
 * @author qwen 3.5 9b
 * @brief CRC helpers for AEL packets.
 * @version 0.1
 * @date Created: 2026-10-26
 * @modified Last Modified: 2026-10-27
 *
 * @copyright Copyright (c) 2026
 */

#define AEL_INTERNAL
#include "ael/ael_packet.h"

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

// CRC-32/ISO-HDLC. The CRC field itself is skipped, so the covered ranges
// are [0, offsetof(crc)) and [offsetof(payload), sizeof(ael_packet_t)).
static uint32_t _ael_crc32_update(uint32_t crc, const void *data, size_t len) {
    assert(data != NULL);

    const uint8_t *bytes = data;

    while (len > 0) {
        crc ^= *bytes++;
        for (unsigned bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
        len--;
    }

    return crc;
}

uint32_t ael_packet_crc(const ael_packet_t *pPacket) {
    assert(pPacket != NULL);

    uint32_t crc = 0xFFFFFFFFu;
    crc = _ael_crc32_update(crc, (const uint8_t *) pPacket,
                            offsetof(ael_packet_t, crc));
    crc = _ael_crc32_update(
        crc, (const uint8_t *) pPacket + offsetof(ael_packet_t, payload),
        sizeof(ael_packet_t) - offsetof(ael_packet_t, payload));

    return crc ^ 0xFFFFFFFFu;
}

bool ael_packet_verify(const ael_packet_t *pPacket) {
    return pPacket && pPacket->crc == ael_packet_crc(pPacket);
}
