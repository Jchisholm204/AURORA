# Python AEL Listener

Python port of the C `ael_listener` (`utilities/ael_listener`, #100), plus a
small library for working with AEL packets (#101). Standard library only;
Python 3.10+.

## Listener

```sh
python3 ael_listener.py [PORT [LEVEL [CSV_LOCATION]]]
```

- `PORT` UDP port to receive on (1..65535, default 8334)
- `LEVEL` stdout logging level: `none`, `group-light`, `group-all` (default),
  `light`, `all`. Does not affect the CSV.
- `CSV_LOCATION` CSV that every received message is appended to (optional)

Arguments, levels, log lines, and the CSV are the same as the C listener: given
the same datagrams, both write byte identical CSV files. Stop it with Ctrl+C or
SIGTERM.

## Library

| Module | C counterpart | Contents |
|--------|---------------|----------|
| `ael.py` | `common/include/ael/ael.h` | `AelEvent`, `AelOrigin`, `AelMessage`, `decode`, `encode`, `AelReceiver` |
| `listener_group.py` | `listener_group.c` | `GroupTable`: first/complete tracking of group events |
| `listener_csv.py` | `listener_csv.c` | `CsvSink`: the listener CSV format |
| `ael_listener.py` | `main.c` | `Listener` class and the command line entry point |

```python
import ael

with ael.AelReceiver(port=8334) as rx:
    msg = rx.recv(timeout=1.0)      # None on timeout, AelError on a bad packet
    if msg and msg.event == ael.AelEvent.CHECKPOINT_START:
        print(msg.rank, msg.id_version, msg.name)
```

`ael.encode(...)` builds a valid datagram for tests and fake senders.

The wire format lives at the top of `ael.py` and must be kept in step with
`common/include/ael/ael_packet.h`.

## Tests

```sh
pip install pytest
python3 -m pytest tests
```

`tests/test_ael_listener.py` also runs the C listener next to the Python one
and compares their CSV and log output. It looks for the binary in
`<repo>/build*/utilities/ael_listener/ael_listener`, or at the path in
`AEL_C_LISTENER`; without it those tests are skipped.
