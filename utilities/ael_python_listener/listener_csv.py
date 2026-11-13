#  @file listener_csv.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief CSV sink for received AEL messages
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026
#
#  Python port of utilities/ael_listener/src/listener_csv.c. Both listeners
#  write byte identical files, so the rows are built by hand rather than with
#  the csv module (whose quoting rules differ slightly).

import os

from ael import AelMessage

CSV_COLUMNS = [
    "origin", "event_id", "gid", "rank", "timestamp", "id_version", "size",
    "name", "from_ip", "from_port", "event_message",
]

_METACHARS = (b",", b'"', b"\r", b"\n")


def _field(raw: bytes) -> bytes:
    """Quote a string field when it holds CSV metacharacters.

    Message strings come off the network and are untrusted, so any comma,
    quote, or line break must not be able to break the row structure.
    """
    if not any(c in raw for c in _METACHARS):
        return raw
    return b'"' + raw.replace(b'"', b'""') + b'"'


def format_row(msg: AelMessage) -> bytes:
    """One received message as a CSV row, including the line break."""
    numbers = (f"{msg.origin.label},{msg.event.label},{msg.group_id},"
               f"{msg.rank},{msg.timestamp},{msg.id_version},{msg.size},")
    sender = f",{msg.from_ip},{msg.from_port},"
    return (numbers.encode() + _field(msg.name_bytes) + sender.encode()
            + _field(msg.event_message_bytes) + b"\n")


class CsvSink:
    """Appends received messages to a CSV file.

    The header row is written only when the file is new or empty, so repeated
    runs append to one table. Every row is flushed as it is written.
    """

    def __init__(self, path: str):
        self.path = path
        self.rows = 0
        self._fp = open(path, "ab")
        try:
            if os.fstat(self._fp.fileno()).st_size == 0:
                self._fp.write(",".join(CSV_COLUMNS).encode() + b"\n")
                self._fp.flush()
        except OSError:
            self._fp.close()
            raise

    def write(self, msg: AelMessage) -> None:
        self._fp.write(format_row(msg))
        self._fp.flush()
        self.rows += 1

    def close(self) -> None:
        self._fp.close()

    def __enter__(self) -> "CsvSink":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        self.close()
        return False
