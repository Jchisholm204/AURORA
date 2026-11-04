#!/usr/bin/env python3
"""
@file multi_client_launcher.py
@brief Launcher and protocol for the C `multi_client` sender.

No pytest imports here. Used by conftest.py fixtures and standalone mode.
"""

import os
import re
import shlex
import subprocess
from pathlib import Path
from typing import Optional

# Default configuration for the multi_client binary
DEFAULT_RANKS = 4
DEFAULT_PACKETS_PER_RANK = 36
DEFAULT_HOST = "127.0.0.1"

MISSING_BINARY_MESSAGE = (
    "Cannot find multi_client binary in: $AEL_MULTI_CLIENT, /tmp/opencode/aurora-build/tests/multi_client, or <repo>/build/tests/multi_client"
)


def free_udp_port() -> int:
    """Return a random unused UDP port."""
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("", 0))
        return int(s.getsockname()[1])


def find_multi_client() -> Optional[str]:
    """
    Find the multi_client binary using environment variable or standard paths.
    
    Returns path to a runnable binary, or None if not found.
    """
    candidates = []
    
    # Check $AEL_MULTI_CLIENT
    env_path = os.environ.get("AEL_MULTI_CLIENT")
    if env_path:
        candidates.append(env_path)
    
    # Standard build directory (AURORA repo root is 2 levels up from tests/)
    repo_root = Path(__file__).resolve().parents[2]
    standard_paths = [
        str(repo_root / "build" / "tests" / "multi_client"),
        "/tmp/opencode/aurora-build/tests/multi_client",
    ]
    candidates.extend(standard_paths)
    
    for candidate in candidates:
        # Check if file exists and is executable
        if Path(candidate).exists():
            full_path = str(Path(candidate).resolve())
            try:
                # Test with --help to ensure it's runnable
                try:
                    subprocess.run(
                        [shlex.split(full_path)[0], "--help"],
                        timeout=2,
                        capture_output=True,
                        check=False
                    )
                    return full_path
                except subprocess.CalledProcessError:
                    continue
                return full_path
            except (subprocess.CalledProcessError, FileNotFoundError):
                continue
    
    return None


class MultiClient:
    """
    A client-like object that wraps the C multi_client subprocess.
    
    Provides start/wait/stop lifecycle with protocol for testing.
    Does NOT launch itself in pytest mode; conftest.py does that.
    Can be used directly in standalone mode if needed.
    """
    
    def __init__(
        self,
        binary: str,
        *,
        port: int,
        ranks: int = DEFAULT_RANKS,
        packets: int = DEFAULT_PACKETS_PER_RANK,
        host: str = DEFAULT_HOST,
        timeout: float = 20.0
    ):
        """
        Initialize with a runnable binary path.
        
        Args:
            binary: Path to the multi_client executable
            port: UDP port for the server
            ranks: Number of ranks (default 4)
            packets: Packets per rank (default 36)
            host: Host address (default 127.0.0.1)
            timeout: Default wait timeout in seconds
        """
        self.binary = binary
        self.port = port
        self.ranks = frozenset(range(ranks))
        self.packets = packets
        self.host = host
        self.timeout = timeout
        self._process: Optional[subprocess.Popen] = None
    
    def _build_args(self) -> list[str]:
        """Build the command arguments for multi_client."""
        ranks_str = str(int(self.ranks)) if isinstance(self.ranks, (int, float)) else "4"
        packets_str = str(int(self.packets)) if isinstance(self.packets, (int, float)) else "36"
        return [self.binary, self.host, ranks_str, packets_str, str(self.port)]
    
    def start(self) -> int:
        """
        Start the multi_client subprocess.
        
        Returns:
            PID of the process
            
        Raises:
            OSError: If binary not found or fails to start
        """
        if self._process is None and Path(self.binary).exists():
            args = shlex.split(" ".join(self._build_args()))
            # Launch with stdout/stderr to terminal (or capture as needed)
            self._process = subprocess.Popen(args, stdout=None, stderr=None)
            return self._process.pid
        elif self._process is not None:
            return self._process.pid
        else:
            raise OSError(f"Binary not found: {self.binary}")
    
    def wait(self, timeout: Optional[float] = None) -> int:
        """
        Wait for the sender to finish.
        
        Args:
            timeout: Override default timeout (None = process finishes anytime)
            
        Returns:
            Exit code of the process
            
        Raises:
            TimeoutError: If timeout exceeded
        """
        if self._process is None:
            return 0
        
        to_use = timeout if timeout is not None else self.timeout
        return self._process.wait(timeout=to_use)
    
    def stop(self, force: bool = False) -> None:
        """
        Stop the sender.
        
        Args:
            force: If True, immediately kill without waiting (default False)
        """
        if self._process is None:
            return
        
        if not force and self._process.returncode is None:
            # Try graceful termination first
            self._process.terminate()
            try:
                self._process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                self._process.kill()
        else:
            self._process.kill()
        
        try:
            self._process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            pass
        
        self._process = None
    
    def __enter__(self):
        """Context manager entry."""
        pid = self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - always cleanup."""
        self.stop()
        return False
