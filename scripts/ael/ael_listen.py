#!/usr/bin/env python3
"""Reference AEL subscriber.

Subscribes to an AURORA External Logger relay and prints every event it
forwards. Use --csv for machine-readable output.
"""

import argparse
import socket
import struct
import sys

MAGIC = b"AEL1"
PACKET_FMT = "<4sBBHQQQqqd16s"
PACKET_SIZE = struct.calcsize(PACKET_FMT)  # 72

TYPES = {
    0x01: "CONNECT",
    0x02: "CHECKPOINT",
    0x03: "RESTART",
    0x04: "DISCONNECT",
}
SUBSCRIBE = 0x80
UNSUBSCRIBE = 0x81

FIELDS = ["timestamp_ns", "type", "status", "rank", "group_id",
          "group_size", "version", "name", "duration_ms"]


def ctrl(type_):
    return MAGIC + bytes([type_])


def decode(data):
    if len(data) != PACKET_SIZE:
        return None
    (magic, type_, status, _res, ts, rank, gid, gsize, version, dur,
     name) = struct.unpack(PACKET_FMT, data)
    if magic != MAGIC or type_ not in TYPES:
        return None
    return {
        "timestamp_ns": ts,
        "type": TYPES[type_],
        "status": "OK" if status == 0 else "FAIL",
        "rank": rank,
        "group_id": gid,
        "group_size": gsize,
        "version": version,
        "name": name.split(b"\0", 1)[0].decode(errors="replace"),
        "duration_ms": dur,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="127.0.0.1", help="AEL relay host")
    ap.add_argument("--port", type=int, default=9064, help="AEL relay port")
    ap.add_argument("--csv", action="store_true", help="print CSV rows")
    ap.add_argument("--count", type=int, default=0,
                    help="exit after N events (0 = run forever)")
    args = ap.parse_args()

    relay = (args.host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(ctrl(SUBSCRIBE), relay)

    if args.csv:
        print(",".join(FIELDS), flush=True)

    seen = 0
    try:
        while args.count == 0 or seen < args.count:
            data, _ = sock.recvfrom(512)
            evt = decode(data)
            if evt is None:
                continue
            seen += 1
            if args.csv:
                print(",".join(str(evt[f]) for f in FIELDS), flush=True)
            else:
                print(" ".join(f"{k}={v}" for k, v in evt.items()),
                      flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        sock.sendto(ctrl(UNSUBSCRIBE), relay)
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
