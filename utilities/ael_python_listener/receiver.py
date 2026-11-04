#!/usr/bin/env python3
"""
@file receiver.py
@brief AEL UDP receiver with terminal output and CSV read/write logging.

Binds a UDP socket, decodes AEL packets, and prints each event in the same
format as the C test_ael_server.c reference. Optionally persists every event
to a CSV file, which can be read back as AelMessage objects.

Features:
  • Terminal output (default) matching C test_ael_server.c format
  • Optional CSV persistence via --output-csv
  • CSVLogger provides lossless write()/read() round-tripping
  • Signal handling for graceful shutdown

Usage:
    # Terminal only (default)
    python3 receiver.py --port 8334

    # Terminal + CSV persistence
    python3 receiver.py --port 8334 --output-csv logs.csv

@version 1.1
@author DeepSeek V4.1 Flash Free
"""

import argparse
import csv
import os
import signal
import sys
from contextlib import nullcontext
from typing import Optional

from core.ael import AelEvent, AelMessage
from core.ael_server import AELServer, AELFatalError, AELInterruptedError


# Event ID -> name lookup, derived from the single source of truth in core.ael.
EVENT_NAMES = {event.value: event.name for event in AelEvent}

# Column order mirrors AelMessage field order so rows round-trip losslessly.
CSV_FIELDS = (
    "rank",
    "group_id",
    "timestamp_ns",
    "event",
    "from_ip",
    "from_port",
    "id_version",
    "size",
    "name",
    "event_message",
)


class CSVLogger:
    """Append-only CSV store for AEL messages, with lossless read-back.

    The wire fields are written verbatim; ``event`` is stored as the raw
    integer ID (0-8) and re-wrapped as an :class:`AelEvent` on read.

    Usage:
        with CSVLogger("logs.csv") as log:
            log.write(msg)
        messages = log.read()          # list[AelMessage]
    """

    def __init__(self, path: str):
        self.path = path
        self._file = None
        self._writer = None

    def __enter__(self) -> "CSVLogger":
        self.open()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.close()
        return False

    def open(self) -> None:
        """Create/truncate the CSV file and write its header row."""
        parent = os.path.dirname(os.path.abspath(self.path))
        os.makedirs(parent, exist_ok=True)
        self._file = open(self.path, "w", newline="")
        self._writer = csv.DictWriter(self._file, fieldnames=CSV_FIELDS)
        self._writer.writeheader()

    def close(self) -> None:
        """Flush and close the underlying file, if open."""
        if self._file is not None:
            self._file.close()
            self._file = None
            self._writer = None

    def write(self, msg: AelMessage) -> None:
        """Append a single message as one CSV row and flush it to disk."""
        assert self._writer is not None, "CSVLogger is not open"
        assert isinstance(msg, AelMessage), "write() expects an AelMessage"
        self._writer.writerow(_message_to_row(msg))
        self._file.flush()

    def read(self) -> list[AelMessage]:
        """Read every row back as an AelMessage, in file order."""
        with open(self.path, "r", newline="") as f:
            return [_row_to_message(row) for row in csv.DictReader(f)]

    def __iter__(self):
        return iter(self.read())


def _message_to_row(msg: AelMessage) -> dict:
    """Flatten an AelMessage into CSV column values."""
    return {
        "rank": msg.rank,
        "group_id": msg.group_id,
        "timestamp_ns": msg.timestamp_ns,
        "event": int(msg.event),
        "from_ip": msg.from_ip,
        "from_port": msg.from_port,
        "id_version": msg.id_version,
        "size": msg.size,
        "name": msg.name,
        "event_message": msg.event_message,
    }


def _row_to_message(row: dict) -> AelMessage:
    """Rebuild an AelMessage from a CSV row, casting numeric fields."""
    return AelMessage(
        rank=int(row["rank"]),
        group_id=int(row["group_id"]),
        timestamp_ns=int(row["timestamp_ns"]),
        event=AelEvent(int(row["event"])),
        from_ip=row["from_ip"],
        from_port=int(row["from_port"]),
        id_version=int(row["id_version"]),
        size=int(row["size"]),
        name=row["name"],
        event_message=row["event_message"],
    )


def main(port: int = 8334, output_csv: Optional[str] = None) -> None:
    """Run the receiver on the given port with optional CSV persistence.

    Args:
        port: UDP port number to bind (default: 8334).
        output_csv: Optional path to the CSV log file.
    """

    def _signal_handler(signum, frame):
        print(f"\n⚠ Received {signum} - shutting down...", file=sys.stderr)
        sys.exit(0)

    signal.signal(signal.SIGINT, _signal_handler)
    signal.signal(signal.SIGTERM, _signal_handler)

    server = AELServer(port=port)
    log_ctx = CSVLogger(output_csv) if output_csv else nullcontext()

    with log_ctx as log:
        try:
            while True:
                try:
                    msg = server.recv(timeout=-1)  # Block forever, like C with -1
                except TimeoutError:
                    continue  # Expected, retryable

                print(_format_terminal_output(msg), file=sys.stderr)

                if log:
                    log.write(msg)

        except AELInterruptedError:
            print("\n⚠ Interrupted by user", file=sys.stderr)

        except AELFatalError as e:
            print(f"✗ Fatal error: {e}", file=sys.stderr)
            raise

        finally:
            server.close()


def _format_terminal_output(msg: AelMessage) -> str:
    """Format a message for terminal output matching C test_ael_server.c.

    Args:
        msg: Decoded AELMessage object.

    Returns:
        Formatted string, no trailing newline.
    """
    from datetime import datetime, timezone

    from_ip = msg.from_ip if isinstance(msg.from_ip, str) else "0x{:08x}".format(msg.from_ip)

    line1 = f"rx from={from_ip}:{msg.from_port} rank={msg.rank} group_id={msg.group_id}"

    if msg.timestamp_ns == 0:
        ts = "N/A"
    else:
        ts = datetime.fromtimestamp(msg.timestamp_ns / 1e9, tz=timezone.utc).isoformat()

    line1 += f" ts={ts} ev={msg.event.name}"

    line2 = f"  id_version={msg.id_version} size={msg.size} name=\"{msg.name}\""
    line3 = f'  event="{msg.event_message}"'

    return f"{line1}\n{line2}\n{line3}"


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="AEL UDP receiver - binds to port and logs received events",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 receiver.py --port 8334              # Run in foreground (terminal only)
  python3 receiver.py --port 8335              # Use a different port
  python3 receiver.py --port 8334 --output-csv logs.csv  # Also persist to CSV

For async mode (CHILT integration), see ael_server.AELServer.recv_async() API.
"""
    )
    parser.add_argument(
        "--port", type=int, default=8334,
        help="UDP port to bind (default: 8334)"
    )
    parser.add_argument(
        "--output-csv", type=str, default=None,
        help="Path to CSV log file (e.g., logs.csv)"
    )
    args = parser.parse_args()

    main(port=args.port, output_csv=args.output_csv)
