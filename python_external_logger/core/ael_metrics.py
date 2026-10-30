#!/usr/bin/env python3
"""
@file ael_metrics.py
@brief Background AEL server with per-category history and per-rank counts.

Wraps AELServer in a thread-owned asyncio loop and records every decoded
message twice:

  * in one of three ordered lists (CLI, checkpoint, region) so callers can
    inspect history and ask for the most recently received message, and
  * in an identity -> rank-set map so simple "how many ranks?" queries are
    O(1).

Message identity excludes the fields that differ per sender:

    rank          the value being counted
    timestamp_ns  wall clock, differs per rank
    from_ip       source address
    from_port     source port
    event_message free-form text, differs per rank for CLI events

Everything else (event, group_id, id_version, size, name) forms the identity.
Ranks are stored in a set, so a rank that retransmits a message is counted
once.

Queries are implicit-last: when neither a version/id nor a name is supplied,
the most recently received matching message defines the identity and the
returned count is the number of ranks that sent that identity. Supplying a
version/id and/or name switches to a cumulative count over every matching
message, which is how "all ranks started version 1" is expressed.

Usage:
    from core.ael import AelEvent
    from core.ael_metrics import AELMetrics

    with AELMetrics(port=8334) as metrics:
        ...
        n = metrics.get_checkpoint(AelEvent.CHECKPOINT_START)
        n = metrics.get_checkpoint(AelEvent.CHECKPOINT_START, version=1,
                                   name="ckpt-start")
        n = metrics.get_region(AelEvent.REGION_ADD, id=42, name="rgn-42")
        msg = metrics.last_checkpoint(AelEvent.CHECKPOINT_START)

@date Created: 2026-10-30
@modified Last Modified: 2026-10-30

@copyright Copyright (c) 2026
@version 0.2
@author DeepSeek V4.1 Flash Free
"""

import asyncio
import enum
import threading
from typing import Optional

from .ael import AELErrors, AelEvent, AelMessage
from .ael_server import AELServer, AELFatalError, AELInterruptedError


# (event, group_id, id_version, size, name) -> ranks that sent it
MessageIdentity = tuple[int, int, int, int, str]


class AelCategory(enum.Enum):
    """Top-level message groups, each backed by its own ordered list."""

    CLI = "cli"
    CHECKPOINT = "checkpoint"
    REGION = "region"


def _identity(msg: AelMessage) -> MessageIdentity:
    """Build the per-message identity, deliberately excluding sender fields."""
    return (
        int(msg.event),
        msg.group_id,
        msg.id_version,
        msg.size,
        msg.name,
    )


def _category_for(event: AelEvent) -> AelCategory:
    """Map an event code onto its message group."""
    event = AelEvent(event)
    if event <= AelEvent.CLI_DISCONNECT:
        return AelCategory.CLI
    if event <= AelEvent.RESTORE_END:
        return AelCategory.CHECKPOINT
    return AelCategory.REGION


