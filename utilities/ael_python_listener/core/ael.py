#!/usr/bin/env python3
"""
@file ael.py
@brief AEL (AURORA External Logger) packet parsing library.

Provides:
  - AELPacket class for encode/decode operations
  - AelEvent enum for event type codes
  - AelMessage dataclass representing decoded events

This module is imported by both receiver and sender to ensure they
share the same packet format understanding.

@version 0.3
@author qwen 3.5 9b

AEL Packet Wire Format (128 bytes total):
  [0:8]   rank        uint64_t (little-endian)
  [8:16]  group_id    uint64_t (little-endian)
  [16:24] timestamp   uint64_t (nanoseconds since epoch)
  [24:28] event       int32_t (event type code, little-endian)
  [28:32] crc         uint32_t (CRC32 checksum, little-endian)
  [32:40] id_version  int64_t (signed, little-endian)
  [40:48] size        uint64_t (name field length)
  [48:64] name        char[16] (null-terminated string)
  [64:128] msg        char[64] (null-terminated message)

CRC is computed over bytes 0-28 + 32-128 (excluding CRC field itself).

Event types mirror eAEL_event from C code:
  0 - CLI_CONNECT
  1 - CLI_FAILURE
  2 - CLI_DISCONNECT
  3 - CHECKPOINT_START
  4 - CHECKPOINT_END
  5 - RESTORE_START
  6 - RESTORE_END
  7 - REGION_ADD
  8 - REGION_REMOVE
"""

import enum
import struct
import zlib
from dataclasses import dataclass


AEL_PACKET_SIZE = 128
AEL_NAME_LEN = 16
AEL_EVENT_MSG_LEN = 64


class AelEvent(enum.IntEnum):
    """Mirrors eAEL_event from C ael_cdefs.h."""
    CLI_CONNECT = 0
    CLI_FAILURE = 1
    CLI_DISCONNECT = 2
    CHECKPOINT_START = 3
    CHECKPOINT_END = 4
    RESTORE_START = 5
    RESTORE_END = 6
    REGION_ADD = 7
    REGION_REMOVE = 8


@dataclass(frozen=True)
class AelMessage:
    """Decoded AEL event message.
    
    Mirrors ael_message_t structure from C code for compatibility.
    """
    rank: int
    group_id: int
    timestamp_ns: int
    event: AelEvent
    from_ip: str
    from_port: int
    id_version: int
    size: int
    name: str
    event_message: str
    
    @property
    def timestamp(self):
        """Format timestamp as ISO 8601 UTC string."""
        if self.timestamp_ns == 0:
            return "N/A"
        from datetime import datetime, timezone
        ts = datetime.fromtimestamp(self.timestamp_ns / 1e9, tz=timezone.utc)
        return ts.isoformat()


class AELErrors(Exception):
    """Base class for AEL packet errors."""
    pass


class CRCError(AELErrors):
    """CRC checksum mismatch error."""
    pass


class PacketSizeError(AELErrors):
    """Invalid packet size error."""
    pass


class EventCodeError(AELErrors):
    """Event code out of valid range."""
    pass


