# This file contains AEL simple sender implementation.
#!/usr/bin/env python3
"""
@file ael_sender_simple.py
@brief Simple AEL packet sender for testing.

Usage:
    python3 ael_sender_simple.py --rank 1 --event CHECKPOINT_END

Events (mirror C eAEL_event enum):
  CLI_CONNECT          - Client connected
  CLI_FAILURE          - Client failed to connect  
  CLI_DISCONNECT       - Client disconnected
  CHECKPOINT_START     - Checkpoint started
  CHECKPOINT_END       - Checkpoint ended
  RESTORE_START        - Region restore started
  RESTORE_END          - Region restore ended
  REGION_ADD           - Region added
  REGION_REMOVE        - Region removed

@date Created: 2026-10-28
@modified Last Modified: 2026-10-28

@copyright Copyright (c) 2026
@version 0.3
@author qwen 3.5 9b
"""

import argparse
import socket
import sys


from ael import AELPacket, AelEvent, AEL_PACKET_SIZE


def event_from_name(name: str) -> int:
    """Convert event name string to enum code."""
    try:
        return getattr(AelEvent, name.upper()).value
    except AttributeError:
        print(f"Error: Unknown event '{name}'. Valid events:", file=sys.stderr)
        for ev in AelEvent:
            print(f"  {ev.name}: {ev.value}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Send AEL packets to receiver")
    parser.add_argument("--rank", type=int, default=0, help="Source rank")
    parser.add_argument("--event", type=str, required=True, choices=[e.name for e in AelEvent],
                       help=f"Event type (see choices)")
    parser.add_argument("--ip", type=str, default="127.0.0.1", help="Receiver IP")
    parser.add_argument("--port", type=int, default=8334, help="Receiver port")
    parser.add_argument("--group-id", type=int, default=0, help="Group ID (default: 0)")
    parser.add_argument("--timestamp-ns", type=str, default="", help="Timestamp in ns (empty = current)")
    parser.add_argument("--name", type=str, default="", help="Region/event name (empty = no name)")
    
    args = parser.parse_args()
    
    # Get timestamp: empty string means current time, else use provided value
    if args.timestamp_ns == "":
        import time
        timestamp_ns = int(time.time() * 1e9)
    else:
        timestamp_ns = int(args.timestamp_ns)
    
    event_code = event_from_name(args.event)
    
    # Create packet with all fields (name is optional, event_message defaults to empty string)
    packet = AELPacket.encode(
        event_code=event_code,
        rank=args.rank,
        group_id=args.group_id,
        timestamp_ns=timestamp_ns,
        name=args.name  # Empty string means no name field
    )
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        print(f"Sending {AelEvent(event_code).name} to {args.ip}:{args.port}...", file=sys.stderr)
        sock.sendto(packet, (args.ip, args.port))
        print("✅ Packet transmitted successfully", file=sys.stderr)
    except socket.timeout:
        print("⚠ Send timed out - receiver may not be listening", file=sys.stderr)
    except socket.error as e:
        print(f"✗ Send error: {e}", file=sys.stderr)
    finally:
        sock.close()


if __name__ == "__main__":
    main()
