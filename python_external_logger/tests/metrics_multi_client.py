#!/usr/bin/env python3
"""
@file metrics_multi_client.py
@brief End-to-end test of AELMetrics against the C multi-threaded AEL client.

Runs ``tests/ael/multi_client.c`` (one thread per rank) against a live
AELMetrics server and waits for every rank to reach every event, printing the
result as each event completes:

    waiting for checkpoint started ...
    checkpoint started for all 4 ranks

The wait/confirm pattern is the intended usage model for test programs:
block until the rank count reaches the expected total, then proceed.

Binary discovery order:
    1. $AEL_MULTI_CLIENT
    2. /tmp/opencode/aurora-build/tests/multi_client
    3. <repo>/build/tests/multi_client

If no runnable binary is found the test skips (script mode exits 0).

Usage:
    python3 tests/metrics_multi_client.py
    python3 -m pytest tests/metrics_multi_client.py
"""

import os
import socket
import subprocess
import sys
import time

# Allow direct execution from python_external_logger/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.ael import AelEvent
from core.ael_metrics import AELMetrics


RANKS = set(range(4))
PACKETS_PER_RANK = 36
CLIENT_TIMEOUT = 20.0

EVENT_LABELS = {
    AelEvent.CLI_CONNECT: "CLI connect",
    AelEvent.CLI_FAILURE: "CLI failure",
    AelEvent.CLI_DISCONNECT: "CLI disconnect",
    AelEvent.CHECKPOINT_START: "checkpoint started",
    AelEvent.CHECKPOINT_END: "checkpoint finished",
    AelEvent.RESTORE_START: "restore started",
    AelEvent.RESTORE_END: "restore finished",
    AelEvent.REGION_ADD: "region added",
    AelEvent.REGION_REMOVE: "region removed",
}

SKIP_MESSAGE = (
    "multi_client binary not found or not runnable; build it with "
    "`UCX_HOME=/usr/local make BUILD_DIR=/tmp/opencode/aurora-build all` "
    "or set AEL_MULTI_CLIENT"
)


def _free_port() -> int:
    """Ask the OS for an unused UDP port."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def _runnable(path: str) -> bool:
    """Return True if path exists and can actually load and run."""
    if not (path and os.path.isfile(path) and os.access(path, os.X_OK)):
        return False
    try:
        result = subprocess.run(
            [path, "--help"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return False
    return result.returncode == 0


def _find_client():
    repo_root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    )
    candidates = []
    env_path = os.environ.get("AEL_MULTI_CLIENT")
    if env_path:
        candidates.append(env_path)
    candidates.extend([
        "/tmp/opencode/aurora-build/tests/multi_client",
        os.path.join(repo_root, "build", "tests", "multi_client"),
    ])

    for candidate in candidates:
        if _runnable(candidate):
            return candidate
    return None


def _rank_getter(metrics: AELMetrics, event: AelEvent):
    if event <= AelEvent.CLI_DISCONNECT:
        return lambda: metrics.ranks_cli(event)
    if event <= AelEvent.RESTORE_END:
        return lambda: metrics.ranks_checkpoint(event)
    return lambda: metrics.ranks_region(event)


def _wait_for_all(label: str, ranks_fn, expected=RANKS, timeout: float = 15.0):
    """Wait for an event to be seen from every rank, then print it."""
    print(f"waiting for {label} ...", flush=True)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if ranks_fn() == expected:
            print(f"{label} for all {len(expected)} ranks", flush=True)
            return
        time.sleep(0.01)
    raise AssertionError(
        f"timeout waiting for {label}: got {ranks_fn()}, "
        f"expected {expected}"
    )


def _run(client: str) -> None:
    """Run the C client and wait for every event from every rank."""
    print(f"multi_client: {client}", flush=True)

    port = _free_port()
    expected_total = len(RANKS) * PACKETS_PER_RANK
    metrics = AELMetrics(port=port)
    metrics.start()

    proc = None
    try:
        proc = subprocess.Popen(
            [
                client,
                "127.0.0.1",
                str(len(RANKS)),
                str(PACKETS_PER_RANK),
                str(port),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        for event in (
            AelEvent.CLI_CONNECT,
            AelEvent.CLI_FAILURE,
            AelEvent.CLI_DISCONNECT,
            AelEvent.CHECKPOINT_START,
            AelEvent.CHECKPOINT_END,
            AelEvent.RESTORE_START,
            AelEvent.RESTORE_END,
            AelEvent.REGION_ADD,
            AelEvent.REGION_REMOVE,
        ):
            _wait_for_all(
                EVENT_LABELS[event],
                _rank_getter(metrics, event),
            )

        returncode = proc.wait(timeout=CLIENT_TIMEOUT)
        assert returncode == 0, f"multi_client exited with {returncode}"

        # Deterministic identities from the last cycle (36 packets: events
        # 0..8 repeat four times, so the final ADD is rgn-34, REMOVE rgn-35).
        _wait_for_all(
            "checkpoint started (version 1, ckpt-start)",
            lambda: metrics.ranks_checkpoint(
                AelEvent.CHECKPOINT_START, version=1, name="ckpt-start"
            ),
        )
        _wait_for_all(
            "checkpoint finished (version 2, ckpt-end)",
            lambda: metrics.ranks_checkpoint(
                AelEvent.CHECKPOINT_END, version=2, name="ckpt-end"
            ),
        )
        _wait_for_all(
            "restore started (version 1, restore-start)",
            lambda: metrics.ranks_checkpoint(
                AelEvent.RESTORE_START, version=1, name="restore-start"
            ),
        )
        _wait_for_all(
            "restore finished (version 2, restore-end)",
            lambda: metrics.ranks_checkpoint(
                AelEvent.RESTORE_END, version=2, name="restore-end"
            ),
        )
        _wait_for_all(
            "region added (rgn-34)",
            lambda: metrics.ranks_region(
                AelEvent.REGION_ADD, id=34, name="rgn-34"
            ),
        )
        _wait_for_all(
            "region removed (rgn-35)",
            lambda: metrics.ranks_region(
                AelEvent.REGION_REMOVE, id=35, name="rgn-35"
            ),
        )

        deadline = time.monotonic() + CLIENT_TIMEOUT
        while (
            metrics.total_messages < expected_total
            and time.monotonic() < deadline
        ):
            time.sleep(0.01)

        assert metrics.total_messages == expected_total, (
            f"expected {expected_total} messages, "
            f"got {metrics.total_messages}"
        )
        assert metrics.invalid_packets == 0, (
            f"{metrics.invalid_packets} invalid packet(s)"
        )

        print(
            f"multi_client completed: all 9 events observed from "
            f"{len(RANKS)} ranks",
            flush=True,
        )
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        metrics.stop()


def test_multi_client_all_events():
    """The C multi-threaded client drives every event to every rank."""
    client = _find_client()
    if client is None:
        import pytest

        pytest.skip(SKIP_MESSAGE)
    _run(client)


def main() -> int:
    client = _find_client()
    if client is None:
        print(f"SKIP: {SKIP_MESSAGE}")
        return 0

    try:
        _run(client)
    except AssertionError as exc:
        print(f"FAILED: {exc}")
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
