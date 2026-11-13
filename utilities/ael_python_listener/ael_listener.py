#!/usr/bin/env python3
#  @file ael_listener.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief AEL aggregate listener: logs AEL events to stdout and a CSV
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026
#
#  Python port of utilities/ael_listener/src/main.c (AURORA #100, #101).
#
#  Usage: ael_listener.py [PORT [LEVEL [CSV_LOCATION]]]

import enum
import logging
import signal
import sys

import ael
from ael import AelEvent, AelMessage
from listener_csv import CsvSink
from listener_group import GroupState, GroupTable

_logger = logging.getLogger("ael-listener")

# How often a blocked receive wakes up to check for a stop request
_POLL_SECONDS = 0.25


class Level(enum.IntEnum):
    """Logging level; ordered so each level includes the group output of the
    levels below it. Does not affect the CSV."""
    # Internal listener events only
    NONE = 0
    # Group connect/disconnect
    GROUP_LIGHT = 1
    # All group events
    GROUP_ALL = 2
    # group-all, plus per-rank connect/disconnect/failure
    LIGHT = 3
    # group-all, plus every per-rank event
    ALL = 4

    @property
    def label(self) -> str:
        return self.name.lower().replace("_", "-")

    @classmethod
    def parse(cls, text: str) -> "Level":
        for level in cls:
            if level.label == text:
                return level
        raise ValueError(f"Invalid LEVEL '{text}'")


DEFAULT_LEVEL = Level.GROUP_ALL


def _group_visible(level: Level, event: AelEvent) -> bool:
    if level >= Level.GROUP_ALL:
        return True
    if level == Level.GROUP_LIGHT:
        return event in (AelEvent.CLI_CONNECT, AelEvent.CLI_DISCONNECT)
    return False


def _rank_visible(level: Level, event: AelEvent) -> bool:
    if level == Level.ALL:
        return True
    if level == Level.LIGHT:
        return event.is_lifecycle
    return False


def _escape(raw: bytes) -> str:
    """Escape an untrusted string for terminal output: quotes, backslashes,
    and non-printable bytes, so one message stays one line."""
    out = []
    for c in raw:
        if c in (0x22, 0x5c):
            out.append("\\" + chr(c))
        elif c == 0x0a:
            out.append("\\n")
        elif c < 0x20 or c >= 0x7f:
            out.append(f"\\x{c:02x}")
        else:
            out.append(chr(c))
    return "".join(out)


def format_group(groups: GroupTable, msg: AelMessage,
                 state: GroupState) -> str | None:
    """Group level log line for a recorded message, or None when the message
    was neither the first nor the last of its group event."""
    if (GroupState.DUPLICATE in state
            or not state & (GroupState.FIRST | GroupState.LAST)):
        return None

    ident = f" id={msg.id_version}" if GroupState.KEYED in state else ""
    done = groups.ranks_done(msg.origin, msg.group_id, msg.event,
                             msg.id_version)
    size = groups.size(msg.origin, msg.group_id) or "?"
    phase = "complete" if GroupState.LAST in state else "first"
    return (f"[group {msg.origin.label}:{msg.group_id}] {msg.event.label}"
            f"{ident} {phase} ({done}/{size}) rank={msg.rank}")


def format_rank(msg: AelMessage) -> str:
    """Per-rank log line for a received message."""
    return (f"[rank {msg.origin.label}:{msg.group_id}:{msg.rank}] "
            f"{msg.event.label} id_version={msg.id_version} size={msg.size} "
            f'name="{_escape(msg.name_bytes)}" '
            f'msg="{_escape(msg.event_message_bytes)}" '
            f"from={msg.from_ip}:{msg.from_port} ts={msg.timestamp}")


