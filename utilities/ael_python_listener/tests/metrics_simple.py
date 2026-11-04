#!/usr/bin/env python3
"""
@file metrics_simple.py
@brief Tests for the AELMetrics background aggregation layer.

Sends real UDP datagrams to a local AELMetrics instance and checks per-category
history, implicit-last counting, explicit version/id/name filters, and
lifecycle handling.

Usage:
    python3 tests/metrics_simple.py
    python3 -m pytest tests/metrics_simple.py
"""

import os
import socket
import sys
import time

# Allow direct execution from python_external_logger/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.ael import AELPacket, AelEvent
from core.ael_metrics import AELMetrics, AelCategory
from core.ael_server import AELFatalError

TIMESTAMP_BASE = 1_700_000_000_000_000_000


def _free_port() -> int:
    """Ask the OS for an unused UDP port."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def _send(port: int, rank: int, event: AelEvent, **overrides) -> None:
    """Send one AEL packet from the given rank."""
    packet = AELPacket.encode(
        event_code=int(event),
        rank=rank,
        group_id=overrides.get("group_id", 0),
        timestamp_ns=overrides.get("timestamp_ns", TIMESTAMP_BASE + rank),
        id_version=overrides.get("id_version", -1),
        size=overrides.get("size", 16),
        name=overrides.get("name", ""),
        event_message=overrides.get("event_message", ""),
    )
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, ("127.0.0.1", port))


def _wait_value(fn, expected, timeout: float = 2.0):
    """Poll fn() until it equals expected, then return the last value read."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = fn()
        if value == expected:
            return value
        time.sleep(0.005)
    return fn()


def _wait_until(predicate, timeout: float = 2.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.005)
    return False


def test_counts_unique_ranks():
    """Per-rank CLI text still collapses into one identity with four ranks."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        for rank in range(4):
            _send(port, rank, AelEvent.CLI_CONNECT,
                  event_message=f"client {rank} connect")

        ranks = _wait_value(
            lambda: metrics.ranks_cli(AelEvent.CLI_CONNECT), set(range(4))
        )
        assert ranks == set(range(4)), f"Expected all four ranks, got {ranks}"

        assert metrics.get_cli(AelEvent.CLI_CONNECT) == 4
        # AelEvent is an IntEnum and category strings are normalized.
        assert metrics.get_cli(0) == 4
        assert metrics.get("cli", AelEvent.CLI_CONNECT) == 4

        texts = {
            msg.event_message
            for msg in metrics.messages(AelCategory.CLI, AelEvent.CLI_CONNECT)
        }
        assert texts == {f"client {rank} connect" for rank in range(4)}


def test_implicit_last_and_explicit_filters():
    """Missing version/name selects the latest identity; filters go cumulative."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        for rank in range(4):
            _send(port, rank, AelEvent.CHECKPOINT_START,
                  id_version=1, size=4, name="ckpt-start")

        waited = _wait_value(
            lambda: metrics.ranks_checkpoint(AelEvent.CHECKPOINT_START,
                                             version=1),
            set(range(4)),
        )
        assert waited == set(range(4))

        # Send the second checkpoint only after the first is fully received,
        # so the newest message in the list is deterministic.
        for rank in range(2):
            _send(port, rank, AelEvent.CHECKPOINT_START,
                  id_version=3, size=4, name="ckpt-start")

        waited = _wait_value(
            lambda: metrics.ranks_checkpoint(AelEvent.CHECKPOINT_START,
                                             version=3),
            {0, 1},
        )
        assert waited == {0, 1}

        # No version/name -> latest identity (version 3, ranks 0 and 1).
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START) == 2
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START, version=1) == 4
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START, version=3) == 2
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START,
                                      name="ckpt-start") == 4
        # Positional filters match the documented call shape.
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START, 1,
                                      "ckpt-start") == 4
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_START, version=9) == 0
        assert metrics.get_checkpoint(AelEvent.CHECKPOINT_END) == 0

        newest = metrics.last_checkpoint(AelEvent.CHECKPOINT_START)
        assert newest is not None and newest.id_version == 3
        assert newest.rank in {0, 1}


