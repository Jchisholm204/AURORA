#!/usr/bin/env python3
"""
@file fixtures.py
@brief Pytest fixtures for AEL testing without subprocess spawns.

Uses the receiver's CSVLogger read/write abstraction and raw event IDs (0-8)
from the C struct. Paths are resolved relative to this file.
"""

import os
import sys

import pytest

# Allow direct execution from python_external_logger/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.ael import AelEvent, AelMessage
from core.ael_server import AELServer
from receiver import CSVLogger


def _make_message(rank: int, event_id: int, **overrides) -> AelMessage:
    """Build an AelMessage with defaults suitable for fixture tests."""
    fields = {
        "rank": rank,
        "group_id": 0,
        "timestamp_ns": 1_700_000_000_000_000_000,
        "event": AelEvent(event_id),
        "from_ip": "127.0.0.1",
        "from_port": 8506,
        "id_version": -1,
        "size": 16,
        "name": "",
        "event_message": "",
    }
    fields.update(overrides)
    return AelMessage(**fields)


@pytest.fixture
def message_logger():
    """Pytest fixture: creates AEL server on a clean port."""
    # Use unique port per test session to avoid conflicts
    import socket

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 8506))
    port = sock.getsockname()[1]
    sock.close()

    server = AELServer(port=port)
    server.open()

    yield server

    server.close()


@pytest.fixture
def csv_writer(tmp_path):
    """Pytest fixture: open CSVLogger writing to a test-local path."""
    log = CSVLogger(str(tmp_path / "pytest.csv"))
    log.open()

    yield log

    log.close()


@pytest.mark.skipif(
    os.environ.get("AEL_TEST_CLIENT") != "1",
    reason="requires multi_client sending to port 8506 (set AEL_TEST_CLIENT=1)",
)
def test_rank_tracking(message_logger):
    """Test tracking CLI_CONNECT events with direct event IDs."""
    import time
    time.sleep(0.5)  # Give client time to connect

    connects = set()
    for _ in range(16):
        try:
            msg = message_logger.recv(timeout=300)
            if msg.event.value == 0:  # CLI_CONNECT (direct event ID)
                connects.add(msg.rank)
        except TimeoutError:
            break

    assert len(connects) == 4, f"Expected 4 ranks, got {connects}"


def test_region_events(csv_writer):
    """Test tracking REGION_ADD/REMOVE events with direct event IDs."""
    written = []
    for event_id in [7, 8]:  # REGION_ADD, REGION_REMOVE
        msg = _make_message(
            event_id % 4,
            event_id,
            id_version=100,
            size=64,
            name=f"region_{event_id}",
            event_message=f"event {event_id}",
        )
        csv_writer.write(msg)
        written.append(msg)

    assert csv_writer.read() == written


def test_checkpoint_events(csv_writer):
    """Test tracking checkpoint events with direct event IDs."""
    written = []
    for event_id in [3, 4]:  # CHECKPOINT_START, CHECKPOINT_END
        msg = _make_message(
            event_id % 4,
            event_id,
            id_version=-1,
            size=16,
            event_message=f"checkpoint {event_id}",
        )
        csv_writer.write(msg)
        written.append(msg)

    assert csv_writer.read() == written


def test_csv_format(csv_writer):
    """Verify CSV header and row values match the documented schema."""
    msg = _make_message(
        2,
        7,  # REGION_ADD (raw event ID)
        id_version=12345,
        size=4096,
        name="test_region",
        event_message="region added",
    )
    csv_writer.write(msg)

    with open(csv_writer.path, "r", newline="") as f:
        lines = f.read().splitlines()

    assert lines[0] == (
        "rank,group_id,timestamp_ns,event,from_ip,from_port,"
        "id_version,size,name,event_message"
    )

    assert len(lines) == 2, "Expected exactly one data row after a single write"
    row = lines[1].split(",")
    assert row[0] == "2"        # rank
    assert row[3] == "7"        # raw event ID
    assert row[7] == "4096"     # size
    assert row[8] == "test_region"
