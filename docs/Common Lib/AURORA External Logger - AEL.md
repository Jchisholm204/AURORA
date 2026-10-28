# AEL
The AURORA External Logger is a UDP event channel used to report client
lifecycle, checkpoint/restore, and region changes to another process or host.
- Fire-and-forget: each event is one fixed-size datagram. No ACK, retry, or ordering.
- Not a replacement for `log.c`. It does not redirect local log output; it carries structured events.
- Independent of UCX/RDMA, ACI, ADS, and ACN. Only POSIX UDP sockets are required.
- One handle can both send and receive; the two roles are configured per handle.

## Events
| Group | Event codes | Payload meaning |
|---|---|---|
| Client | `eAEL_EVENT_CLI_CONNECT`, `_FAILURE`, `_DISCONNECT` | unused (zeroed) |
| Checkpoint | `eAEL_EVENT_CHECKPOINT_START/END`, `eAEL_EVENT_RESTORE_START/END` | `id_version` = version, `size` = region count, `name` = checkpoint name |
| Region | `eAEL_EVENT_REGION_ADD`, `eAEL_EVENT_REGION_REMOVE` | `id_version` = region ID, `size` = bytes, `name` = region name |

Every packet also carries the sender's rank, group ID, a `CLOCK_REALTIME`
timestamp in nanoseconds, and an optional event message.
Strings are fixed size and truncated, never rejected:
- `AEL_NAME_LEN` = 16 (15 characters + NUL)
- `AEL_EVENT_MSG_LEN` = 64 (63 characters + NUL)

## Initialization
### Instance Creation
Allocates the handle, opens a UDP socket, sets `SO_REUSEADDR` (best effort), and
binds to `INADDR_ANY:local_port`.

```c
ael_hndl *ael_create_instance(uint64_t rank, uint64_t group_id,
                              uint16_t local_port, size_t max_recipients);
```

- `local_port` is in host byte order.
- `local_port = 0` requests an ephemeral port. The assigned port is not exposed
  by the API, so this is only useful for send-only handles.
- `max_recipients` fixes the size of the outbound peer table. Use `0` for a
  receive-only handle.
- `rank` and `group_id` are copied into every outgoing packet.
- Returns `NULL` on allocation, socket, or bind failure.

### Deconstruction
```c
void ael_destroy_instance(ael_hndl **ppHndl);
```
Closes the socket, frees the peer table and handle, and sets the caller's pointer
to `NULL`. Safe to call with a `NULL` handle. Pending datagrams are not flushed.

## Sending
### Add Recipient
```c
eAEL_error ael_add_recipient(ael_hndl *pHndl, const char *ip, uint16_t port);
```
- `ip` is an IPv4 dotted-decimal string; `port` is in host byte order and must be non-zero.
- Appends to the peer table; bounded by `max_recipients`. Duplicates are not checked.
- Returns `eAEL_ERR_NULL`, `eAEL_ERR_IP`, or `eAEL_ERR_COUNT` on failure.

### Send Event
```c
eAEL_error ael_send_cli(ael_hndl *pHndl, eAEL_event id,
                        const char *event_message);
eAEL_error ael_send_checkpoint(ael_hndl *pHndl, eAEL_event id, int64_t version,
                               uint64_t n_regions, const char *name,
                               const char *event_message);
eAEL_error ael_send_region(ael_hndl *pHndl, eAEL_event id, int64_t region_id,
                           uint64_t size, const char *name,
                           const char *event_message);
```
- Each helper rejects event codes that do not belong to its group.
- The packet is built, timestamped, CRC-finalized, and sent to every recipient.
- `name` and `event_message` are optional; `NULL` leaves the field zeroed.
- Returns `eAEL_OK` only if every recipient accepted all 128 bytes. Otherwise it
  returns the first failure, but still attempts the remaining peers, so partial
  delivery is possible. No per-peer result is reported.

