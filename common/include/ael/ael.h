/**
 * @file ael.h
 * @author Connor Leung
 * @brief AURORA External Logger
 * @version 0.1
 * @date Created: 2026-11-02
 * @modified Last Modified: 2026-11-02
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _AEL_AEL_H_
#define _AEL_AEL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AEL_DEFAULT_PORT 9064
#define AEL_NAME_LEN 16
#define AEL_MAGIC "AEL1"
#define AEL_MAGIC_LEN 4
// Minimum size of a control packet (magic + type)
#define AEL_CTRL_SIZE (AEL_MAGIC_LEN + 1)

#define AEL_ENV_HOST "AURORA_AEL_HOST"
#define AEL_ENV_PORT "AURORA_AEL_PORT"

struct aurora_external_logger_hndl;
typedef struct aurora_external_logger_hndl ael_hndl;
typedef struct aurora_external_logger_packet ael_packet_t;

enum eAEL_type {
    // Events (ARE -> relay -> subscribers)
    eAEL_CONNECT = 0x01,
    eAEL_CHECKPOINT = 0x02,
    eAEL_RESTART = 0x03,
    eAEL_DISCONNECT = 0x04,

    // Control (subscriber -> relay)
    eAEL_SUBSCRIBE = 0x80,
    eAEL_UNSUBSCRIBE = 0x81,
};

enum eAEL_status {
    eAEL_STATUS_OK = 0,
    eAEL_STATUS_FAIL = 1,
};

/**
 * Wire format (little-endian, packed, 72 bytes)
 * Python: struct.unpack("<4sBBHQQQqqd16s", data)
 */
struct __attribute__((packed)) aurora_external_logger_packet {
    char magic[AEL_MAGIC_LEN];
    uint8_t type;
    uint8_t status;
    uint16_t _reserved;
    uint64_t timestamp_ns;
    uint64_t rank;
    uint64_t group_id;
    int64_t group_size;
    int64_t version;
    double duration_ms;
    char name[AEL_NAME_LEN];
};

_Static_assert(sizeof(ael_packet_t) == 72, "AEL packet must be 72 bytes");

/**
 * @brief Create an AEL emitter targeting host:port
 *
 * @param host Hostname or IP of the AEL relay
 * @param port UDP port of the AEL relay
 * @return AEL handle, or NULL on failure
 */
extern ael_hndl *ael_init(const char *host, uint16_t port);

/**
 * @brief Create an AEL emitter from AURORA_AEL_HOST / AURORA_AEL_PORT
 *
 * @return AEL handle, or NULL if disabled (host unset) or on failure
 */
extern ael_hndl *ael_init_from_env(void);

/**
 * @brief Destroy an AEL emitter
 *
 * @param ppHndl Pointer to the AEL handle
 * @return 0 on success
 */
extern int ael_finalize(ael_hndl **ppHndl);

/**
 * @brief Send an event to the AEL relay
 * Fills in the magic and timestamp. Safe to call from multiple threads.
 *
 * @param pHndl AEL handle (NULL is a no-op)
 * @param pPacket Event to send
 * @return 0 on success (or disabled), -1 on failure
 */
extern int ael_emit(ael_hndl *pHndl, ael_packet_t *pPacket);

/**
 * @brief Check that a received buffer is a well formed AEL packet
 *
 * @param pData Received data
 * @param size Size of received data
 * @return true if valid
 */
extern bool ael_packet_valid(const void *pData, size_t size);

/**
 * @brief Get a printable name for an AEL packet type
 */
extern const char *ael_type_str(uint8_t type);

#endif