class AELPacket:
    """AEL packet encoding/decoding layer.
    
    Handles all packet format operations including CRC validation.
    Provides a clean API for encode and decode operations.
    
    Usage:
        msg = AELPacket.decode(data, addr)
        packet = AELPacket.encode(event_code, rank=1, ...)
    """
    
    @staticmethod
    def decode(data: bytes, addr: tuple) -> AelMessage:
        """Decode a single AEL UDP packet with CRC validation.
        
        Args:
            data: Raw packet bytes (must be exactly 128 bytes).
            addr: Socket address tuple (host, port).
        
        Returns:
            Parsed AelMessage instance.
        
        Raises:
            PacketSizeError: If packet size is wrong.
            CRCError: If CRC validation fails.
            EventCodeError: If event code is out of valid range.
        """
        if len(data) != AEL_PACKET_SIZE:
            raise PacketSizeError(f"Packet size {len(data)} != {AEL_PACKET_SIZE}")
        
        # Extract and verify CRC (computed over all bytes except the CRC field)
        crc_data = data[0:28] + data[32:AEL_PACKET_SIZE]
        expected_crc = zlib.crc32(crc_data) & 0xFFFFFFFF
        actual_crc = struct.unpack('<I', data[28:32])[0]
        
        if expected_crc != actual_crc:
            raise CRCError(f"CRC mismatch at bytes 28-32: got {actual_crc:#010x}, "
                          f"expected {expected_crc:#010x}")
        
        # Unpack header (bytes 0-28): rank, group_id, timestamp_ns, event_code
        header = struct.unpack('<QQQi', data[0:28])
        rank, group_id, timestamp_ns, event_code = header
        
        # Validate event code is within bounds
        if not (0 <= event_code < len(AelEvent)):
            raise EventCodeError(
                f"Invalid event code {event_code} (valid range: 0-{len(AelEvent)-1})"
            )
        
        # Unpack payload fields (bytes 32-64)
        payload = struct.unpack('qQ16s', data[32:64])
        id_version, size, name_bytes = payload
        
        # Extract message field (bytes 64-128)
        msg_bytes = data[64:AEL_PACKET_SIZE]
        
        # Decode null-terminated strings
        name_str = name_bytes.split(b'\x00')[0].decode('utf-8', errors='replace')
        msg_str = msg_bytes.split(b'\x00')[0].decode('utf-8', errors='replace')
        
        # Map event code to enum value
        event = AelEvent(event_code)
        
        return AelMessage(
            rank=rank,
            group_id=group_id,
            timestamp_ns=timestamp_ns,
            event=event,
            from_ip=addr[0],  # IP string (e.g., "127.0.0.1")
            from_port=addr[1],  # Port number as int
            id_version=id_version,
            size=size,
            name=name_str,
            event_message=msg_str,
        )
    
    @staticmethod
    def encode(event_code: int, rank: int = 0, group_id: int = 0,
               timestamp_ns: int = 0, id_version: int = -1,
               size: int = 16, name: str = "", event_message: str = "") -> bytes:
        """Construct a valid AEL packet from fields.
        
        Used by sender to create packets matching receiver expectations.
        
        Packet layout (128 bytes):
          [0:8]   rank        uint64_t
          [8:16]  group_id    uint64_t
          [16:24] timestamp   uint64_t
          [24:28] event       int32_t
          [28:32] crc         uint32_t (CRC32 of all bytes except this field)
          [32:40] id_version  int64_t
          [40:48] size        uint64_t
          [48:64] name        char[16]
          [64:128] msg        char[64]
        
        Args:
            event_code: Event type (0-8) matching AelEvent values.
            rank: Source rank (default 0).
            group_id: Group identifier (default 0).
            timestamp_ns: Timestamp in nanoseconds since epoch (default 0).
            id_version: Version ID (default -1).
            size: Name field length (default 16).
            name: Region/region name (null-padded to 16 bytes, default "").
            event_message: Event description message (null-terminated, max 64 chars, default "").
        
        Returns:
            128-byte AEL packet with valid CRC.
        
        Raises:
            EventCodeError: If parameters are out of range.
        """
        if not (0 <= event_code < len(AelEvent)):
            raise EventCodeError(f"Event code {event_code} out of range [0, {len(AelEvent)-1}]")
        
        name_bytes = name.encode('utf-8', errors='replace')[:AEL_NAME_LEN].ljust(AEL_NAME_LEN, b'\x00')
        msg_bytes = event_message.encode('utf-8', errors='replace')[:AEL_EVENT_MSG_LEN].ljust(AEL_EVENT_MSG_LEN, b'\x00')
        
        # Pack header (28 bytes): rank + group_id + timestamp + event
        header = struct.pack('<QQQi', rank, group_id, timestamp_ns, event_code)
        
        # Pack payload fields (32 bytes): id_version + size + name
        payload = struct.pack('qQ16s', id_version, size, name_bytes)
        
        # Assemble: header(28) + CRC(4) + payload(32) + message(64) = 128 total
        # Compute CRC over bytes 0-28 + 32-128 (excluding the CRC field itself)
        crc_data = header + payload + msg_bytes
        
        # Compute and pack CRC at bytes 28-32
        crc_value = zlib.crc32(crc_data) & 0xFFFFFFFF
        crc_bytes = struct.pack('<I', crc_value)
        
        # Complete packet: header + CRC field + rest of payload + message
        packet = header + crc_bytes + payload + msg_bytes
        
        return packet


__all__ = ['AELPacket', 'AelEvent', 'AelMessage', 'AELErrors', 'CRCError', 
           'PacketSizeError', 'EventCodeError']