class Listener:
    """Receives AEL messages, logs them, and appends them to a CSV.

    Usage:
        listener = Listener(port=8334, level=Level.ALL, csv_path="ael.csv")
        listener.run()          # until listener.stop() or a fatal error
    """

    def __init__(self, port: int = ael.AEL_DEFAULT_PORT,
                 level: Level = DEFAULT_LEVEL, csv_path: str | None = None,
                 host: str = "0.0.0.0"):
        self.level = level
        self.csv_path = csv_path
        self.groups = GroupTable()
        self.received = 0
        self.discarded = 0

        self._stop = False
        self._csv = CsvSink(csv_path) if csv_path else None
        try:
            self._rx = ael.AelReceiver(port, host)
        except OSError:
            if self._csv:
                self._csv.close()
            raise

    @property
    def port(self) -> int:
        return self._rx.port

    @property
    def csv_rows(self) -> int:
        return self._csv.rows if self._csv else 0

    def stop(self) -> None:
        """Ask run() to return (safe to call from a signal handler)."""
        self._stop = True

    def handle(self, msg: AelMessage) -> None:
        """Process one valid message: CSV first, then group and stdout."""
        self.received += 1
        if self._csv:
            self._csv.write(msg)

        state = self.groups.record(msg)
        if _rank_visible(self.level, msg.event):
            _logger.info(format_rank(msg))
        if _group_visible(self.level, msg.event):
            line = format_group(self.groups, msg, state)
            if line:
                _logger.info(line)

    def run(self) -> None:
        """Receive until stopped.

        Raises:
            OSError: the socket or the CSV failed.
        """
        _logger.info(f"AEL listener on UDP port {self.port}, "
                     f"level={self.level.label}, "
                     f"csv={self.csv_path or '(none)'} (Ctrl+C to stop)")
        while not self._stop:
            try:
                msg = self._rx.recv(_POLL_SECONDS)
            except ael.AelError as e:
                self.discarded += 1
                _logger.warning(f"Discarded AEL datagram: {e}")
                continue
            if msg is not None:
                self.handle(msg)

    def close(self) -> None:
        _logger.info(f"AEL listener stopped: received={self.received} "
                     f"discarded={self.discarded} csv_rows={self.csv_rows} "
                     f"groups={len(self.groups)}")
        self._rx.close()
        if self._csv:
            self._csv.close()


def _usage(prog: str) -> None:
    print(f"""Usage: {prog} [PORT [LEVEL [CSV_LOCATION]]]
  PORT          UDP port to receive AEL messages on (1..65535, default: {ael.AEL_DEFAULT_PORT})
  LEVEL         stdout logging level (default: {DEFAULT_LEVEL.label}):
                  none         internal listener events only
                  group-light  group connect/disconnect
                  group-all    all group events
                  light        group-all + per-rank connect/disconnect/failure
                  all          group-all + every per-rank event
  CSV_LOCATION  Append every received message to this CSV (optional; LEVEL does not filter it)""",
          file=sys.stderr)


def _parse_port(text: str) -> int:
    if not (text.isascii() and text.isdigit()) or not 1 <= int(text) <= 65535:
        raise ValueError(f"Invalid PORT '{text}' (must be 1..65535)")
    return int(text)


def _setup_logging() -> None:
    # Same line layout as the C listener (log.c): time, level, file:line
    logging.addLevelName(logging.WARNING, "WARN")
    logging.basicConfig(
        level=logging.INFO, stream=sys.stderr, datefmt="%H:%M:%S",
        format="%(asctime)s %(levelname)-5s %(filename)s:%(lineno)d: "
               "%(message)s")


def main(argv: list[str]) -> int:
    prog, args = argv[0], argv[1:]
    if args and args[0] in ("-h", "--help"):
        _usage(prog)
        return 0 if len(args) == 1 else 1
    if len(args) > 3:
        _usage(prog)
        return 1

    _setup_logging()
    try:
        port = _parse_port(args[0]) if len(args) > 0 else ael.AEL_DEFAULT_PORT
        level = Level.parse(args[1]) if len(args) > 1 else DEFAULT_LEVEL
    except ValueError as e:
        _logger.error(str(e))
        _usage(prog)
        return 1
    csv_path = args[2] if len(args) > 2 else None

    try:
        listener = Listener(port, level, csv_path)
    except OSError as e:
        _logger.error(f"Failed to start AEL listener: {e}")
        return 1

    signal.signal(signal.SIGINT, lambda *_: listener.stop())
    signal.signal(signal.SIGTERM, lambda *_: listener.stop())

    ret = 0
    try:
        listener.run()
    except OSError as e:
        _logger.error(f"AEL Error: {e}")
        ret = 1
    finally:
        listener.close()
    return ret


if __name__ == "__main__":
    sys.exit(main(sys.argv))
