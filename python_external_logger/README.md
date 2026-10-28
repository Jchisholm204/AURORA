# Python External Logger (AEL) - Receive-Only Interface

## Overview

This is a receive-only client interface for AURORA's Asynchronous External Logger (AEL), implemented in pure Python. It mirrors the C receiver (`are_recv.c` / `test_ael_server.c`) logic and provides:

- Event decoding from UDP packets (rank, group_id, timestamp, event type)
- CRC32 validation using zlib
- Simple CLI sender for testing integration with C clients

## Components

### Core Library (`ael.py`)
**Single source of truth** for all AEL packet logic:
- `AelEvent` enum (mirrors C `eAEL_event`)
- `decode(data, addr)` - decodes 128-byte packets with CRC validation
- `AelMessage` dataclass representing decoded events
- `packet_to_bytes(...)` - constructs valid packets for sending

### Receiver (`ael_receiver.py`)
UDP receiver that:
- Binds to configurable port (default: 8334)
- Receives and decodes AEL packets
- Logs to stderr matching C `test_ael_server.c` format exactly
- Supports graceful shutdown via SIGINT/SIGTERM

### Simple Sender (`ael_sender_simple.py`)
CLI tool to send synthetic packets for testing.

## Usage

### Start Receiver
```bash
python3 ael_receiver.py --port 8334
# Or in daemon mode (Unix/Linux):
python3 ael_receiver.py --port 8334 --daemon
```

### Send Test Packets
```bash
# Using simple sender:
python3 ael_sender_simple.py --rank 1 --event CHECKPOINT_END

# All event types available:
CLI_CONNECT, CLI_FAILURE, CLI_DISCONNECT
CHECKPOINT_START, CHECKPOINT_END
RESTORE_START, RESTORE_END
REGION_ADD, REGION_REMOVE
```

### Python API Usage
```python
from ael import decode, packet_to_bytes, AelEvent

# Decode incoming packet:
msg = decode(data, addr)  # data is 128 bytes, addr is (host, port)

# Send synthetic packet:
packet = packet_to_bytes(
    event_code=AelEvent.CHECKPOINT_END.value,
    rank=3,
    timestamp_ns=1700000000000000000,
)
```

## Integration with C Clients

This implementation is designed to interop with:
- `tests/ael/test_ael_server.c` - C server receiver
- `tests/ael/multi_client.c` - Multi-threaded C client sender

The Python receiver will correctly decode packets from both sources.

## Packet Format

AEL packets are 128 bytes total:

```
[0:8]   rank        uint64_t (little-endian)
[8:16]  group_id    uint64_t (little-endian)  
[16:24] timestamp   uint64_t (nanoseconds since epoch)
[24:28] event       int32_t (event type code)
[28:32] crc         uint32_t (CRC32 checksum - computed over all other bytes)
[32:40] id_version  int64_t (signed, version ID)
[40:48] size        uint64_t (name field length)
[48:64] name        char[16] (null-terminated string)
[64:128] msg        char[64] (null-terminated message)
```

CRC is computed over bytes 0-28 + 32-128 (excluding the CRC field itself).

## Event Types

| Code | Name           | Description              |
|------|----------------|--------------------------|
| 0    | CLI_CONNECT    | Client connected         |
| 1    | CLI_FAILURE    | Client failed to connect |
| 2    | CLI_DISCONNECT | Client disconnected      |
| 3    | CHECKPOINT_START | Checkpoint started   |
| 4    | CHECKPOINT_END     | Checkpoint ended     |
| 5    | RESTORE_START  | Region restore started   |
| 6    | RESTORE_END    | Region restore ended     |
| 7    | REGION_ADD     | Region added             |
| 8    | REGION_REMOVE  | Region removed           |

## Testing

Run integration tests:
```bash
cd /Documents/AURORA
PYTHONPATH=/Documents/AURORA python3 \
  python_external_logger/test_integration.py
```

All tests verify:
- Packet format matches C wire format
- All 9 event types decode correctly  
- CRC validation (valid packets accepted, invalid rejected)
- multi_client.c event patterns work
- Output format matches C code

## Files

| File                          | Purpose                              |
|-------------------------------|--------------------------------------|
| `ael.py`                      | Core packet parsing library          |
| `ael_receiver.py`             | UDP receiver implementation          |
| `ael_sender_simple.py`        | CLI sender for testing               |
| `test_integration.py`         | Integration tests vs C clients       |
| `__init__.py`                 | Package marker (minimal content)     |

## Notes

- This is **receive-only** - no AEL send-side implementation
- Designed to mirror C `are_recv.c` / `test_ael_server.c` behavior exactly
- Uses Python's asyncio for async socket handling
- Signal handlers support graceful shutdown
