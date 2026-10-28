"""Python External Logger (AEL) receive-only client interface.

This package provides:
  - ael.py      : AEL packet parsing library (decode, enum, dataclass)
  - ael_receiver.py : UDP receiver implementation
  - ael_sender_simple.py : Test packet sender

Usage:
    from ael import AELPacket, AelEvent, AelMessage
    
    # Receiver:
    python3 ael_receiver.py --port 8334
    
    # Sender:
    python3 ael_sender_simple.py send --rank 1 --event CHECKPOINT_END

See README.md for more details.
"""
