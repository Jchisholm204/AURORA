The AURORA External Logger (AEL) is a UDP event bus for external data gathering tools during testing.
- The [ARE](../Server%20Docs/Running%20ARE.md) emits one event when each connect, checkpoint, restart or disconnect finishes.
- The relay server `aurora_external_logger` receives every event on a single UDP endpoint. Any number of ARE instances can send to it.
- External tools subscribe to the relay and get every event forwarded to them. The relay uses one port for both receiving and sending.
- Requires:
	- Nothing beyond the common lib (no UCX/MPI)

```
ARE (node 0) ─┐                              ┌─► tool A
ARE (node 1) ─┼─► aurora_external_logger:9064├─► tool B
ARE (node N) ─┘      (single UDP socket)     └─► ...
```

## Running the Relay
```sh
aurora_external_logger <opt_port> <opt_event_log_file>
```
1. Optional: UDP port. Default `9064`
2. Optional: File to append the decoded event lines to

The relay also prints each event to stdout as one line:
```
<timestamp_ns> <TYPE> <OK|FAIL> rank=<r> group=<id>/<size> version=<v> name=<name> duration_ms=<ms>
```

## Enabling the ARE Emitter
The ARE emits events only when this environment variable is set:

| Variable | Default | Description |
| --- | --- | --- |
| `AURORA_AEL_HOST` | *(unset = disabled)* | Hostname/IP of the relay |
| `AURORA_AEL_PORT` | `9064` | UDP port of the relay |

```sh
AURORA_AEL_HOST=10.0.0.5 aurora_remote_engine are.log
```

## Packet Format
Every event is a fixed **72 byte**, packed, **little-endian** datagram.

| Offset | Type | Field | Notes |
| --- | --- | --- | --- |
| 0 | `char[4]` | magic | `"AEL1"` |
| 4 | `uint8` | type | see below |
| 5 | `uint8` | status | `0` = OK, `1` = FAIL |
| 6 | `uint16` | reserved | `0` |
| 8 | `uint64` | timestamp_ns | `CLOCK_REALTIME` on the ARE when the command finished |
| 16 | `uint64` | rank | |
| 24 | `uint64` | group_id | |
| 32 | `int64` | group_size | |
| 40 | `int64` | version | checkpoint version, `-1` for connect/disconnect |
| 48 | `double` | duration_ms | command runtime (`0` for disconnect) |
| 56 | `char[16]` | name | checkpoint name, **not** NUL terminated if 16 chars |

| Type | Value | Direction |
| --- | --- | --- |
| CONNECT | `0x01` | ARE → relay → subscribers |
| CHECKPOINT | `0x02` | ARE → relay → subscribers |
| RESTART | `0x03` | ARE → relay → subscribers |
| DISCONNECT | `0x04` | ARE → relay → subscribers |
| SUBSCRIBE | `0x80` | tool → relay |
| UNSUBSCRIBE | `0x81` | tool → relay |

Python decode:
```python
magic, type_, status, _, ts, rank, gid, gsize, version, dur, name = \
    struct.unpack("<4sBBHQQQqqd16s", data)
```

## Subscribing
- Send the 5 byte control packet `b"AEL1" + bytes([0x80])` to the relay port from the socket you want events on.
- Events are then sent to that socket's address/port.
- Send `b"AEL1" + bytes([0x81])` from the same socket to unsubscribe.
- A reference subscriber is available at [`scripts/ael/ael_listen.py`](../../scripts/ael/ael_listen.py):
```sh
python3 scripts/ael/ael_listen.py --host <relay> --port 9064 [--csv] [--count N]
```

## Emitter API
```c
ael_hndl *ael_init(const char *host, uint16_t port);
ael_hndl *ael_init_from_env(void); // NULL if AURORA_AEL_HOST is unset
int ael_emit(ael_hndl *pHndl, ael_packet_t *pPacket); // NULL handle = no-op
int ael_finalize(ael_hndl **ppHndl);
```
- `ael_emit` fills in the magic and timestamp. A single handle can safely be shared between threads.

## Notes
- Delivery is best effort (UDP). Events can be dropped under load or across lossy links.
- Subscribers do not expire. A tool that exits without unsubscribing stays in the list, and the relay keeps sending to it (the sends are harmless).
- CONNECT is only emitted after a connection succeeds. Connections that fail before the rank is known produce no event.