def test_region_implicit_last_and_filters():
    """Region queries use the region ID (id_version) and name filters."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        for rank in range(3):
            _send(port, rank, AelEvent.REGION_ADD,
                  id_version=42, size=4096, name="rgn-42")
        assert _wait_value(
            lambda: metrics.ranks_region(AelEvent.REGION_ADD, id=42),
            {0, 1, 2},
        ) == {0, 1, 2}

        for rank in range(2):
            _send(port, rank, AelEvent.REGION_ADD,
                  id_version=7, size=4096, name="rgn-7")
        assert _wait_value(
            lambda: metrics.ranks_region(AelEvent.REGION_ADD, id=7),
            {0, 1},
        ) == {0, 1}

        _send(port, 3, AelEvent.REGION_ADD,
              id_version=9, size=4096, name="rgn-9")
        assert _wait_value(
            lambda: metrics.ranks_region(AelEvent.REGION_ADD, id=9), {3}
        ) == {3}

        # Latest region identity is id=9.
        assert metrics.get_region(AelEvent.REGION_ADD) == 1
        assert metrics.get_region(AelEvent.REGION_ADD, id=42) == 3
        assert metrics.get_region(AelEvent.REGION_ADD, name="rgn-7") == 2
        assert metrics.get_region(AelEvent.REGION_ADD, id=42,
                                  name="rgn-42") == 3
        # Positional filters match the documented call shape.
        assert metrics.get_region(AelEvent.REGION_ADD, 42, "rgn-42") == 3
        assert metrics.get_region(AelEvent.REGION_REMOVE) == 0

        newest = metrics.last_region(AelEvent.REGION_ADD)
        assert newest is not None and newest.name == "rgn-9"
        assert newest.id_version == 9


def test_messages_are_newest_first_and_grouped():
    """Category lists keep independent history with the newest message first."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        _send(port, 0, AelEvent.CLI_CONNECT, event_message="first")
        assert _wait_value(lambda: metrics.get_cli(AelEvent.CLI_CONNECT), 1) == 1

        _send(port, 1, AelEvent.CLI_CONNECT, event_message="second")
        assert _wait_value(
            lambda: metrics.ranks_cli(AelEvent.CLI_CONNECT), {0, 1}
        ) == {0, 1}

        cli = metrics.messages(AelCategory.CLI)
        assert [msg.event_message for msg in cli] == ["second", "first"]
        assert metrics.messages(AelCategory.CLI, limit=1)[0].rank == 1

        assert metrics.last_cli(AelEvent.CLI_CONNECT).event_message == "second"
        assert metrics.last_cli(AelEvent.CLI_DISCONNECT) is None
        assert metrics.messages(AelCategory.REGION) == []

        _send(port, 0, AelEvent.CHECKPOINT_END,
              id_version=2, size=4, name="ckpt-end")
        assert _wait_value(
            lambda: metrics.get_checkpoint(AelEvent.CHECKPOINT_END), 1
        ) == 1
        checkpoint = metrics.last_checkpoint(AelEvent.CHECKPOINT_END)
        assert checkpoint is not None and checkpoint.name == "ckpt-end"
        assert len(metrics.messages(AelCategory.CHECKPOINT)) == 1


