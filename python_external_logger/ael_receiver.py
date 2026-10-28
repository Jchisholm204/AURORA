#!/usr/bin/env python3
"""
@file ael_receiver.py
@brief AEL UDP receiver - complete application with signal handling and logging.

Handles socket operations, signal management, and provides CLI interface.
Uses AELServer from ael_server for receive operations.

Usage (sync mode):
  python3 ael_receiver.py --port 8334

@version 0.5
@author qwen 3.5 9b
"""

import argparse
import signal
import socket
import sys
from ael_server import AELServer, AELFatalError, AELInterruptedError
from ael import AelMessage


def main(port: int = 8334):
    """Run receiver on specified port with signal handling."""
    
    # Set up signal handler for graceful shutdown
    def _signal_handler(signum, frame):
        print(f"\n⚠ Received {signum} - shutting down...", file=sys.stderr)
        sys.exit(0)
    
    signal.signal(signal.SIGINT, _signal_handler)
    signal.signal(signal.SIGTERM, _signal_handler)
    
    server = AELServer(port=port)
    
    # Initialize socket and enter receive loop
    try:
        print(f"AEL UDP server listening on port {port} (Ctrl+C to stop)", file=sys.stderr)
        
        while True:
            try:
                msg = server.recv(timeout=-1)  # Block forever like C with -1
                
                # Log message matching C test_ael_server.c output format
                _log_message(msg, file=sys.stderr)
                
            except TimeoutError:
                # Expected behavior - continue loop (like C's transient status)
                continue
                
            except AELFatalError as e:
                print(f"✗ Fatal error: {e}", file=sys.stderr)
                raise
    
    except AELInterruptedError:
        print("\n⚠ Interrupted by user", file=sys.stderr)
    finally:
        server.close()


def _log_message(msg: AelMessage, file=None):
    """Log message matching C test_ael_server.c output format."""
    
    # Format IP address
    from_ip = msg.from_ip if isinstance(msg.from_ip, str) else "0x{:08x}".format(msg.from_ip)
    
    print(f"rx from={from_ip}:{msg.from_port} rank={msg.rank} group_id={msg.group_id}", file=file, end="")
    
    # Format timestamp
    if msg.timestamp_ns == 0:
        ts = "N/A"
    else:
        from datetime import datetime, timezone
        ts = datetime.fromtimestamp(msg.timestamp_ns / 1e9, tz=timezone.utc).isoformat()
    print(f" ts={ts} ev={msg.event.name}", file=file)
    
    # Log payload fields
    print(f"  id_version={msg.id_version} size={msg.size} name=\"{msg.name}\"", file=file)
    print(f"  event=\"{msg.event_message}\"", file=file)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="AEL UDP receiver - binds to port and logs received events",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 ael_receiver.py --port 8334              # Run in foreground (sync)
  python3 ael_receiver.py --port 8335              # Use different port

For async mode (CHILT integration), see ael_server.AELServer.recv_async() API.
"""
    )
    parser.add_argument(
        "--port", type=int, default=8334,
        help="UDP port to bind (default: 8334)"
    )
    args = parser.parse_args()
    
    main(port=args.port)
