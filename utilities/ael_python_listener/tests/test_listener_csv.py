#  @file test_listener_csv.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief Tests for the AEL listener CSV sink
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026

import csv

import ael
from ael import AelEvent, AelOrigin
from listener_csv import CSV_COLUMNS, CsvSink, format_row

HEADER = (b"origin,event_id,gid,rank,timestamp,id_version,size,name,from_ip,"
          b"from_port,event_message\n")


def msg(**kw):
    kw.setdefault("event", AelEvent.CHECKPOINT_START)
    return ael.decode(ael.encode(**kw), ("192.168.1.100", 54321))


def test_row_format():
    row = format_row(msg(rank=2, group_id=5, id_version=-1001, size=256,
                         timestamp_ns=1_762_270_245_123_000_000,
                         name="ckpt", event_message="hello world",
                         origin=AelOrigin.ARE))
    assert row == (b"ARE,checkpoint_start,5,2,2025-11-04T15:30:45.123Z,"
                   b"-1001,256,ckpt,192.168.1.100,54321,hello world\n")


def test_quoting():
    row = format_row(msg(name='a,"b"', event_message="line1\nline2\r"))
    assert row.endswith(b',"a,""b""",192.168.1.100,54321,"line1\nline2\r"\n')
    # Bytes that are not CSV metacharacters pass through unquoted
    row = format_row(msg(name=b"\xff\x01;", event_message=""))
    assert row.endswith(b",\xff\x01;,192.168.1.100,54321,\n")


def test_header_written_once_and_rows_appended(tmp_path):
    path = tmp_path / "ael.csv"
    with CsvSink(str(path)) as sink:
        sink.write(msg(rank=0))
        # Flushed per row, readable before close
        assert path.read_bytes().count(b"\n") == 2
        assert sink.rows == 1
    with CsvSink(str(path)) as sink:
        sink.write(msg(rank=1, name="x,y"))

    data = path.read_bytes()
    assert data.startswith(HEADER) and data.count(HEADER) == 1
    rows = list(csv.DictReader(path.open(newline="")))
    assert list(rows[0]) == CSV_COLUMNS
    assert [r["rank"] for r in rows] == ["0", "1"]
    assert rows[1]["name"] == "x,y"


def test_existing_file_without_header_is_left_alone(tmp_path):
    path = tmp_path / "ael.csv"
    path.write_bytes(b"something\n")
    CsvSink(str(path)).close()
    assert path.read_bytes() == b"something\n"
