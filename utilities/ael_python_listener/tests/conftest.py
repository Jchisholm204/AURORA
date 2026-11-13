#  @file conftest.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief Shared helpers for the Python AEL listener tests
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ael  # noqa: E402


def patched(packet: bytes, offset: int, raw: bytes,
            fix_crc: bool = True) -> bytes:
    """Overwrite bytes of a valid packet, optionally keeping the CRC valid."""
    data = bytearray(packet)
    data[offset:offset + len(raw)] = raw
    if fix_crc:
        data[28:32] = struct.pack("<I", ael._crc(bytes(data)))
    return bytes(data)
