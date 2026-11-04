#!/usr/bin/env python3
"""Debug script to trace AELMetrics receive path."""

import os
os.environ['UCX_HOME'] = '/usr/local'

import asyncio
import socket
import struct
import time
import sys

sys.path.insert(0, '.')

from core.ael import AELPacket, AEL_PACKET_SIZE
from core.ael_server import AELServer


async def test_recv():
    """Test async recv with blocking socket."""
    print("[*] Testing async recv...")
    
    server = AELServer(port=8335)
    server.open()
    print("[+] Socket opened")
    
    # Set to blocking mode (critical for timeout to work)
    server.sock.setblocking(True)
    print("[+] Socket set to blocking mode")
    
    # Wait a moment for client to connect and send
    time.sleep(0.5)
    print("[*] Waiting for packets...")
    
    try:
        print("[+] Attempting recv_async(timeout=2)...")
        msg = await server.recv_async(timeout=2.0)
        print(f"[+] SUCCESS: rank={msg.rank} ev={msg.event.name}")
        return True
    except TimeoutError as e:
        print(f"[-] Timeout after 2s: {e}")
        return False
    except Exception as e:
        print(f"[-] Error: {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        server.close()


if __name__ == '__main__':
    result = asyncio.run(test_recv())
    sys.exit(0 if result else 1)