class AELMetrics:
    """Background AEL receiver with per-category history and rank counts.

    A daemon thread owns a private asyncio event loop running
    AELServer.recv_async(). Every decoded message is appended to its
    category list (oldest first) and folded into ``_counts`` under a lock.

    Args:
        port: UDP port to bind.
        recv_interval: How long each background receive waits before checking
            the stop flag (seconds). Also bounds stop() latency.
    """

    def __init__(self, port: int = 8334, *, recv_interval: float = 0.1):
        self.port = port
        self.recv_interval = recv_interval

        # Newest message is appended at the end; messages() reverses for callers.
        self._cli: list[AelMessage] = []
        self._checkpoints: list[AelMessage] = []
        self._regions: list[AelMessage] = []
        self._counts: dict[MessageIdentity, set[int]] = {}

        self._lock = threading.Lock()
        self._total = 0
        self._invalid = 0

        self._thread: Optional[threading.Thread] = None
        self._ready = threading.Event()
        self._stop = threading.Event()
        self._error: Optional[BaseException] = None

    # ------------------------------------------------------------------ life

    def start(self) -> None:
        """Start receiving in a background daemon thread.

        Blocks until the socket is bound so a bind failure surfaces here as
        AELFatalError instead of dying silently in the thread.
        """
        if self.running:
            return

        self._stop.clear()
        self._ready.clear()
        self._error = None

        self._thread = threading.Thread(
            target=self._thread_main,
            name=f"ael-metrics-{self.port}",
            daemon=True,
        )
        self._thread.start()

        if not self._ready.wait(timeout=5.0):
            raise AELFatalError("Timed out starting AELMetrics receive loop")
        if self._error is not None:
            error = self._error
            self._thread = None
            raise AELFatalError(f"Failed to start AELMetrics: {error}") from error

    def stop(self) -> None:
        """Stop the background loop and close the socket. Idempotent."""
        thread = self._thread
        if thread is None:
            return

        self._stop.set()
        thread.join(timeout=max(1.0, self.recv_interval * 10))
        self._thread = None

    @property
    def running(self) -> bool:
        """True while the background thread is alive."""
        thread = self._thread
        return thread is not None and thread.is_alive()

    def __enter__(self) -> "AELMetrics":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.stop()
        return False

    def _thread_main(self) -> None:
        loop = asyncio.new_event_loop()
        try:
            asyncio.set_event_loop(loop)
            loop.run_until_complete(self._serve())
        except BaseException as e:  # surfaced by start() via _error
            self._error = e
        finally:
            try:
                loop.run_until_complete(loop.shutdown_asyncgens())
            finally:
                loop.close()
                asyncio.set_event_loop(None)

    async def _serve(self) -> None:
        server = AELServer(self.port)
        try:
            server.open()
        except BaseException as e:
            self._error = e
            self._ready.set()
            return

        self._ready.set()
        try:
            while not self._stop.is_set():
                try:
                    msg = await server.recv_async(timeout=self.recv_interval)
                except TimeoutError:
                    continue
                except AELInterruptedError:
                    break
                except AELErrors:
                    with self._lock:
                        self._invalid += 1
                    continue
                except OSError:
                    break  # socket closed during shutdown

                self._record(msg)
        finally:
            server.close()

    def _record(self, msg: AelMessage) -> None:
        category = _category_for(msg.event)
        with self._lock:
            self._total += 1
            self._list_for(category).append(msg)
            self._counts.setdefault(_identity(msg), set()).add(msg.rank)

    # -------------------------------------------------------------- internals

    def _list_for(self, category: AelCategory) -> list[AelMessage]:
        if category is AelCategory.CLI:
            return self._cli
        if category is AelCategory.CHECKPOINT:
            return self._checkpoints
        if category is AelCategory.REGION:
            return self._regions
        raise ValueError(f"Unknown AEL category: {category!r}")

    def _resolve(
        self,
        category: Optional[AelCategory],
        event: Optional[AelEvent],
    ) -> tuple[Optional[AelCategory], Optional[AelEvent]]:
        """Normalize category/event and ensure they agree."""
        if category is not None and not isinstance(category, AelCategory):
            category = AelCategory(category)

        if event is None:
            return category, None

        event = AelEvent(event)
        inferred = _category_for(event)
        if category is not None and category is not inferred:
            raise ValueError(
                f"Event {event.name} belongs to {inferred.value}, "
                f"not {category.value}"
            )
        return inferred, event

    def _newest_locked(
        self,
        category: AelCategory,
        event: Optional[AelEvent],
        group_id: Optional[int],
        size: Optional[int],
    ) -> Optional[AelMessage]:
        """Return the most recently received matching message."""
        for msg in reversed(self._list_for(category)):
            if event is not None and int(msg.event) != int(event):
                continue
            if group_id is not None and msg.group_id != group_id:
                continue
            if size is not None and msg.size != size:
                continue
            return msg
        return None

    def _rebuild_counts_locked(self) -> None:
        self._counts.clear()
        for messages in (self._cli, self._checkpoints, self._regions):
            for msg in messages:
                self._counts.setdefault(_identity(msg), set()).add(msg.rank)

    # ---------------------------------------------------------------- queries

    def get(
        self,
        category: Optional[AelCategory] = None,
        event: Optional[AelEvent] = None,
        *,
        group_id: Optional[int] = None,
        id_version: Optional[int] = None,
        size: Optional[int] = None,
        name: Optional[str] = None,
    ) -> int:
        """Return the number of distinct ranks matching a message query.

        Implicit-last: if neither ``id_version`` nor ``name`` is given, the
        newest matching message defines the identity and only ranks that sent
        that identity are counted. Supplying either filter switches to a
        cumulative count over every matching message.

        Args:
            category: Message group to search (inferred from ``event`` if None).
            event: Event type to match (None = any event in the group).
            group_id: Group ID to match (None = any).
            id_version: Checkpoint version or region ID (None = any).
            size: Payload size to match (None = any).
            name: Region/checkpoint name, exact match (None = any).

        Returns:
            Count of distinct ranks in the selected message set.
        """
        return len(
            self.ranks(
                category,
                event,
                group_id=group_id,
                id_version=id_version,
                size=size,
                name=name,
            )
        )

    def ranks(
        self,
        category: Optional[AelCategory] = None,
        event: Optional[AelEvent] = None,
        *,
        group_id: Optional[int] = None,
        id_version: Optional[int] = None,
        size: Optional[int] = None,
        name: Optional[str] = None,
    ) -> set[int]:
        """Return the set of distinct ranks matching a message query.

        Same filters and implicit-last rule as :meth:`get`. The returned set
        is a copy and is safe to mutate.
        """
        category, event = self._resolve(category, event)

        if id_version is None and name is None:
            if category is None:
                raise ValueError(
                    "Implicit-last queries need a category or event; "
                    "pass id_version/name for a cross-category query"
                )
            with self._lock:
                ref = self._newest_locked(category, event, group_id, size)
                if ref is None:
                    return set()
                return set(self._counts.get(_identity(ref), ()))

        result: set[int] = set()
        with self._lock:
            for identity, ranks in self._counts.items():
                if (
                    category is not None
                    and _category_for(identity[0]) is not category
                ):
                    continue
                if event is not None and identity[0] != int(event):
                    continue
                if group_id is not None and identity[1] != group_id:
                    continue
                if id_version is not None and identity[2] != id_version:
                    continue
                if size is not None and identity[3] != size:
                    continue
                if name is not None and identity[4] != name:
                    continue
                result |= ranks

        return result

    # ---------------------------------------------------------------- history

    def messages(
        self,
        category: AelCategory,
        event: Optional[AelEvent] = None,
        *,
        limit: Optional[int] = None,
    ) -> list[AelMessage]:
        """Return matching messages, newest first.

        The list is a snapshot copy, so it is safe to iterate while the
        background thread keeps receiving.
        """
        category, event = self._resolve(category, event)
        if category is None:
            raise ValueError("messages() needs a category or event")

        with self._lock:
            messages = [
                msg
                for msg in self._list_for(category)
                if event is None or int(msg.event) == int(event)
            ]

        messages.reverse()
        if limit is not None:
            messages = messages[:limit]
        return messages

    def last(
        self,
        category: Optional[AelCategory] = None,
        event: Optional[AelEvent] = None,
        *,
        group_id: Optional[int] = None,
        id_version: Optional[int] = None,
        size: Optional[int] = None,
        name: Optional[str] = None,
    ) -> Optional[AelMessage]:
        """Return the most recently received matching message, or None."""
        category, event = self._resolve(category, event)
        if category is None:
            raise ValueError("last() needs a category or event")

        with self._lock:
            for msg in reversed(self._list_for(category)):
                if event is not None and int(msg.event) != int(event):
                    continue
                if group_id is not None and msg.group_id != group_id:
                    continue
                if id_version is not None and msg.id_version != id_version:
                    continue
                if size is not None and msg.size != size:
                    continue
                if name is not None and msg.name != name:
                    continue
                return msg
        return None

    # ---------------------------------------------------------- group helpers

    def get_cli(self, event: Optional[AelEvent] = None) -> int:
        """Count ranks for a CLI event (implicit-last)."""
        return self.get(AelCategory.CLI, event)

    def get_checkpoint(
        self,
        event: Optional[AelEvent] = None,
        version: Optional[int] = None,
        name: Optional[str] = None,
    ) -> int:
        """Count ranks for a checkpoint/restore event.

        With no ``version``/``name`` the latest checkpoint identity is used.
        """
        return self.get(
            AelCategory.CHECKPOINT,
            event,
            id_version=version,
            name=name,
        )

    def get_region(
        self,
        event: Optional[AelEvent] = None,
        id: Optional[int] = None,
        name: Optional[str] = None,
    ) -> int:
        """Count ranks for a region event.

        With no ``id``/``name`` the latest region identity is used.
        """
        return self.get(AelCategory.REGION, event, id_version=id, name=name)

    def ranks_cli(self, event: Optional[AelEvent] = None) -> set[int]:
        """Set of ranks for a CLI event (implicit-last)."""
        return self.ranks(AelCategory.CLI, event)

    def ranks_checkpoint(
        self,
        event: Optional[AelEvent] = None,
        version: Optional[int] = None,
        name: Optional[str] = None,
    ) -> set[int]:
        """Set of ranks for a checkpoint/restore event."""
        return self.ranks(
            AelCategory.CHECKPOINT,
            event,
            id_version=version,
            name=name,
        )

    def ranks_region(
        self,
        event: Optional[AelEvent] = None,
        id: Optional[int] = None,
        name: Optional[str] = None,
    ) -> set[int]:
        """Set of ranks for a region event."""
        return self.ranks(AelCategory.REGION, event, id_version=id, name=name)

    def last_cli(self, event: Optional[AelEvent] = None) -> Optional[AelMessage]:
        """Most recently received CLI message."""
        return self.last(AelCategory.CLI, event)

    def last_checkpoint(
        self,
        event: Optional[AelEvent] = None,
        version: Optional[int] = None,
        name: Optional[str] = None,
    ) -> Optional[AelMessage]:
        """Most recently received checkpoint/restore message."""
        return self.last(
            AelCategory.CHECKPOINT,
            event,
            id_version=version,
            name=name,
        )

    def last_region(
        self,
        event: Optional[AelEvent] = None,
        id: Optional[int] = None,
        name: Optional[str] = None,
    ) -> Optional[AelMessage]:
        """Most recently received region message."""
        return self.last(AelCategory.REGION, event, id_version=id, name=name)

    # ------------------------------------------------------------- management

    def clear(
        self,
        category: Optional[AelCategory] = None,
        event: Optional[AelEvent] = None,
    ) -> None:
        """Drop matching history and counts.

        With no arguments all three message groups are emptied. This is the
        only reset path; queries themselves never consume state.
        """
        category, event = self._resolve(category, event)

        with self._lock:
            if category is None:
                self._cli.clear()
                self._checkpoints.clear()
                self._regions.clear()
            elif event is None:
                self._list_for(category).clear()
            else:
                messages = self._list_for(category)
                messages[:] = [
                    msg for msg in messages if int(msg.event) != int(event)
                ]
            self._rebuild_counts_locked()

    @property
    def total_messages(self) -> int:
        """Number of valid messages received since start (including repeats)."""
        with self._lock:
            return self._total

    @property
    def invalid_packets(self) -> int:
        """Number of datagrams rejected during decode (CRC, size, event code)."""
        with self._lock:
            return self._invalid


__all__ = ['AELMetrics', 'AelCategory']
