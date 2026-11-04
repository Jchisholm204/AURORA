#!/usr/bin/env python3
"""
@file ael_server.py
@brief AEL UDP server - handles socket operations and packet receiving.

Encapsulates all socket handling (open, bind, receive) while delegating
packet parsing to ael.AELPacket for separation of concerns.

Provides hybrid sync/async API:
  • recv()         - Blocking synchronous receive (C-style)
  • recv_async()   - Async receive for CHILT integration

Usage pattern:
  server = AELServer(port=8334)
  server.open()
  try:
    while True:
      msg = server.recv(timeout=-1)
      log_message(msg)
  except TimeoutError:
    pass  # Expected, continue loop
  finally:
    server.close()

@date Created: 2026-10-28
@modified Last Modified: 2026-10-30

@copyright Copyright (c) 2026
@version 0.7
@author qwen 3.5 9b
"""

import asyncio
import errno
import select
import socket
import time
from typing import Optional

from .ael import AELPacket, AelMessage, AEL_PACKET_SIZE


class AELFatalError(Exception):
    """Raised on fatal initialization errors (handle invalid)."""
    pass


class AELInterruptedError(Exception):
    """Raised on SIGINT/SIGTERM (Ctrl+C)."""
    pass


class AELServer:
    """UDP server that receives and decodes AEL packets.

    Encapsulates all socket handling while delegating packet parsing to
    ael.AELPacket. Provides a hybrid sync/async receive API matching C
    test_ael_server.c behaviour.

    Both receive methods handle the full lifecycle internally:
      - Create the socket if it is not initialized
      - Block (or wait) until a packet arrives or the timeout expires
      - Decode and return the AEL message
      - Raise the appropriate exception on error

    Attributes:
        port: UDP port number to bind.
        sock: Socket instance (created lazily by recv/open).

    Usage:
        # Option 1: Context manager (recommended)
        with AELServer(port=8334) as server:
            while True:
                msg = server.recv(timeout=-1)
                print(msg.event.name)

        # Option 2: Async receive for CHILT integration
        async with AELServer(port=8334) as server:
            while True:
                msg = await server.recv_async(timeout=-1)
                print(msg.event.name)
    """

    def __init__(self, port: int):
        self.port = port
        self.sock: Optional[socket.socket] = None

    def _receive_packet(self) -> tuple[bytes, tuple]:
        """Receive a single datagram with timeout polling.

        Returns:
            A ``(data, addr)`` tuple as returned by ``sock_recvfrom``.

        Raises:
            AELFatalError: if the socket is not open.
        """
        if self.sock is None:
            raise AELFatalError("Socket is not open")

        # Use select for polling with short timeouts
        import select
        
        sock = self.sock
        start = time.time()

        while True:
            elapsed = time.time() - start

            readable, _, _ = select.select([sock], [], [], 0.1)
            if readable:
                data, addr = sock.recvfrom(AEL_PACKET_SIZE)
                return data, addr

    def recv(self, timeout: int = -1) -> AelMessage:
        """Blocking synchronous receive (C-style).

        Args:
            timeout: Seconds to wait (-1 = block forever, 0 = non-blocking).

        Returns:
            Decoded AelMessage.

        Raises:
            TimeoutError: when the timeout expires (non-fatal, retryable).
            AELFatalError: on fatal initialization error.
            AELInterruptedError: on Ctrl+C/SIGINT.
        """
        if self.sock is None:
            try:
                self.open()
            except Exception as e:
                raise AELFatalError(f"Failed to initialize socket: {e}") from e

        while True:
            try:
                rlist, _, _ = select.select(
                    [self.sock], [], [], timeout if timeout >= 0 else None
                )
            except (KeyboardInterrupt, SystemExit):
                raise AELInterruptedError("Interrupted by user") from None
            except OSError as e:
                if self.sock is None:
                    raise AELFatalError("Socket closed while waiting for data") from e
                raise TimeoutError("Select error on socket") from e

            if not rlist:
                raise TimeoutError(f"Receive timeout after {timeout}s")

            try:
                data, addr = self.sock.recvfrom(AEL_PACKET_SIZE)
            except (KeyboardInterrupt, SystemExit):
                raise AELInterruptedError("Interrupted by user") from None
            except BlockingIOError:
                continue

            return AELPacket.decode(data, addr)

    async def recv_async(self, timeout: Optional[int] = None) -> AelMessage:
        """Async receive for CHILT integration.

        Args:
            timeout: Seconds to wait (None = block forever).

        Returns:
            Decoded AelMessage.

        Raises:
            TimeoutError: when the timeout expires (non-fatal, retryable).
            AELFatalError: on fatal initialization error.
            AELInterruptedError: on cancellation.
        """
        if self.sock is None:
            try:
                self.open()
            except Exception as e:
                raise AELFatalError(f"Failed to initialize socket: {e}") from e

        # Use blocking socket with select polling for timeout handling
        self.sock.setblocking(True)

        loop = asyncio.get_running_loop()
        data, addr = await asyncio.wait_for(
            loop.run_in_executor(None, self._receive_packet), timeout=timeout
        )
        return AELPacket.decode(data, addr)

    def open(self) -> None:
        """Create and bind the UDP socket in non-blocking mode."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("", self.port))
        # Non-blocking lets the asyncio path await datagrams; recv() still
        # gates on select() before reading, so it remains effectively blocking.
        self.sock.setblocking(False)

    def close(self) -> None:
        """Close and clear the socket."""
        if self.sock:
            self.sock.close()
            self.sock = None

    def __enter__(self) -> 'AELServer':
        """Enter context - initialize the socket if not already done."""
        if self.sock is None:
            self.open()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Exit context - cleanup the socket regardless of exception."""
        self.close()
        return False

    async def __aenter__(self) -> 'AELServer':
        """Enter async context - initialize the socket if not already done."""
        return self.__enter__()

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Exit async context - cleanup the socket regardless of exception."""
        return self.__exit__(exc_type, exc_val, exc_tb)


__all__ = ['AELServer', 'AELFatalError', 'AELInterruptedError']
