#  @file listener_group.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief Group level aggregation of per-rank AEL events
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026
#
#  Python port of utilities/ael_listener/src/listener_group.c; the two must
#  behave the same.
#
#  A group is identified by (origin, group_id). Every received event is
#  attributed to a group event, keyed by event code plus, for checkpoint,
#  restore, and region events, the payload id_version (checkpoint version or
#  region ID). Lifecycle events (connect, failure, disconnect) use one key per
#  group. Each member is counted once per group event, so duplicate datagrams
#  do not advance the count. A member is identified by its rank and sending
#  endpoint (IP and port): the rank alone is not unique, because every ARE
#  instance of a group reports rank 0.
#
#  A group can run more than once. A connect from a new member starts a new
#  run, and clears the group's events, once the previous run has ended: a
#  member disconnected or failed, or every member had connected.
#
#  The group size is learned from the payload size field of connect and
#  failure events; a size of 0 there is a sender fault and is logged and
#  ignored. Until a valid size is received, a group event can report its first
#  rank but never completes, and a run that ends without any disconnect or
#  failure is not recognised as ended.

import enum
import logging
from dataclasses import dataclass, field

from ael import AelEvent, AelMessage, AelOrigin

_logger = logging.getLogger("ael-listener")


class GroupState(enum.Flag):
    """Result of GroupTable.record: zero or more flags."""
    OK = 0
    # This rank is the first to report the group event
    FIRST = enum.auto()
    # This rank completed the group event (count reached group size)
    LAST = enum.auto()
    # This member (rank and sending endpoint) had already reported the group
    # event
    DUPLICATE = enum.auto()
    # The event is counted per id_version (checkpoint version or region ID)
    # rather than once per group
    KEYED = enum.auto()


@dataclass
class _Tracker:
    """One group level event and the distinct members that sent it."""
    members: set = field(default_factory=set)
    completed: bool = False


@dataclass
class _Group:
    size: int = 0
    trackers: dict = field(default_factory=dict)


def _event_key(event: AelEvent, id_version: int) -> int:
    """Lifecycle events use one key per group, all others are counted per
    payload id_version."""
    return 0 if event.is_lifecycle else id_version


def _member(msg: AelMessage) -> tuple:
    return (msg.rank, msg.from_ip, msg.from_port)


class GroupTable:
    """Tracks the group events of every (origin, group_id) seen."""

    def __init__(self):
        self._groups: dict[tuple, _Group] = {}

    def __len__(self) -> int:
        """Number of distinct groups seen."""
        return len(self._groups)

    @staticmethod
    def _starts_new_run(group: _Group, msg: AelMessage) -> bool:
        """Whether a connect message starts a new run of its group: the
        sender is new to the current run and that run has already ended."""
        if msg.event != AelEvent.CLI_CONNECT:
            return False

        connect = group.trackers.get((AelEvent.CLI_CONNECT, 0))
        if connect and _member(msg) in connect.members:
            return False
        if connect and connect.completed:
            return True

        for end in (AelEvent.CLI_DISCONNECT, AelEvent.CLI_FAILURE):
            tracker = group.trackers.get((end, 0))
            if tracker and tracker.members:
                return True
        return False

    def record(self, msg: AelMessage) -> GroupState:
        """Attribute a received message to its group event."""
        group = self._groups.setdefault((msg.origin, msg.group_id), _Group())
        who = f"Group {msg.origin.label}:{msg.group_id}"

        if self._starts_new_run(group, msg):
            _logger.debug(f"{who} new run (rank {msg.rank})")
            # The group identity and size are kept
            group.trackers.clear()

        # Connect and failure payloads carry the group size; 0 is a sender
        # fault
        if msg.event in (AelEvent.CLI_CONNECT, AelEvent.CLI_FAILURE):
            if msg.size == 0:
                _logger.warning(f"{who} {msg.event.label} with size 0 "
                                f"(rank {msg.rank})")
            elif msg.size != group.size:
                if group.size != 0:
                    _logger.warning(f"{who} size changed {group.size} -> "
                                    f"{msg.size} (rank {msg.rank})")
                group.size = msg.size

        state = GroupState.OK if msg.event.is_lifecycle else GroupState.KEYED
        key = (msg.event, _event_key(msg.event, msg.id_version))
        tracker = group.trackers.setdefault(key, _Tracker())

        member = _member(msg)
        if member in tracker.members:
            return state | GroupState.DUPLICATE
        tracker.members.add(member)

        if len(tracker.members) == 1:
            state |= GroupState.FIRST
        if (not tracker.completed and group.size > 0
                and len(tracker.members) >= group.size):
            tracker.completed = True
            state |= GroupState.LAST
        return state

    def size(self, origin: AelOrigin, group_id: int) -> int:
        """Group size from the group's connect/failure payloads; 0 when the
        group is unknown or no valid connect/failure has been received."""
        group = self._groups.get((origin, group_id))
        return group.size if group else 0

    def ranks_done(self, origin: AelOrigin, group_id: int, event: AelEvent,
                   id_version: int = 0) -> int:
        """Number of distinct members that have reported a group event in the
        group's current run. id_version is ignored for lifecycle events."""
        group = self._groups.get((origin, group_id))
        if not group:
            return 0
        tracker = group.trackers.get((event, _event_key(event, id_version)))
        return len(tracker.members) if tracker else 0
