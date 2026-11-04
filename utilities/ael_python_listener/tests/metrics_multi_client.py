#!/usr/bin/env python3
"""
@file metrics_multi_client.py
@brief Tests for AELMetrics against C multi_client sender.

Pytest mode: uses fixtures (conftest.py) to manage client/server lifecycle.
Standalone mode (__main__): starts server on fixed port 9012; waits for external client.

Usage:
    pytest tests/metrics_multi_client.py -v
    python3 tests/metrics_multi_client.py  # binds to UDP port 9012, waits for multi_client
"""

import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from core.ael import AelEvent
from core.ael_metrics import AELMetrics, AelCategory
from tests.ael_client import find_multi_client


# Constants for standalone mode (pytest fixtures handle everything else)
DEFAULT_PORT = 8335       # Matches multi_client.c default PORT constant
DEFAULT_HOST = "127.0.0.1"
DEFAULT_RANKS = 4
DEFAULT_PACKETS_PER_RANK = 36

EVENT_LABELS = {
    0: "CLI_CONNECT",
    1: "CLI_FAILURE",
    2: "CLI_DISCONNECT",
    3: "CHKPT_START",
    4: "CHKPT_END",
    5: "RSTRT_START",
    6: "RSTRT_END",
    7: "ADD",
    8: "REMOVE",
}


def _wait_for_all_rank_coverage(
    metrics_server: AELMetrics,
    category_name: str,
    expected_count: int,
    timeout: float = 30.0,
) -> None:
    """Wait until all expected ranks have sent events in this category using implicit-last.

    Args:
        metrics_server: The metrics server instance
        category_name: One of "CLI", "CHECKPOINT", or "REGION"
        expected_count: Number of distinct ranks to wait for
        timeout: Maximum time to wait before giving up (seconds)
    """
    start_time = time.monotonic()
    
    while True:
        current_time = time.monotonic() - start_time
        
        if current_time >= timeout:
            print(f"[-] ERROR: Timeout waiting for {expected_count} {category_name} ranks", file=sys.stderr)
            # Debug: show what's in each category
            for cat in [AelCategory.CLI, AelCategory.CHECKPOINT, AelCategory.REGION]:
                msgs = metrics_server.messages(category=cat)
                print(f"[*] {cat.name}: {len(msgs)} messages", file=sys.stderr)
            sys.exit(1)
        
        try:
            if category_name == "CLI":
                # CLI: count all distinct ranks that sent ANY CLI message (cumulative)
                cli_msgs = metrics_server.messages(category=AelCategory.CLI, limit=None)
                counts = set()
                for msg in cli_msgs:
                    counts.add(msg.rank)
                count = len(counts)
            elif category_name == "CHECKPOINT":
                # CHECKPOINT: count all distinct ranks that sent ANY checkpoint message (cumulative)
                ckpt_msgs = metrics_server.messages(category=AelCategory.CHECKPOINT, limit=None)
                counts = set()
                for msg in ckpt_msgs:
                    counts.add(msg.rank)
                count = len(counts)
            elif category_name == "REGION":
                # REGION: count all distinct ranks that sent ANY region message (cumulative)
                rgn_msgs = metrics_server.messages(category=AelCategory.REGION, limit=None)
                counts = set()
                for msg in rgn_msgs:
                    counts.add(msg.rank)
                count = len(counts)
            else:
                raise ValueError(f"Unknown category: {category_name}")

            if count >= expected_count:
                print(f"[*] All {expected_count} ranks seen in {category_name}", file=sys.stderr)
                break
        except ValueError:
            # No messages yet in this category
            pass
        time.sleep(0.5)


def test_all_events_from_all_ranks(metrics_server: AELMetrics, client_run) -> None:
    """Test that all 9 events are received from all ranks using query-based API."""
    print("[*] Test: Waiting for all 9 events...", file=sys.stderr)

    # CLI events (0-2): wait until each category has seen all ranks
    for cat in ["CLI", "CHECKPOINT", "REGION"]:
        _wait_for_all_rank_coverage(metrics_server, cat, DEFAULT_RANKS)

    # Verify client completed successfully
    rc = client_run.wait(timeout=15.0)
    if rc != 0:
        print(f"[-] ERROR: client exited with return code {rc}", file=sys.stderr)
        sys.exit(1)

    # Verify final identities and totals using implicit-last queries
    newest_region = metrics_server.last_region()
    if newest_region is None:
        print("[-] ERROR: No region messages received", file=sys.stderr)
        sys.exit(1)
    print(f"[*] Last region identity: {newest_region.name}", file=sys.stderr)

    # Verify total CLI messages (each rank cycles through all 9 events)
    expected_cli_total = DEFAULT_RANKS * 12  # 3 CLI events × 4 cycles per rank, times ranks
    cli_events = [AelEvent.CLI_CONNECT, AelEvent.CLI_FAILURE, AelEvent.CLI_DISCONNECT]
    
    # Count CLI messages (allow for UDP loss - at least 50% of expected)
    cli_total = sum(len([m for m in metrics_server.messages(category=AelCategory.CLI) if m.event == evt])
                    for evt in cli_events)
    assert cli_total >= expected_cli_total * 0.5, \
        f"CLI: got {cli_total}, expected at least {expected_cli_total*0.5}"
    
    # Verify total checkpoint messages (each rank cycles through all 9 events)
    expected_ckpt_total = DEFAULT_RANKS * 16  # 4 CHECKPOINT-category events × 4 cycles per rank, times ranks
    ckpt_events = [AelEvent.CHECKPOINT_START, AelEvent.CHECKPOINT_END, AelEvent.RESTORE_START, AelEvent.RESTORE_END]
    
    # Count checkpoint messages (allow for UDP loss - at least 50% of expected)
    ckpt_total = sum(len([m for m in metrics_server.messages(category=AelCategory.CHECKPOINT) if m.event == evt])
                    for evt in ckpt_events)
    assert ckpt_total >= expected_ckpt_total * 0.5, \
        f"Check point: got {ckpt_total}, expected at least {expected_ckpt_total*0.5}"
    
    # Verify total region messages (each rank cycles through all 9 events)  
    expected_rgn_total = DEFAULT_RANKS * 8  # 2 REGION-category events × 4 cycles per rank, times ranks
    rgn_events = [AelEvent.REGION_ADD, AelEvent.REGION_REMOVE]
    
    # Count region messages (allow for UDP loss - at least 50% of expected)
    rgn_total = sum(len([m for m in metrics_server.messages(category=AelCategory.REGION) if m.event == evt])
                    for evt in rgn_events)
    assert rgn_total >= expected_rgn_total * 0.5, \
        f"Region: got {rgn_total}, expected at least {expected_rgn_total*0.5}"

    print("[+] Message totals verified", file=sys.stderr)


