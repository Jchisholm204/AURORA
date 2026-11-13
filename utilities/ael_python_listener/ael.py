#  @file ael.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Jacob Chisholm (https://Jchisholm204.github.io)
#  @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief AURORA External Logger (AEL) packet library
#  @version 0.2
#  @date Created: 2026-11-03
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026
#
#  Python counterpart of common/include/ael/ael.h. The wire format must match
#  common/include/ael/ael_packet.h, and the receive checks must match
#  common/src/ael/ael_recv.c.

import enum
import socket
import struct
import zlib
from dataclasses import dataclass
from datetime import datetime, timezone

# ---------------------------------------------------------------- wire format
# All layout details live here so a change to ael_packet.h only needs to be
# made in one place. Fields are little endian (the C side sends the raw
# struct).
#
#   [0:8]    rank          uint64
#   [8:16]   group_id      uint64
#   [16:24]  timestamp_ns  uint64
#   [24:26]  event         uint16 (eAEL_event)
#   [26:28]  origin        uint16 (eAEL_origin)
#   [28:32]  crc           uint32 (CRC-32 of every byte except this field)
#   [32:40]  id_version    int64
#   [40:48]  size          uint64
#   [48:64]  name          char[16]
#   [64:128] event_message char[64]
AEL_DEFAULT_PORT = 8334
AEL_PACKET_SIZE = 128
AEL_NAME_LEN = 16
AEL_EVENT_MSG_LEN = 64

_HEADER_FMT = "<QQQHH"
_HEADER = slice(0, 28)
_CRC_FMT = "<I"
_CRC = slice(28, 32)
_PAYLOAD_FMT = f"<qQ{AEL_NAME_LEN}s"
_PAYLOAD = slice(32, 64)
_MSG = slice(64, AEL_PACKET_SIZE)

# Packet strings are untrusted bytes. surrogateescape keeps every byte
# recoverable, so text written back out is byte identical to what was sent.
_ENCODING = "utf-8"
_ERRORS = "surrogateescape"


class AelEvent(enum.IntEnum):
    """Mirrors eAEL_event from ael_packet.h."""
    CLI_CONNECT = 0
    CLI_FAILURE = 1
    CLI_DISCONNECT = 2
    CHECKPOINT_START = 3
    CHECKPOINT_END = 4
    RESTORE_START = 5
    RESTORE_END = 6
    REGION_ADD = 7
    REGION_REMOVE = 8

    @property
    def label(self) -> str:
        """CSV/stdout name, e.g. "checkpoint_start"."""
        return self.name.lower()

    @property
    def is_lifecycle(self) -> bool:
        """Client lifecycle event: connect, failure, or disconnect."""
        return self in (AelEvent.CLI_CONNECT, AelEvent.CLI_FAILURE,
                        AelEvent.CLI_DISCONNECT)


class AelOrigin(enum.IntEnum):
    """Mirrors eAEL_origin from ael_packet.h."""
    ARE = 0
    MPI = 1
    SMARTNIC = 2
    HOST = 3

    @property
    def label(self) -> str:
        """CSV/stdout name, e.g. "SmartNIC"."""
        return _ORIGIN_LABELS[self]


_ORIGIN_LABELS = {
    AelOrigin.ARE: "ARE",
    AelOrigin.MPI: "MPI",
    AelOrigin.SMARTNIC: "SmartNIC",
    AelOrigin.HOST: "Host",
}


class AelError(Exception):
    """Base class for a datagram that is discarded (never fatal)."""


class PacketSizeError(AelError):
    pass


class CRCError(AelError):
    pass


class EventCodeError(AelError):
    pass


class OriginCodeError(AelError):
    pass


def format_time(timestamp_ns: int) -> str:
    """Nanosecond epoch timestamp as ISO 8601 UTC with milliseconds
    (e.g. 2026-11-04T15:30:45.123Z), as the C listener prints it."""
    secs, ns = divmod(timestamp_ns, 1_000_000_000)
    try:
        base = datetime.fromtimestamp(secs, tz=timezone.utc)
    except (OverflowError, OSError, ValueError):
        return str(timestamp_ns)
    return f"{base:%Y-%m-%dT%H:%M:%S}.{ns // 1_000_000:03d}Z"


@dataclass(frozen=True)
class AelMessage:
    """Decoded AEL packet (mirrors ael_message_t)."""
    rank: int
    group_id: int
    timestamp_ns: int
    event: AelEvent
    origin: AelOrigin
    id_version: int
    size: int
    name: str
    event_message: str
    from_ip: str = ""
    from_port: int = 0

    @property
    def timestamp(self) -> str:
        """Sender timestamp as ISO 8601 UTC."""
        return format_time(self.timestamp_ns)

    @property
    def name_bytes(self) -> bytes:
        """Payload name exactly as received."""
        return self.name.encode(_ENCODING, _ERRORS)

    @property
    def event_message_bytes(self) -> bytes:
        """Event message exactly as received."""
        return self.event_message.encode(_ENCODING, _ERRORS)


