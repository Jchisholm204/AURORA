#!/usr/bin/env python3
"""
@file ael_client.py
@brief C multi_client launcher, protocol definition, and discovery helpers.

No pytest or AELMetrics dependencies — pure client orchestration.
"""

import os
import subprocess
import sys
from pathlib import Path
from typing import Optional, Callable, Set, Tuple

DEFAULT_RANKS = 4
DEFAULT_PACKETS_PER_RANK = 36
DEFAULT_HOST = "127.0.0.1"

# Discovery paths
AEL_MULTI_CLIENT_ENV = os.environ.get("AEL_MULTI_CLIENT", "")
BUILD_DIR = Path("/tmp/opencode/aurora-build/tests/multi_client")
REPO_BUILD = (Path(os.path.dirname(__file__)).parent.parent / "build/tests/multi_client")

MISSING_BINARY_MESSAGE = (
    "[!] multi_client binary not found. Build it first:\n"
    "  make BUILD_DIR=/tmp/opencode/aurora-build clean all\n"
)


def find_multi_client() -> Optional[str]:
    """Find the C client binary via environment or standard paths."""
    candidates: list[Path] = []

    if AEL_MULTI_CLIENT_ENV:
        candidates.append(Path(AEL_MULTI_CLIENT_ENV))

    if BUILD_DIR.exists():
        candidates.append(BUILD_DIR)

    if REPO_BUILD.exists():
        candidates.append(REPO_BUILD)

    for path in candidates:
        try:
            proc = subprocess.run(
                [str(path), "--help"],
                capture_output=True,
                timeout=2.0,
                check=False,
            )
            if proc.returncode == 0:
                return str(path)
        except Exception:
            continue

    # If no valid binary found but paths exist, return the first one anyway
    for path in candidates:
        if path.exists():
            return str(path)

    return None


def free_udp_port() -> int:
    """Find a random ephemeral port."""
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind(("", 0))
        return s.getsockname()[1]
    finally:
        s.close()


# Protocol definition
class AelClient:
    """Protocol for any C client that emits AEL events."""

    def __init__(self, binary: str, *, port: int, ranks: Set[int], packets: int,
                 host: str = DEFAULT_HOST) -> None:
        self.binary = binary
        self.port = port
        self.ranks = frozenset(ranks)
        self.packets = packets
        self.host = host
        self._proc: Optional[subprocess.Popen] = None
        self._running = False

    def start(self) -> None:
        args = [self.binary, self.host, str(len(self.ranks)), str(self.packets), str(self.port)]
        self._proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self._running = True

    def wait(self, timeout: Optional[float] = None) -> int:
        """Wait for client process to finish."""
        if not self._running or self._proc is None:
            return -1
        try:
            return self._proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return -1

    def stop(self) -> None:
        """Terminate the client process."""
        if self._proc is not None and self._running:
            try:
                self._proc.terminate()
                self._proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                try:
                    self._proc.kill()
                except ProcessLookupError:
                    pass
            finally:
                self._running = False

    def __enter__(self) -> "AelClient":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()


def AelClientFactory() -> Callable[[int], AelClient]:
    """Default factory returning MultiClient instances."""
    binary = find_multi_client()
    if not binary:
        raise FileNotFoundError(MISSING_BINARY_MESSAGE)

    def create(port: int) -> AelClient:
        return AelClient(binary, port=port, ranks=range(DEFAULT_RANKS), packets=DEFAULT_PACKETS_PER_RANK)

    return create


class MultiClient(AelClient):
    """Concrete implementation with fixed defaults."""

    def __init__(self, binary: str, *, port: int, ranks: Optional[Set[int]] = None,
                 packets: Optional[int] = None, host: str = DEFAULT_HOST) -> None:
        super().__init__(binary, port=port, ranks=ranks or set(range(DEFAULT_RANKS)),
                        packets=packets or DEFAULT_PACKETS_PER_RANK, host=host)
