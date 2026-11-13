#  @file test_ael.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief Tests for the AEL packet library
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026

import socket
import struct

import pytest

import ael
from ael import AelEvent, AelOrigin
from conftest import patched


def test_round_trip():
    data = ael.encode(AelEvent.CHECKPOINT_END, rank=3, group_id=7,
                      timestamp_ns=1_762_270_245_123_456_789, id_version=-5,
                      size=2**63, name="heatdis", event_message="done",
                      origin=AelOrigin.SMARTNIC)
    msg = ael.decode(data, ("10.0.0.1", 4242))
    assert (msg.rank, msg.group_id, msg.id_version, msg.size) == \
        (3, 7, -5, 2**63)
    assert (msg.event, msg.origin) == \
        (AelEvent.CHECKPOINT_END, AelOrigin.SMARTNIC)
    assert (msg.name, msg.event_message) == ("heatdis", "done")
    assert (msg.from_ip, msg.from_port) == ("10.0.0.1", 4242)
    assert msg.timestamp == "2025-11-04T15:30:45.123Z"


def test_layout_matches_ael_packet_h():
    data = ael.encode(AelEvent.REGION_ADD, rank=1, group_id=2,
                      timestamp_ns=3, id_version=4, size=5, name="n",
                      event_message="m", origin=AelOrigin.HOST)
    assert len(data) == 128
    assert struct.unpack_from("<QQQHH", data, 0) == (1, 2, 3, 7, 3)
    assert struct.unpack_from("<qQ", data, 32) == (4, 5)
    assert data[48:64] == b"n".ljust(16, b"\0")
    assert data[64:128] == b"m".ljust(64, b"\0")


def test_crc_is_crc32_skipping_the_crc_field():
    # Known answer from the C implementation's algorithm (CRC-32/ISO-HDLC
    # over bytes [0,28) + [32,128)) for an all-zero packet
    data = ael.encode(AelEvent.CLI_CONNECT, id_version=0,
                      origin=AelOrigin.ARE)
    assert data[:28] == bytes(28) and data[32:] == bytes(96)
    crc = 0xFFFFFFFF
    for _ in range(124):
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1))
    assert struct.unpack_from("<I", data, 28)[0] == crc ^ 0xFFFFFFFF


def test_names():
    assert [e.label for e in AelEvent] == [
        "cli_connect", "cli_failure", "cli_disconnect", "checkpoint_start",
        "checkpoint_end", "restore_start", "restore_end", "region_add",
        "region_remove"]
    assert [o.label for o in AelOrigin] == ["ARE", "MPI", "SmartNIC", "Host"]
    assert [e for e in AelEvent if e.is_lifecycle] == [
        AelEvent.CLI_CONNECT, AelEvent.CLI_FAILURE, AelEvent.CLI_DISCONNECT]


def test_strings_truncated_on_encode():
    msg = ael.decode(ael.encode(AelEvent.CLI_CONNECT, name="x" * 40,
                                event_message="y" * 100))
    assert msg.name == "x" * 15
    assert msg.event_message == "y" * 63


def test_unterminated_strings_are_force_terminated():
    data = ael.encode(AelEvent.CLI_CONNECT)
    data = patched(data, 48, b"N" * 16)
    data = patched(data, 64, b"M" * 64)
    msg = ael.decode(data)
    assert msg.name == "N" * 15
    assert msg.event_message == "M" * 63


def test_string_bytes_survive():
    raw = b"\xff\xfe,\"\n\x01"
    msg = ael.decode(ael.encode(AelEvent.CLI_CONNECT, name=raw,
                                event_message=raw))
    assert msg.name_bytes == raw
    assert msg.event_message_bytes == raw


@pytest.mark.parametrize("ns, text", [
    (0, "1970-01-01T00:00:00.000Z"),
    (999_999, "1970-01-01T00:00:00.000Z"),
    (1_762_270_245_999_999_999, "2025-11-04T15:30:45.999Z"),
    (2**64 - 1, "2554-07-21T23:34:33.709Z"),
])
def test_format_time(ns, text):
    assert ael.format_time(ns) == text


def test_bad_size():
    good = ael.encode(AelEvent.CLI_CONNECT)
    for data in (b"", good[:-1], good + b"\0"):
        with pytest.raises(ael.PacketSizeError):
            ael.decode(data)


def test_bad_crc():
    data = patched(ael.encode(AelEvent.CLI_CONNECT), 0, b"\x01",
                   fix_crc=False)
    with pytest.raises(ael.CRCError):
        ael.decode(data)


def test_bad_event_and_origin():
    good = ael.encode(AelEvent.CLI_CONNECT)
    with pytest.raises(ael.EventCodeError):
        ael.decode(patched(good, 24, struct.pack("<H", len(AelEvent))))
    with pytest.raises(ael.OriginCodeError):
        ael.decode(patched(good, 26, struct.pack("<H", len(AelOrigin))))
    with pytest.raises(ael.EventCodeError):
        ael.encode(99)
    with pytest.raises(ael.OriginCodeError):
        ael.encode(AelEvent.CLI_CONNECT, origin=99)


def test_receiver():
    with ael.AelReceiver(port=0, host="127.0.0.1") as rx, \
            socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as tx:
        assert rx.recv(timeout=0) is None
        assert rx.recv(timeout=0.01) is None

        to = ("127.0.0.1", rx.port)
        tx.sendto(ael.encode(AelEvent.RESTORE_START, rank=9), to)
        msg = rx.recv(timeout=2)
        assert (msg.event, msg.rank) == (AelEvent.RESTORE_START, 9)
        assert (msg.from_ip, msg.from_port) == \
            ("127.0.0.1", tx.getsockname()[1])

        tx.sendto(ael.encode(AelEvent.CLI_CONNECT) + b"extra", to)
        with pytest.raises(ael.PacketSizeError):
            rx.recv(timeout=2)
        tx.sendto(b"short", to)
        with pytest.raises(ael.PacketSizeError):
            rx.recv(timeout=2)
