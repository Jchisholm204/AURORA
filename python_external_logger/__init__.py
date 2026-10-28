"""Python External Logger (AEL) receive-only client interface.

This package provides:
  - ael.py      : AEL packet parsing library (decode, enum, dataclass)
  - ael_receiver.py : UDP receiver implementation
  - ael_sender.py  : Test packet sender

Usage:
    from ael import decode, AelEvent, AelMessage, packet_to_bytes
    
    # Receiver:
    python3 ael_receiver.py --port 8334
    
    # Sender:
    python3 ael_sender.py send --rank 1 --event CHECKPOINT_END

See README.md for more details.
"""
