#  @file test_ael_listener.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief End to end tests of the Python AEL listener, and parity with the C
#         ael_listener
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026
#
#  The parity tests need the C listener. Build it (cmake target ael_listener)
#  and either leave it in <repo>/build*/utilities/ael_listener/ or point
#  AEL_C_LISTENER at the binary; otherwise they are skipped.

import os
import re
import signal
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

import pytest

import ael
from ael import AelEvent as E, AelOrigin as O
from conftest import patched

HERE = Path(__file__).resolve().parent
PY_LISTENER = [sys.executable, str(HERE.parent / "ael_listener.py")]
LEVELS = ["none", "group-light", "group-all", "light", "all"]

_ANSI = re.compile(r"\x1b\[[0-9;]*m")
# "HH:MM:SS LEVEL file:line: message"
_PREFIX = re.compile(r"^\d\d:\d\d:\d\d (\w+)\s+\S+:\d+: (.*)$")
_COMPARED = re.compile(r"^(\[group |\[rank |Group \S+ (cli_|size changed)|"
                       r"AEL listener stopped)")


def _c_listener() -> list[str] | None:
    env = os.environ.get("AEL_C_LISTENER")
    if env:
        return [env]
    repo = HERE.parents[2]
    for path in sorted(repo.glob("build*/utilities/ael_listener/ael_listener")):
        return [str(path)]
    return None


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Running:
    """A listener subprocess on its own port."""

    def __init__(self, cmd: list[str], level: str, csv_path: Path):
        self.port = _free_port()
        self.csv_path = csv_path
        self.proc = subprocess.Popen(
            cmd + [str(self.port), level, str(csv_path)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # Bound once the banner has been printed
        banner = self.proc.stdout.readline().decode()
        assert "AEL listener on UDP port" in banner, banner
        self.banner = _ANSI.sub("", banner)

    def stop(self, expect_rows: int) -> list[str]:
        """Wait for the expected rows, stop, and return the log messages
        (without the time/level/location prefix)."""
        deadline = time.monotonic() + 10
        while (self.csv_path.read_bytes().count(b"\n") < expect_rows + 1
               and time.monotonic() < deadline):
            time.sleep(0.02)
        # Let trailing invalid datagrams (which add no rows) drain
        time.sleep(0.3)
        self.proc.send_signal(signal.SIGINT)
        out, _ = self.proc.communicate(timeout=10)
        assert self.proc.returncode == 0

        lines = []
        for line in _ANSI.sub("", out.decode(errors="replace")).splitlines():
            m = _PREFIX.match(line)
            if m and _COMPARED.match(m.group(2)):
                lines.append(m.group(2))
        return lines


def scenario() -> tuple[list[tuple[int, bytes]], int]:
    """(sender index, datagram) pairs covering every listener feature, and
    the number of valid datagrams."""
    t0 = 1_762_270_245_000_000_000
    out = []

    def send(event, sender=0, **kw):
        kw.setdefault("timestamp_ns", t0 + len(out) * 1_234_567)
        out.append((sender, ael.encode(event, **kw)))

    mpi = dict(group_id=7, origin=O.MPI)
    are = dict(group_id=7, origin=O.ARE, rank=0)

    # Checkpoint before any connect: group size unknown
    send(E.CHECKPOINT_START, rank=0, id_version=1, name="early", **mpi)
    # A group of 3 MPI ranks connects; one duplicate
    for rank in (0, 1, 1, 2):
        send(E.CLI_CONNECT, rank=rank, size=3, id_version=7,
             event_message=f"rank {rank} up", **mpi)
    # Two ARE instances, both rank 0, from different endpoints
    send(E.CLI_CONNECT, sender=0, size=2, **are)
    send(E.CLI_CONNECT, sender=1, size=2, **are)
    # Regions and two interleaved checkpoints
    for rank in (0, 1, 2):
        send(E.REGION_ADD, rank=rank, id_version=11, size=1 << 20,
             name="grid", **mpi)
    for version in (200, 400):
        for event in (E.CHECKPOINT_START, E.CHECKPOINT_END):
            for rank in (2, 0, 1, 0):
                send(event, rank=rank, id_version=version, size=1,
                     name="heatdis", event_message="ckpt, \"quoted\"", **mpi)
        send(E.CHECKPOINT_END, sender=0, id_version=version, **are)
        send(E.CHECKPOINT_END, sender=1, id_version=version, **are)
    # Untrusted strings: line breaks, non UTF-8, control bytes, full length
    send(E.REGION_REMOVE, rank=1, id_version=-11, name=b"\xff\xfe\x01\x7f",
         event_message=b"a\nb\rc\\d\te", **mpi)
    send(E.REGION_REMOVE, rank=2, id_version=-11, name="n" * 15,
         event_message="m" * 63, **mpi)
    out.append((0, patched(patched(ael.encode(
        E.REGION_REMOVE, rank=0, id_version=-11, **mpi),
        48, b"N" * 16), 64, b"M" * 64)))
    # Extreme numbers
    send(E.RESTORE_START, rank=0, id_version=-2**63, size=2**64 - 1,
         timestamp_ns=0, **mpi)
    send(E.RESTORE_END, rank=2**64 - 1, id_version=2**63 - 1,
         timestamp_ns=2**64 - 1, group_id=2**64 - 1, origin=O.SMARTNIC)
    # A rank fails; size 0 and a size change are sender faults
    send(E.CLI_FAILURE, rank=1, size=3, id_version=139, name="SIGSEGV",
         **mpi)
    send(E.CLI_FAILURE, rank=2, size=0, id_version=1, **mpi)
    send(E.CLI_DISCONNECT, rank=0, **mpi)
    # Relaunch: new endpoint, group resized to 2, runs to completion
    for rank in (0, 1):
        send(E.CLI_CONNECT, sender=1, rank=rank, size=2, **mpi)
    for rank in (0, 1):
        send(E.RESTORE_START, sender=1, rank=rank, id_version=400, **mpi)
        send(E.RESTORE_END, sender=1, rank=rank, id_version=400, **mpi)
    for rank in (0, 1):
        send(E.CLI_DISCONNECT, sender=1, rank=rank, **mpi)
    send(E.CLI_CONNECT, sender=0, origin=O.HOST, group_id=0, size=1)
    send(E.CLI_DISCONNECT, sender=0, origin=O.HOST, group_id=0)
    valid = len(out)

    # Invalid datagrams: all discarded
    good = ael.encode(E.CLI_CONNECT, size=1, group_id=99)
    out += [(0, b""), (0, good[:-1]), (0, good + b"\0"),
            (0, patched(good, 0, b"\x01", fix_crc=False)),
            (0, patched(good, 24, struct.pack("<H", len(E)))),
            (0, patched(good, 26, struct.pack("<H", len(O))))]
    return out, valid


def run(listeners: list[Running]) -> list[list[str]]:
    datagrams, valid = scenario()
    senders = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
               for _ in range(2)]
    try:
        for s in senders:
            s.bind(("127.0.0.1", 0))
        for sender, data in datagrams:
            for listener in listeners:
                senders[sender].sendto(data, ("127.0.0.1", listener.port))
            time.sleep(0.001)
    finally:
        logs = [listener.stop(valid) for listener in listeners]
        for s in senders:
            s.close()
    return logs


@pytest.mark.parametrize("level", LEVELS)
def test_python_listener(level, tmp_path):
    datagrams, valid = scenario()
    (log,) = run([Running(PY_LISTENER, level, tmp_path / "py.csv")])

    rows = (tmp_path / "py.csv").read_bytes()
    assert rows.startswith(b"origin,event_id,gid,rank,timestamp,")
    assert log[-1] == (f"AEL listener stopped: received={valid} "
                       f"discarded={len(datagrams) - valid} "
                       f"csv_rows={valid} groups=4")

    groups = [line for line in log if line.startswith("[group ")]
    ranks = [line for line in log if line.startswith("[rank ")]
    if level == "none":
        assert not groups and not ranks
    if level == "group-light":
        assert groups and not ranks
        assert all(" cli_connect " in g or " cli_disconnect " in g
                   for g in groups)
    if level in ("group-all", "light", "all"):
        assert "[group MPI:7] cli_connect first (1/3) rank=0" in groups
        assert "[group MPI:7] cli_connect complete (3/3) rank=2" in groups
        assert "[group ARE:7] cli_connect complete (2/2) rank=0" in groups
        assert ("[group MPI:7] checkpoint_start id=1 first (1/?) rank=0"
                in groups)
        assert ("[group MPI:7] checkpoint_end id=400 complete (3/3) rank=1"
                in groups)
        assert "[group Host:0] cli_connect complete (1/1) rank=0" in groups
    if level == "group-all":
        assert not ranks
    if level == "light":
        assert ranks and all(" cli_" in r for r in ranks)
    if level == "all":
        assert len(ranks) == valid
        port = ranks[0].rsplit("from=127.0.0.1:", 1)[1].split()[0]
        assert ranks[0] == (
            '[rank MPI:7:0] checkpoint_start id_version=1 size=0 '
            f'name="early" msg="" from=127.0.0.1:{port} '
            'ts=2025-11-04T15:30:45.000Z')
        assert any('name="\\xff\\xfe\\x01\\x7f" msg="a\\nb\\x0dc\\\\d\\x09e"'
                   in r for r in ranks)


def test_csv_appends_across_runs(tmp_path):
    _, valid = scenario()
    for _ in range(2):
        run([Running(PY_LISTENER, "none", tmp_path / "py.csv")])
    data = (tmp_path / "py.csv").read_bytes()
    assert data.count(b"origin,event_id") == 1
    assert data.count(b"\n") >= 2 * valid + 1


@pytest.mark.parametrize("args", [
    ["0"], ["65536"], ["abc"], ["-1"], ["8334", "loud"],
    ["8334", "all", "x.csv", "extra"], ["-h", "extra"],
])
def test_bad_arguments(args, tmp_path):
    result = subprocess.run(PY_LISTENER + args, capture_output=True,
                            cwd=tmp_path, timeout=10)
    assert result.returncode == 1
    assert b"Usage:" in result.stderr
    assert not list(tmp_path.iterdir())


def test_help():
    result = subprocess.run(PY_LISTENER + ["--help"], capture_output=True,
                            timeout=10)
    assert result.returncode == 0
    assert b"CSV_LOCATION" in result.stderr


@pytest.mark.parametrize("level", LEVELS)
def test_matches_c_listener(level, tmp_path):
    """Same datagrams into both listeners: identical CSV and log output."""
    c_listener = _c_listener()
    if c_listener is None:
        pytest.skip("C ael_listener not built (set AEL_C_LISTENER)")

    py = Running(PY_LISTENER, level, tmp_path / "py.csv")
    c = Running(c_listener, level, tmp_path / "c.csv")
    py_log, c_log = run([py, c])

    assert (tmp_path / "py.csv").read_bytes() == \
        (tmp_path / "c.csv").read_bytes()
    assert py_log == c_log
    strip = (lambda b, port: _PREFIX.match(b.strip()).group(2)
             .replace(str(port), "PORT").replace("py.csv", "X.csv")
             .replace("c.csv", "X.csv"))
    assert strip(py.banner, py.port) == strip(c.banner, c.port)