## Receiving
```c
eAEL_error ael_recv_message(ael_hndl *pHndl, ael_message_t *pMsg,
                            int timeout_ms);
```
- Polls the socket: negative blocks, `0` polls without blocking, positive waits
  up to that many milliseconds.
- Validation order is datagram size, then CRC, then event range. Rejected
  datagrams return `eAEL_ERR_SIZE`, `eAEL_ERR_CRC`, or `eAEL_ERR_EVENT` and leave
  `*pMsg` untouched.
- On success the message is filled with the sender IP and port in host byte
  order, and both string fields are force-terminated.
- `eAEL_ERR_TIMEOUT` and `eAEL_ERR_INTR` are normal loop conditions. Socket
  errors are fatal.
- The CRC protects integrity only: any host that can reach the port can send a
  valid packet, so all fields are untrusted input.

## Wire Format
One 128-byte datagram, sent as a raw struct (no serialization step):

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | `rank` |
| 8 | 8 | `group_id` |
| 16 | 8 | `timestamp` (ns since epoch) |
| 24 | 4 | `event` |
| 28 | 4 | CRC-32 (or reserved while being built) |
| 32 | 32 | `payload` (`id_version`, `size`, `name[16]`) |
| 64 | 64 | `event_message` |

- CRC-32/ISO-HDLC over every byte except the CRC field itself.
- The layout is native-endian and has no protocol version field. Sender and
  receiver must share endianness and struct layout; `_Static_assert` enforces the
  layout at build time.
- The packet struct is private to the implementation (`AEL_INTERNAL`); the
  public surface is `ael_message_t`.

## Internal Interface
Everything below is hidden unless `AEL_INTERNAL` is defined before the first
include of `ael/ael.h`, for example:

```c
#define AEL_INTERNAL
#include "ael/ael.h"
```

Defining it exposes the handle body, the packet struct, and the packet helpers,
and pulls in `<arpa/inet.h>`/`<netinet/in.h>` for `struct sockaddr_in`. It must
never be defined by a consumer that only needs the event API.

### Handle Layout
```c
struct aurora_external_logger_hndl {
    uint64_t rank;
    uint64_t group_id;

    size_t n_send_peers;     // peer table capacity
    size_t recipient_count;  // peers added so far
    struct sockaddr_in *send_peers;

    struct aurora_external_logger_socket {
        int fd;
        uint16_t port;       // requested local port, not always the bound port
    } socket;
};
```
- The handle and the peer table are separate allocations; the table only exists
  when `max_recipients > 0`.
- `fd` is `-1` before setup and after `ael_destroy_instance`.
- There is no background thread and no lock. All sending and receiving happens
  on the caller's thread.

### Packet Construction
The normal order is `_ael_packet_init`, then `_ael_packet_set_payload` when the
event has one, then `ael_packet_finalize`, then send.

#### Packet Init
```c
void _ael_packet_init(ael_packet_t *pPacket, eAEL_event id, uint64_t rank,
                      uint64_t group_id, const char *event_message);
```
- Zeroes the whole packet, then fills rank, group, event, and timestamp.
- Timestamp is `CLOCK_REALTIME` in nanoseconds, or `0` if `clock_gettime` fails.
- Copies `event_message` with `snprintf` when it is not `NULL`.
- Does not set the payload; it stays zeroed unless set separately.
- Asserts that the packet is non-null and `id < eAEL_N_EVENT`.

#### Set Payload
```c
void _ael_packet_set_payload(ael_payload_t *pPayload, int64_t id_version,
                             uint64_t size, const char *name);
```
- Writes `id_version`, `size`, and optionally `name`.
- A `NULL` name leaves the existing name bytes untouched, so initialize the
  packet first.
- Asserts that the payload pointer is non-null.

#### Finalize
```c
void ael_packet_finalize(ael_packet_t *pPacket);
```
Computes the CRC and stores it in the packet. Silently does nothing when the
pointer is `NULL`. `_ael_send` calls this itself, so the public send helpers do
not need to.