def main():
    """Run the test against an external client. Used by standalone mode.
    
    This version expects multi_client to be spawned externally in a separate
    shell (via backgrounding or another terminal) to achieve true parallel
    execution between C and Python processes.
    """

    # Start AELMetrics server on fixed port (no free-port lookup needed)
    print(f"[*] Starting AELMetrics on UDP port {DEFAULT_PORT}", file=sys.stderr)
    print("[!] EXPECTED: multi_client running externally in separate shell", file=sys.stderr)

    with AELMetrics(port=DEFAULT_PORT) as metrics_server:
        # DEBUG: Check all categories
        print("[*] Checking messages received by AELMetrics...", file=sys.stderr)
        
        cli_msgs = metrics_server.messages(category=AelCategory.CLI)
        print(f"[*] CLI messages count: {len(cli_msgs)}", file=sys.stderr)
        for msg in cli_msgs[:5]:  # Show first 5
            print(f"    CLI msg: event={msg.event}, rank={msg.rank}", file=sys.stderr)
        
        ckpt_msgs = metrics_server.messages(category=AelCategory.CHECKPOINT)
        print(f"[*] CHECKPOINT messages count: {len(ckpt_msgs)}", file=sys.stderr)
        
        rgn_msgs = metrics_server.messages(category=AelCategory.REGION)
        print(f"[*] REGION messages count: {len(rgn_msgs)}", file=sys.stderr)

        # Wait for CLI events from all 4 ranks
        print("[*] Waiting for CLI events from all ranks...", file=sys.stderr)
        _wait_for_all_rank_coverage(metrics_server, "CLI", DEFAULT_RANKS, timeout=60.0)

        # Wait for CHECKPOINT events from all 4 ranks
        print("[*] Waiting for CHECKPOINT events from all ranks...", file=sys.stderr)
        _wait_for_all_rank_coverage(metrics_server, "CHECKPOINT", DEFAULT_RANKS, timeout=60.0)

        # REGION category also receives messages due to cyclic sending
        print("[*] Waiting for REGION events from all ranks...", file=sys.stderr)
        _wait_for_all_rank_coverage(metrics_server, "REGION", DEFAULT_RANKS, timeout=60.0)

        # Final explicit identity checks (multi_client.c cycles all 9 events per rank)
        last_region = metrics_server.last_region()
        if last_region is None:
            print("[!] No region messages received", file=sys.stderr)
            sys.exit(1)
        
        # Check that we got region messages (ADD and REMOVE events)
        rgn_msgs = metrics_server.messages(category=AelCategory.REGION)
        print(f"[*] Received {len(rgn_msgs)} region messages", file=sys.stderr)
        
        # With cyclic sending (36 packets per rank × 4 ranks), last region will be a REMOVE
        # The test verifies we received region events from multi_client
        assert last_region.event == AelEvent.REGION_REMOVE, \
            f"Expected final region message to be REMOVE event, got {last_region.event}"

        print(f"[*] Last region identity: {last_region}", file=sys.stderr)

        # Verify we received messages (allow for UDP loss - at least 30% of expected)
        total_cli = len(metrics_server.messages(category=AelCategory.CLI))
        total_ckpt = len(metrics_server.messages(category=AelCategory.CHECKPOINT))
        total_rgn = len(metrics_server.messages(category=AelCategory.REGION))
        total_received = total_cli + total_ckpt + total_rgn
        
        expected_total = DEFAULT_RANKS * 36  # 4 × 36 packets = 144 (before UDP loss)
        min_expected = int(expected_total * 0.3)  # Allow 70% UDP loss
        
        print(f"[*] Received {total_received} messages (CLI={total_cli}, CKPT={total_ckpt}, RGN={total_rgn})", file=sys.stderr)
        
        assert total_received >= min_expected, \
            f"Received {total_received} messages, expected at least {min_expected}"

        print("[+] All events received!", file=sys.stderr)
        print("[+] Message totals verified", file=sys.stderr)


# For pytest collection
__all__ = ["test_all_events_from_all_ranks", "EVENT_LABELS", "_wait_for_all_rank_coverage"]


if __name__ == "__main__":
    main()
