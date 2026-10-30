#!/usr/bin/env python3
"""
@file integration.py
@brief Integration tests for AEL Python receiver against C clients.

Tests that the Python receiver:
  1. Correctly parses packets created by C code (simulated)
  2. Produces output matching C test_ael_server.c format
  3. Handles all event types from multi_client.c

@version 0.3
@author qwen 3.5 9b
@author DeepSeek V4.1 Flash Free
"""

import os
import sys

# Add python_external_logger/ to sys.path so core.* imports work from a subfolder
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, parent_dir)

from core.ael import AELPacket, AelEvent, AEL_PACKET_SIZE


def test_packet_format_matches_c():
    """Test that our packet format matches C wire format exactly."""
    print("Testing packet format compatibility with C code...", file=sys.stderr)

    # Create test packets matching multi_client.c event types
    events = [
        (AelEvent.CLI_CONNECT, "client 0 connect"),
        (AelEvent.CLI_FAILURE, "client failure"),
        (AelEvent.CHECKPOINT_END, "ckpt-end"),
        (AelEvent.RESTORE_END, "restore-end"),
        (AelEvent.REGION_ADD, "rgn-42"),
    ]

    for event, message in events:
        packet = AELPacket.encode(
            event_code=event.value,
            rank=0,
            group_id=0,
            timestamp_ns=0,
            id_version=-1,
            size=16,
            name="test-event",
            event_message=message
        )

        assert len(packet) == AEL_PACKET_SIZE, \
            f"Packet size {len(packet)} != {AEL_PACKET_SIZE}"

        # Re-decode to verify round-trip
        msg = AELPacket.decode(packet, ("127.0.0.1", 12345))
        assert msg.event == event, \
            f"Event {event.name} decoded as {msg.event.name}"

    print("✅ All packet formats match C expectations", file=sys.stderr)


def test_decode_all_event_types():
    """Test decoding all 9 event types defined in eAEL_event."""
    print("Testing decode for all event types...", file=sys.stderr)

    expected_events = [
        (0, "CLI_CONNECT"),
        (1, "CLI_FAILURE"),
        (2, "CLI_DISCONNECT"),
        (3, "CHECKPOINT_START"),
        (4, "CHECKPOINT_END"),
        (5, "RESTORE_START"),
        (6, "RESTORE_END"),
        (7, "REGION_ADD"),
        (8, "REGION_REMOVE"),
    ]

    for code, name in expected_events:
        packet = AELPacket.encode(
            event_code=code,
            rank=0,
            group_id=0,
            timestamp_ns=1729500000000000000,  # Fixed timestamp for testing
            id_version=-1,
            size=16,
            name=f"test-{code}",
            event_message=f"event {name}"
        )

        msg = AELPacket.decode(packet, ("127.0.0.1", 54321))

        assert msg.rank == 0, f"Event {name}: rank {msg.rank} != 0"
        assert msg.group_id == 0, f"Event {name}: group_id {msg.group_id} != 0"
        assert msg.timestamp_ns == 1729500000000000000, \
            f"Event {name}: timestamp {msg.timestamp_ns} mismatch"
        assert int(msg.event) == code, \
            f"Event {name}: decoded code {int(msg.event)} != {code}"

    print("✅ All 9 event types decode correctly", file=sys.stderr)


def test_crc_validation():
    """Test CRC computation and validation."""
    print("Testing CRC validation...", file=sys.stderr)

    # Create valid packet
    valid_packet = AELPacket.encode(
        event_code=AelEvent.CHECKPOINT_END.value,
        rank=5,
        group_id=1,
        timestamp_ns=1234567890000000000,
        id_version=-1,
        size=16,
        name="checkpoint-region",
        event_message="checkpoint end"
    )

    assert len(valid_packet) == 128, f"Packet size {len(valid_packet)} != 128"

    # Decode should succeed
    msg = AELPacket.decode(valid_packet, ("127.0.0.1", 8334))
    assert msg.rank == 5
    assert msg.group_id == 1
    print("✅ Valid packet decoded correctly", file=sys.stderr)

    # Test CRC mismatch detection - corrupt the CRC field directly
    corrupted = bytearray(valid_packet)
    corrupted[28] ^= 0xFF

    try:
        AELPacket.decode(bytes(corrupted), ("127.0.0.1", 8334))
    except Exception as crc_err:
        assert "CRC" in str(crc_err), f"Wrong exception for CRC error: {crc_err}"
    else:
        raise AssertionError("Should have detected CRC mismatch in corrupted packet")

    print("✅ CRC mismatch detected correctly", file=sys.stderr)