#### Calculate CRC
```c
uint32_t ael_packet_crc(const ael_packet_t *pPacket);
```
Reflected CRC-32/ISO-HDLC (polynomial `0xEDB88320`, init/final `0xFFFFFFFF`).
It covers `[0, offsetof(crc))` and `[offsetof(payload), sizeof(ael_packet_t))`,
so the CRC field itself is skipped. Asserts a non-null packet.

#### Verify CRC
```c
bool ael_packet_verify(const ael_packet_t *pPacket);
```
Recomputes the CRC and compares it with the stored field. Returns `false` for a
`NULL` packet. This checks integrity only; it says nothing about the sender.

### Send Path
```c
static eAEL_error _ael_send(ael_hndl *pHndl, ael_packet_t *pPacket);
```
Private to `ael_send.c`; the public send helpers call it after building the
packet.
- Asserts that both pointers are non-null.
- Fails with `eAEL_ERR_SOCKET | eAEL_ERR_FATAL` when the socket is closed.
- Fails with `eAEL_ERR_SEND | eAEL_ERR_FATAL` when no recipients are configured.
- Finalizes the CRC once, then calls `sendto` for each peer.
- Keeps going after a failed peer and reports the first failure, so the result
  describes the batch, not individual peers.

### Receive Path
```c
static void _ael_decode(const ael_packet_t *pPacket,
                        const struct sockaddr_in *pFrom, ael_message_t *pMsg);
```
Private to `ael_recv.c`; only called after size, CRC, and event validation have
passed.
- Asserts all three pointers are non-null.
- Zeroes the output message, then copies rank, group, timestamp, event, payload,
  and event message.
- Converts the source address to host byte order in `from_ip` and `from_port`.
- Force-terminates `payload.name` and `event_message`, because the sender may not
  have done so.

### Layout Invariants
`ael_packet.h` asserts the wire contract at compile time:
- `sizeof(eAEL_event) == 4`
- `sizeof(ael_payload_t) == 32`
- CRC at offset 28, payload at 32, event message at 64
- `sizeof(ael_packet_t) == 128`

Changing any of these breaks compatibility with every other AEL peer, even
though there is no version field to detect it.

## Errors
`eAEL_error` is a bit mask. Test with `&`, and compare success with `== eAEL_OK`.

| Codes | Meaning |
|---|---|
| `eAEL_ERR_INTR`, `_TIMEOUT`, `_SIZE`, `_CRC`, `_EVENT`, `_SEND` | Transient. Retry or discard the datagram and continue. |
| `eAEL_ERR_NULL`, `_COUNT`, `_IP` | Caller error. The handle remains valid. |
| `eAEL_ERR_SOCKET`, `_RECV` | Handle error. The socket is unusable. |
| `eAEL_ERR_FATAL` | Bit set when repeating the same call cannot succeed. |

## Usage
```c
/* Listener: bind port 8334, no outbound peers */
ael_hndl *rx = ael_create_instance(0, 0, 8334, 0);
ael_message_t msg;
for (;;) {
    eAEL_error st = ael_recv_message(rx, &msg, -1);
    if (st == eAEL_OK) {
        /* consume msg */
    } else if (st & eAEL_ERR_FATAL) {
        break;
    }
}

/* Sender: ephemeral local port, one recipient */
ael_hndl *tx = ael_create_instance(rank, group_id, 0, 1);
ael_add_recipient(tx, "10.0.0.42", 8334);
ael_send_checkpoint(tx, eAEL_EVENT_CHECKPOINT_START, version, n_regions,
                    "heatdis", NULL);
```

Reference programs live under `tests/ael/`: `test_ael_server` (listener on port
8334), `test_ael_client`, `multi_client`, and `test_ael_recv_bad` (malformed and
hostile datagram rejection).

## Limits
- UDP semantics: packets may be lost, duplicated, or reordered. There is no
  delivery guarantee.
- No authentication or encryption. Any sender with a valid CRC is accepted.
- No cross-endian support and no protocol version negotiation.
- No internal locking. Use one handle per thread, or serialize access externally.
- Fixed-size fields; long names and messages are silently truncated.