def test_duplicate_rank_counts_once():
    """A rank retransmitting the same message is counted once."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        for _ in range(3):
            _send(port, 5, AelEvent.CLI_FAILURE, event_message="boom")

        assert _wait_until(lambda: metrics.total_messages >= 3)
        assert metrics.get_cli(AelEvent.CLI_FAILURE) == 1
        assert metrics.total_messages == 3, "Every datagram should be counted"


def test_clear_drops_history_and_counts():
    """clear() is the only reset path and empties matching state."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        for rank in range(2):
            _send(port, rank, AelEvent.RESTORE_START,
                  id_version=1, size=4, name="restore-start")

        assert _wait_value(
            lambda: metrics.get_checkpoint(AelEvent.RESTORE_START), 2
        ) == 2
        assert len(metrics.messages(AelCategory.CHECKPOINT)) == 2

        metrics.clear(event=AelEvent.RESTORE_START)

        assert metrics.get_checkpoint(AelEvent.RESTORE_START) == 0
        assert metrics.messages(AelCategory.CHECKPOINT) == []
        assert metrics.last_checkpoint(AelEvent.RESTORE_START) is None

        _send(port, 0, AelEvent.RESTORE_START,
              id_version=1, size=4, name="restore-start")
        assert _wait_value(
            lambda: metrics.get_checkpoint(AelEvent.RESTORE_START), 1
        ) == 1


def test_invalid_packet_is_survived():
    """A malformed datagram is counted and does not stop the loop."""
    port = _free_port()

    with AELMetrics(port=port) as metrics:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(b"\x00" * 128, ("127.0.0.1", port))

        assert _wait_until(lambda: metrics.invalid_packets >= 1), \
            "Corrupt datagram was not counted as invalid"

        _send(port, 9, AelEvent.CLI_DISCONNECT, event_message="bye")
        assert _wait_value(lambda: metrics.get_cli(AelEvent.CLI_DISCONNECT), 1) == 1
        assert metrics.running, "Background loop died after a bad packet"


def test_start_stop_is_idempotent():
    """start()/stop() can be called repeatedly and the server restarts."""
    port = _free_port()
    metrics = AELMetrics(port=port)

    metrics.start()
    metrics.start()  # no-op
    assert metrics.running
    metrics.stop()
    assert not metrics.running
    metrics.stop()  # no-op

    metrics.start()
    try:
        _send(port, 0, AelEvent.CLI_CONNECT, event_message="again")
        assert _wait_value(lambda: metrics.get_cli(AelEvent.CLI_CONNECT), 1) == 1
    finally:
        metrics.stop()


def test_bind_conflict_raises():
    """Binding an occupied port surfaces as AELFatalError from start()."""
    port = _free_port()
    blocker = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    blocker.bind(("", port))
    try:
        metrics = AELMetrics(port=port)
        try:
            metrics.start()
        except AELFatalError:
            pass
        else:
            metrics.stop()
            raise AssertionError("Expected AELFatalError for an occupied port")
    finally:
        blocker.close()


def test_invalid_queries_raise():
    """Mismatched categories and ambiguous last queries raise ValueError."""
    metrics = AELMetrics(port=_free_port())

    try:
        metrics.get(AelCategory.REGION, AelEvent.CLI_CONNECT)
    except ValueError:
        pass
    else:
        raise AssertionError("Expected ValueError for category/event mismatch")

    try:
        metrics.get(None)
    except ValueError:
        pass
    else:
        raise AssertionError("Expected ValueError for category-less last query")

    try:
        metrics.last(None)
    except ValueError:
        pass
    else:
        raise AssertionError("Expected ValueError for category-less last()")


def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("AEL Metrics Tests")
    print("=" * 60)
    print()

    tests = [
        test_counts_unique_ranks,
        test_implicit_last_and_explicit_filters,
        test_region_implicit_last_and_filters,
        test_messages_are_newest_first_and_grouped,
        test_duplicate_rank_counts_once,
        test_clear_drops_history_and_counts,
        test_invalid_packet_is_survived,
        test_start_stop_is_idempotent,
        test_bind_conflict_raises,
        test_invalid_queries_raise,
    ]

    try:
        for test in tests:
            test()
            print(f"✓ {test.__name__}")
    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        return False

    print()
    print("=" * 60)
    print("ALL TESTS PASSED ✓")
    print("=" * 60)
    return True


if __name__ == '__main__':
    sys.exit(0 if run_all_tests() else 1)