def _crc(data: bytes) -> int:
    return zlib.crc32(data[_HEADER] + data[_PAYLOAD.start:]) & 0xFFFFFFFF


def _cstr(raw: bytes) -> str:
    # The C receiver force terminates the last byte of each string field
    return raw[:-1].split(b"\0", 1)[0].decode(_ENCODING, _ERRORS)


def _check_event(event_code: int) -> AelEvent:
    try:
        return AelEvent(event_code)
    except ValueError:
        raise EventCodeError(
            f"Invalid event code {event_code} "
            f"(valid range: 0-{len(AelEvent) - 1})") from None


def _check_origin(origin_code: int) -> AelOrigin:
    try:
        return AelOrigin(origin_code)
    except ValueError:
        raise OriginCodeError(
            f"Invalid origin code {origin_code} "
            f"(valid range: 0-{len(AelOrigin) - 1})") from None


def decode(data: bytes, addr: tuple = ("", 0)) -> AelMessage:
    """Decode and verify one AEL datagram.

    The checks run in the same order as ael_recv_message: size, CRC, event
    code, origin code.

    Raises:
        PacketSizeError, CRCError, EventCodeError, OriginCodeError
    """
    if len(data) != AEL_PACKET_SIZE:
        raise PacketSizeError(
            f"Packet size {len(data)} != {AEL_PACKET_SIZE}")

    (crc,) = struct.unpack(_CRC_FMT, data[_CRC])
    expected = _crc(data)
    if crc != expected:
        raise CRCError(f"CRC mismatch: got {crc:#010x}, "
                       f"expected {expected:#010x}")

    rank, group_id, timestamp_ns, event_code, origin_code = struct.unpack(
        _HEADER_FMT, data[_HEADER])
    event = _check_event(event_code)
    origin = _check_origin(origin_code)
    id_version, size, name = struct.unpack(_PAYLOAD_FMT, data[_PAYLOAD])

    return AelMessage(
        rank=rank,
        group_id=group_id,
        timestamp_ns=timestamp_ns,
        event=event,
        origin=origin,
        id_version=id_version,
        size=size,
        name=_cstr(name),
        event_message=_cstr(data[_MSG]),
        from_ip=addr[0],
        from_port=addr[1],
    )


def encode(event: int, rank: int = 0, group_id: int = 0,
           timestamp_ns: int = 0, id_version: int = -1, size: int = 0,
           name: str | bytes = "", event_message: str | bytes = "",
           origin: int = AelOrigin.MPI) -> bytes:
    """Build a valid AEL datagram (used by tests and fake senders).

    Strings are cut to fit with a NUL terminator, like the C snprintf.
    """
    event = _check_event(int(event))
    origin = _check_origin(int(origin))
    if isinstance(name, str):
        name = name.encode(_ENCODING, _ERRORS)
    if isinstance(event_message, str):
        event_message = event_message.encode(_ENCODING, _ERRORS)
    name_b = name[:AEL_NAME_LEN - 1]
    msg_b = event_message[:AEL_EVENT_MSG_LEN - 1]

    header = struct.pack(_HEADER_FMT, rank, group_id, timestamp_ns, event,
                         origin)
    payload = struct.pack(_PAYLOAD_FMT, id_version, size, name_b)
    body = payload + msg_b.ljust(AEL_EVENT_MSG_LEN, b"\0")

    crc = zlib.crc32(header + body) & 0xFFFFFFFF
    return header + struct.pack(_CRC_FMT, crc) + body


class AelReceiver:
    """Receive-only AEL endpoint (the Python ael_recv_message).

    Usage:
        with AelReceiver(port=8334) as rx:
            msg = rx.recv(timeout=1.0)
    """

    def __init__(self, port: int = AEL_DEFAULT_PORT, host: str = "0.0.0.0"):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._sock.bind((host, port))
        except OSError:
            self._sock.close()
            raise

    @property
    def port(self) -> int:
        """Bound port (useful when constructed with port=0)."""
        return self._sock.getsockname()[1]

    def fileno(self) -> int:
        return self._sock.fileno()

    def recv(self, timeout: float | None = None) -> AelMessage | None:
        """Receive and decode one AEL message from any sender.

        Args:
            timeout: seconds to wait; None blocks, 0 polls.

        Returns:
            The message, or None when the timeout expired.

        Raises:
            AelError: an invalid datagram was received and discarded.
            OSError: the socket failed (fatal).
        """
        self._sock.settimeout(timeout)
        try:
            # One spare byte so an oversized datagram is seen as oversized
            data, addr = self._sock.recvfrom(AEL_PACKET_SIZE + 1)
        except (TimeoutError, BlockingIOError):
            return None
        return decode(data, addr)

    def close(self) -> None:
        self._sock.close()

    def __enter__(self) -> "AelReceiver":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.close()
        return False
