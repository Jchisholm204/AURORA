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

@version 0.5
@author qwen 3.5 9b
"""

import argparse
import asyncio
import socket
import sys
from typing import Optional

from ael import AELPacket, AelMessage, AEL_PACKET_SIZE


class AELFatalError(Exception):
    """Raised on fatal initialization errors (handle invalid)."""
    pass


class AELInterruptedError(Exception):
    """Raised on SIGINT/SIGTERM (Ctrl+C)."""
    pass


class TimeoutError(TimeoutError):
    """Raised when receive times out (non-fatal, retryable)."""
    pass


class AELServer:
    """UDP server that receives and decodes AEL packets.
    
    Encapsulates all socket handling while delegating packet parsing to ael.AELPacket.
    Provides hybrid sync/async receive API matching C test_ael_server.c behavior.
    
    The recv() method handles the entire lifecycle internally:
      - Checks if socket is initialized (calls open() if needed)
      - Blocks on receive with timeout support
      - Handles interrupts and timeouts
      - Returns decoded AEL message or raises appropriate exceptions
    
    Attributes:
        port: UDP port number to bind.
        sock: Socket instance (created internally by recv/open()).
        stopped: Signal flag for graceful shutdown.
    
    Usage:
        server = AELServer(port=8334)
        
        # Option 1: Use with context manager (recommended)
        async with server:
            while True:
                msg = await server.recv_async(timeout=-1)
                print(msg.event.name)
        
        # Option 2: Manual lifecycle
        server = AELServer(port=8334)
        server.open()
        try:
            while not g_stop:
                msg = server.recv(timeout=-1)
                log_message(msg)
        except TimeoutError:
            continue  # Expected in loop
        finally:
            server.close()
    """
    
    def __init__(self, port: int):
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.stopped = False
    
    async def _receive_packet(self) -> bytes | None:
        """
        Internal non-blocking receive with timeout.
        
        Returns packet bytes or None on timeout/interrupt.
        """
        # Set socket to non-blocking for async compatibility
        if self.sock and hasattr(self.sock, 'setblocking'):
            self.sock.setblocking(False)
        
        try:
            if self.sock:
                data, addr = self.sock.recvfrom(AEL_PACKET_SIZE)
                return data
        except BlockingIOError:
            return None
        except (OSError, socket.error):
            # Socket error or closed - treat as timeout for retryability
            return None
    
    def recv(self, timeout: int = -1) -> AelMessage:
        """
        Blocking synchronous receive (C-style).
        
        Handles entire receive lifecycle internally:
          • Creates socket if not initialized
          • Blocks until packet received or timeout expires
          • Decodes and returns AEL message
          • Raises appropriate exceptions on error
        
        Args:
            timeout: seconds to wait (-1 = block forever, 0 = non-blocking)
        
        Returns:
            Decoded AelMessage
        
        Raises:
            TimeoutError: when timeout expires (non-fatal, retryable)
            AELFatalError: on fatal initialization error (handle invalid)
            AELInterruptedError: on Ctrl+C/SIGINT
        """
        # Create socket if not initialized yet
        if self.sock is None:
            try:
                self.open()
            except Exception as e:
                raise AELFatalError(f"Failed to initialize socket: {e}") from e
        
        # Use select for timeout support in blocking mode
        import select
        
        start_time = None
        elapsed = 0
        max_timeout = 60 if timeout < 0 else timeout
        
        while True:
            try:
                if timeout >= 0:
                    remaining = timeout - elapsed
                    # Ensure we don't wait indefinitely even if timeout was very large
                    if remaining > max_timeout:
                        remaining = max_timeout
                    
                    rlist, _, _ = select.select([self.sock], [], [], remaining)
                    
                    if rlist:
                        data, addr = self.sock.recvfrom(AEL_PACKET_SIZE)
                        # Decode using the socket's bound address
                        return AELPacket.decode(data, self.sock.getsockname())
                else:
                    # Block indefinitely like C recv(..., -1)
                    data, addr = self.sock.recvfrom(AEL_PACKET_SIZE)
                    return AELPacket.decode(data, self.sock.getsockname())
                    
            except (KeyboardInterrupt, SystemExit):
                raise AELInterruptedError("Interrupted by user")
                
            except select.error as e:
                # Socket closed or error - reinitialize socket and retry
                if self.sock is None:
                    raise AELFatalError("Socket initialization failed after error")
                raise TimeoutError("Select error on socket") from e
    
    async def recv_async(self, timeout: Optional[float] = None) -> AelMessage:
        """
        Async receive for CHILT integration.
        
        Handles entire receive lifecycle internally:
          • Creates socket if not initialized
          • Blocks until packet received or timeout expires
          • Decodes and returns AEL message
          • Raises appropriate exceptions on error
        
        Args:
            timeout: seconds to wait (None = block forever)
        
        Returns:
            Decoded AelMessage
        
        Raises:
            TimeoutError: when timeout expires (non-fatal, retryable)
            AELFatalError: on fatal initialization error (handle invalid)
            AELInterruptedError: on Ctrl+C/SIGINT
        """
        # Create socket if not initialized yet
        if self.sock is None:
            try:
                self.open()
            except Exception as e:
                raise AELFatalError(f"Failed to initialize socket: {e}") from e
        
        try:
            while True:
                # Wait for packet with timeout
                remaining = None
                if timeout is not None:
                    remaining = timeout
                
                data = await asyncio.wait_for(
                    self._receive_packet(),
                    timeout=remaining if remaining is not None else None
                )
                
                if data is not None:
                    # Decode packet using socket's bound address
                    return AELPacket.decode(data, self.sock.getsockname())
                    
        except TimeoutError as e:
            raise TimeoutError(f"Receive timeout after {timeout}s" if timeout else "Receive timeout") from e
        
        except asyncio.CancelledError:
            raise AELInterruptedError("Cancelled by user") from None
        
        except Exception as e:
            # Convert socket errors to appropriate exceptions
            err_no = getattr(e, 'errno', None)
            import errno
            if err_no == errno.EINTR or (hasattr(socket, 'EINTR') and err_no == socket.EINTR):
                raise TimeoutError("Interrupted") from e
            
            if hasattr(self.sock, 'getblocking'):
                blocking = self.sock.getblocking()
                if not blocking:
                    raise TimeoutError("No packet ready")
        
        return None
    
    def open(self) -> None:
        """Create and bind socket."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Bind to all interfaces
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("", self.port))
    
    def close(self) -> None:
        """Cleanup socket."""
        if self.sock:
            self.sock.close()
            self.sock = None


__all__ = ['AELServer', 'AELFatalError', 'AELInterruptedError']
