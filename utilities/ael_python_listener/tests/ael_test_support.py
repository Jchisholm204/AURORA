#!/usr/bin/env python3
"""
@file ael_test_support.py
@brief Shared test support for AEL metrics testing.

Contains:
- EVENT_LABELS: Human-readable labels for each event ID
- rank_getter: Helper to get rank-checking function for a given event
- wait_for_all: Wait until all ranks observe an event (with progress output)
- wait_for_total: Wait until total message count is reached
- observe_all_events: Wait for all events from all ranks

These are client-agnostic and work with any sender that emits the 9 AEL events.
"""

from core.ael_metrics import AELMetrics
from core.ael import AelEvent
from typing import Callable, FrozenSet


# Human-readable labels for each event (matches multi_client.c order)
EVENT_LABELS = [
    "CLI connect",
    "CLI failure", 
    "CLI disconnect",
    "CLI accept",
    "CHECKPOINT start",
    "CHECKPOINT end",
    "CHECKPOINT restore-start",
    "CHECKPOINT restore-end",
    "REGION add",
]


def rank_getter(metrics: AELMetrics, label: str) -> Callable[[FrozenSet[int]], set]:
    """
    Return a getter function that checks how many distinct ranks observed this event.
    
    Args:
        metrics: AELMetrics instance to query
        label: Human-readable event label (used for progress output)
        
    Returns:
        Function taking a target rank set and returning the union of ranks that
        have observed the event, or None if no such function exists for this event.
    """
    # CLI events use group_id == 0 with any event <= AelEvent.CLI_END
    if label in ("CLI connect", "CLI failure", "CLI disconnect", "CLI accept"):
        target_event = AelEvent.CLI_CONNECT
        return lambda ranks: metrics.get_cli(
            event=AelEvent.CLI_CONNECT,
            version=None,  # implicit-last
        ) & ranks or set()
    
    # Region events use group_id == 2 with any region category event
    if label in ("REGION add",):
        return lambda ranks: metrics.ranks_region_add() & ranks or set()
    
    # Checkpoint events use group_id == 1 with checkpoint category events
    if label in ("CHECKPOINT start", "CHECKPOINT end", "CHECKPOINT restore-start", 
                  "CHECKPOINT restore-end"):
        target_event = AelEvent.CHECKPOINT_START
        return lambda ranks: metrics.ranks_checkpoint(
            started=True,
            version=None,  # implicit-last
        ) & ranks or set()
    
    return None


def wait_for_all(label: str, getter: Callable[[FrozenSet[int]], set], 
                 expected: FrozenSet[int], timeout: float = 30.0) -> None:
    """
    Wait until all ranks in `expected` have observed the event.
    
    Prints progress updates during the wait.
    
    Args:
        label: Human-readable event label (for progress output)
        getter: Function returning the set of ranks that have observed this event
        expected: Set of ranks we expect to see
        timeout: Seconds to wait before raising TimeoutError
    """
    import time
    
    start_time = time.monotonic()
    while time.monotonic() - start_time < timeout:
        observed = getter(expected)
        
        if expected.issubset(observed):
            return
        
        # Show progress
        missing = expected - observed
        print(f"  waiting for {label} ({', '.join(map(str, missing))}...)", 
              file=__import__('sys').stderr)
        time.sleep(0.5)
    
    raise TimeoutError(f"Timeout waiting for {label}")


def wait_for_total(metrics: AELMetrics, expected_total: int, timeout: float = 30.0) -> None:
    """
    Wait until total message count reaches expected_total.
    
    Args:
        metrics: AELMetrics instance
        expected_total: Expected total message count
        timeout: Seconds to wait
    """
    import time
    
    start_time = time.monotonic()
    while time.monotonic() - start_time < timeout:
        actual = metrics.total_messages
        
        if actual >= expected_total:
            return
        
        print(f"  waiting for {expected_total} messages (got {actual}...)", 
              file=__import__('sys').stderr)
        time.sleep(0.5)
    
    raise TimeoutError(f"Timeout waiting for {expected_total} messages")


def observe_all_events(metrics: AELMetrics, client, 
                       events=None, timeout: float = 30.0) -> None:
    """
    Wait for all events from all ranks in the client.
    
    Client-agnostic: works with any sender that emits the 9 AEL events
    from ranks 0..N-1.
    
    Args:
        metrics: AELMetrics instance
        client: Client object with .ranks attribute (frozenset or set of ints)
        events: Optional tuple of event IDs to wait for (default: all 9 events)
        timeout: Seconds to wait per event
    """
    if events is None:
        events = range(9)
    
    # Use implicit-last for each event category
    for event_id in events:
        label = EVENT_LABELS[event_id]
        
        # Choose the appropriate getter based on event category
        if 0 <= event_id <= 2:  # CLI events
            ranks_observed = metrics.get_cli(event=AelEvent.CLI_CONNECT, version=None)
        elif 3 <= event_id <= 8:  # Region and checkpoint events
            if event_id == 7:  # REGION ADD (event 7)
                ranks_observed = metrics.ranks_region_add()
            else:  # Checkpoint events
                ranks_observed = metrics.ranks_checkpoint(started=True, version=None)
        else:
            raise ValueError(f"Unknown event ID: {event_id}")
        
        wait_for_all(label, lambda r=ranks_observed: r, 
                     expected=client.ranks, timeout=timeout)
