#!/usr/bin/env python3
"""
@file csv_simple.py
@brief Direct CSVLogger tests - no server required.

Exercises the write()/read() abstraction in receiver.CSVLogger using raw
event IDs (0-8) and verifies lossless round-tripping to AelMessage objects.

Usage:
    python3 tests/csv_simple.py
"""

import os
import sys
import tempfile

# Allow direct execution from python_external_logger/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.ael import AelEvent, AelMessage
from receiver import CSVLogger, EVENT_NAMES


def _make_message(rank: int, event_id: int, **overrides) -> AelMessage:
    """Build an AelMessage with sensible defaults for tests."""
    fields = {
        "rank": rank,
        "group_id": 0,
        "timestamp_ns": 1_700_000_000_000_000_000,
        "event": AelEvent(event_id),
        "from_ip": "127.0.0.1",
        "from_port": 8334,
        "id_version": -1,
        "size": 16,
        "name": "",
        "event_message": "",
    }
    fields.update(overrides)
    return AelMessage(**fields)


def test_csv_write_read_roundtrip():
    """Test that written messages read back identically."""
    messages = [
        _make_message(0, 0, event_message="client 0 connect"),
        _make_message(1, 1, event_message="client 1 failure"),
        _make_message(2, 2, event_message="client 2 disconnect"),
    ]

    with tempfile.TemporaryDirectory(prefix="ael_csv_") as tmp:
        path = os.path.join(tmp, "test_direct.csv")

        with CSVLogger(path) as log:
            for msg in messages:
                log.write(msg)

        assert os.path.exists(path), "CSV file was not created"

        read_back = CSVLogger(path).read()
        assert read_back == messages, f"Round-trip mismatch: {read_back!r} != {messages!r}"

    print(f"✓ Write/read round-trip preserved {len(messages)} messages")


def test_event_name_lookup():
    """Test the EVENT_NAMES table derived from AelEvent."""
    print(f"  EVENT_NAMES: {EVENT_NAMES}")

    expected = {
        0: "CLI_CONNECT",
        1: "CLI_FAILURE",
        2: "CLI_DISCONNECT",
        3: "CHECKPOINT_START",
        4: "CHECKPOINT_END",
        5: "RESTORE_START",
        6: "RESTORE_END",
        7: "REGION_ADD",
        8: "REGION_REMOVE",
    }
    assert EVENT_NAMES == expected, f"EVENT_NAMES mismatch: {EVENT_NAMES}"
    print(f"✓ Event name lookup table works ({len(EVENT_NAMES)} events)")


def test_all_event_ids_roundtrip():
    """Test that every raw event ID 0-8 survives a CSV round-trip."""
    messages = [
        _make_message(event_id % 4, event_id, name=f"rgn-{event_id}", event_message=f"event {event_id}")
        for event_id in range(9)
    ]

    with tempfile.TemporaryDirectory(prefix="ael_csv_") as tmp:
        path = os.path.join(tmp, "raw_ids.csv")
        with CSVLogger(path) as log:
            for msg in messages:
                log.write(msg)

        read_back = CSVLogger(path).read()

    assert [int(m.event) for m in read_back] == list(range(9))
    assert read_back == messages
    print("✓ Direct event ID access (0-8) round-trips through CSV")


def test_header_and_column_order():
    """Test that the header matches the documented column order."""
    with tempfile.TemporaryDirectory(prefix="ael_csv_") as tmp:
        path = os.path.join(tmp, "header.csv")
        with CSVLogger(path) as log:
            log.write(_make_message(0, 0))

        with open(path, "r", newline="") as f:
            header = f.readline().strip()

    expected = "rank,group_id,timestamp_ns,event,from_ip,from_port,id_version,size,name,event_message"
    assert header == expected, f"Header mismatch:\n  {header}\n  {expected}"
    print("✓ CSV header/column order is correct")


def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("AEL CSV Logger Tests")
    print("=" * 60)
    print()

    try:
        test_csv_write_read_roundtrip()
        print()
        test_event_name_lookup()
        print()
        test_all_event_ids_roundtrip()
        print()
        test_header_and_column_order()

        print()
        print("=" * 60)
        print("ALL TESTS PASSED ✓")
        print("=" * 60)
        return True

    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        return False


if __name__ == '__main__':
    success = run_all_tests()
    sys.exit(0 if success else 1)