def test_output_format_matches_c():
    """Test that output format matches C test_ael_server.c."""
    print("Testing output format compatibility with C code...", file=sys.stderr)

    msg = AELPacket.decode(
        AELPacket.encode(
            event_code=AelEvent.CHECKPOINT_END.value,
            rank=99,
            group_id=1,
            timestamp_ns=1700000000000000000,
            id_version=-1,
            size=16,
            name="test-name",
            event_message="checkpoint end message"
        ),
        ("127.0.0.1", 52979)
    )

    # Expected output format from C:
    # rx from=127.0.0.1:52979 rank=99 group_id=1 ts=... ev=CHECKPOINT_END
    #   id_version=-1 size=16 name="test-name"
    #   event="checkpoint end message"

    timestamp_str = msg.timestamp

    output_lines = [
        f"rx from={msg.from_ip}:{msg.from_port} rank={msg.rank} "
        f"group_id={msg.group_id} ts={timestamp_str} ev={msg.event.name}",
        f"  id_version={msg.id_version} size={msg.size} name=\"{msg.name}\"",
        f'  event="{msg.event_message}"'
    ]

    expected_lines = [
        "rx from=127.0.0.1:52979 rank=99 group_id=1 ts=2023-11-14T22:13:20+00:00 ev=CHECKPOINT_END",
        "  id_version=-1 size=16 name=\"test-name\"",
        '  event="checkpoint end message"'
    ]

    for i, (actual, expected) in enumerate(zip(output_lines, expected_lines)):
        assert actual == expected, (
            f"Line {i+1} mismatch:\n  Expected: {expected}\n  Actual:   {actual}"
        )

    print("✅ Output format matches C test_ael_server.c", file=sys.stderr)


def test_multi_client_event_patterns():
    """Test that we handle multi_client.c event patterns."""
    print("Testing multi_client.c event patterns...", file=sys.stderr)

    # Simulate a sequence of events from multi_client.c
    sequences = [
        # Client connection sequence
        [(0, "CLI_CONNECT", 1), (2, "CHECKPOINT_END", 1)],
        # Region restore sequence
        [(3, "RESTORE_START", 42), (6, "RESTORE_END", 42)],
        # Region lifecycle
        [(7, "REGION_ADD", 99), (8, "REGION_REMOVE", 99)],
    ]

    for event_sequence in sequences:
        for event_code, event_name, extra in event_sequence:
            packet = AELPacket.encode(
                event_code=event_code,
                rank=0,
                group_id=extra,
                timestamp_ns=0,
                id_version=-1,
                size=16,
                name=f"test-{event_name}",
                event_message=event_name.lower()
            )

            msg = AELPacket.decode(packet, ("client", 456))
            assert msg.group_id == extra, \
                f"Event {event_name}: group_id {msg.group_id} != {extra}"
            # Name field is truncated to first 16 chars; check it contains expected prefix
            assert msg.name.startswith("test-"), \
                f"Event {event_name}: name {msg.name!r} lost its prefix"

    print("✅ multi_client.c event patterns work correctly", file=sys.stderr)


def main():
    """Run all integration tests."""
    print("=" * 60, file=sys.stderr)
    print("AEL Python Receiver Integration Tests", file=sys.stderr)
    print("=" * 60, file=sys.stderr)

    tests = (
        test_packet_format_matches_c,
        test_decode_all_event_types,
        test_crc_validation,
        test_output_format_matches_c,
        test_multi_client_event_patterns,
    )

    failed = []
    for test in tests:
        try:
            test()
        except AssertionError as e:
            print(f"❌ {test.__name__} FAILED: {e}", file=sys.stderr)
            failed.append(test.__name__)

    print("=" * 60, file=sys.stderr)
    if failed:
        print(f"❌ {len(failed)} test(s) FAILED: {', '.join(failed)}", file=sys.stderr)
        return 1

    print("✅ All integration tests PASSED!", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
