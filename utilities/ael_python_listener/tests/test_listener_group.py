#  @file test_listener_group.py
#  @author Connor Leung (https://github.com/connor-leung)
#  @author Claude Fable 5.2 (claude-fable-5-2) Claude Code
#  @brief Tests for group level aggregation of AEL events
#  @version 0.1
#  @date Created: 2026-11-13
#  @modified Last Modified: 2026-11-13
#
#  @copyright Copyright (c) 2026

import ael
from ael import AelEvent as E, AelOrigin as O
from listener_group import GroupState as S, GroupTable


def msg(event, rank=0, size=0, version=0, gid=1, origin=O.MPI, port=1000):
    return ael.decode(ael.encode(event, rank=rank, group_id=gid, size=size,
                                 id_version=version, origin=origin),
                      ("10.0.0.1", port))


def test_first_and_last():
    t = GroupTable()
    assert t.record(msg(E.CLI_CONNECT, 0, size=3)) == S.FIRST
    assert t.record(msg(E.CLI_CONNECT, 1, size=3)) == S.OK
    assert t.record(msg(E.CLI_CONNECT, 2, size=3)) == S.LAST
    assert t.size(O.MPI, 1) == 3
    assert t.ranks_done(O.MPI, 1, E.CLI_CONNECT) == 3
    assert len(t) == 1


def test_single_member_group_is_first_and_last():
    t = GroupTable()
    assert t.record(msg(E.CLI_CONNECT, size=1)) == S.FIRST | S.LAST


def test_duplicates_do_not_count():
    t = GroupTable()
    t.record(msg(E.CLI_CONNECT, 0, size=2))
    assert t.record(msg(E.CHECKPOINT_START, 0, version=5)) == S.KEYED | S.FIRST
    assert t.record(msg(E.CHECKPOINT_START, 0, version=5)) == \
        S.KEYED | S.DUPLICATE
    assert t.ranks_done(O.MPI, 1, E.CHECKPOINT_START, 5) == 1
    assert t.record(msg(E.CHECKPOINT_START, 1, version=5)) == S.KEYED | S.LAST
    # A late duplicate after completion does not complete again
    assert t.record(msg(E.CHECKPOINT_START, 1, version=5)) == \
        S.KEYED | S.DUPLICATE


def test_keyed_events_count_per_version():
    t = GroupTable()
    t.record(msg(E.CLI_CONNECT, 0, size=2))
    assert S.FIRST in t.record(msg(E.CHECKPOINT_END, 0, version=1))
    assert S.FIRST in t.record(msg(E.CHECKPOINT_END, 0, version=2))
    assert t.ranks_done(O.MPI, 1, E.CHECKPOINT_END, 1) == 1
    assert t.ranks_done(O.MPI, 1, E.CHECKPOINT_END, 3) == 0
    # Lifecycle events ignore id_version
    assert t.ranks_done(O.MPI, 1, E.CLI_CONNECT, 12345) == 1


def test_members_are_rank_and_endpoint():
    # Every ARE instance of a group reports rank 0
    t = GroupTable()
    assert t.record(msg(E.CLI_CONNECT, 0, size=2, origin=O.ARE,
                        port=1)) == S.FIRST
    assert t.record(msg(E.CLI_CONNECT, 0, size=2, origin=O.ARE,
                        port=2)) == S.LAST


def test_groups_are_origin_and_id():
    t = GroupTable()
    t.record(msg(E.CLI_CONNECT, size=2, origin=O.MPI, gid=1))
    t.record(msg(E.CLI_CONNECT, size=4, origin=O.ARE, gid=1))
    t.record(msg(E.CLI_CONNECT, size=8, origin=O.MPI, gid=2))
    assert len(t) == 3
    assert (t.size(O.MPI, 1), t.size(O.ARE, 1), t.size(O.MPI, 2)) == (2, 4, 8)
    assert t.size(O.HOST, 1) == 0
    assert t.ranks_done(O.HOST, 1, E.CLI_CONNECT) == 0


def test_unknown_size_never_completes():
    t = GroupTable()
    assert t.record(msg(E.CHECKPOINT_START, 0, version=1)) == S.KEYED | S.FIRST
    assert t.record(msg(E.CHECKPOINT_START, 1, version=1)) == S.KEYED
    assert t.record(msg(E.CLI_CONNECT, 0, size=0)) == S.FIRST
    assert t.size(O.MPI, 1) == 0


def test_size_from_failure_and_size_change():
    t = GroupTable()
    t.record(msg(E.CLI_FAILURE, 0, size=2))
    assert t.size(O.MPI, 1) == 2
    t.record(msg(E.CLI_FAILURE, 1, size=5))
    assert t.size(O.MPI, 1) == 5
    # Size 0 is ignored, not adopted
    t.record(msg(E.CLI_FAILURE, 2, size=0))
    assert t.size(O.MPI, 1) == 5


def test_new_run_after_all_connected():
    t = GroupTable()
    t.record(msg(E.CLI_CONNECT, 0, size=2, port=1))
    t.record(msg(E.CLI_CONNECT, 1, size=2, port=1))
    t.record(msg(E.CHECKPOINT_START, 0, version=1, port=1))
    # Same members again are duplicates, not a new run
    assert t.record(msg(E.CLI_CONNECT, 0, size=2, port=1)) == S.DUPLICATE
    assert t.ranks_done(O.MPI, 1, E.CHECKPOINT_START, 1) == 1
    # A relaunched process has a new endpoint
    assert t.record(msg(E.CLI_CONNECT, 0, size=2, port=2)) == S.FIRST
    assert t.ranks_done(O.MPI, 1, E.CLI_CONNECT) == 1
    assert t.ranks_done(O.MPI, 1, E.CHECKPOINT_START, 1) == 0
    assert t.size(O.MPI, 1) == 2
    assert t.record(msg(E.CLI_CONNECT, 1, size=2, port=2)) == S.LAST


def test_new_run_after_disconnect_or_failure():
    for end in (E.CLI_DISCONNECT, E.CLI_FAILURE):
        t = GroupTable()
        t.record(msg(E.CLI_CONNECT, 0, size=3, port=1))
        t.record(msg(end, 0, size=3, port=1))
        assert t.record(msg(E.CLI_CONNECT, 1, size=3, port=1)) == S.FIRST
        assert t.ranks_done(O.MPI, 1, end) == 0


def test_no_new_run_while_group_is_still_connecting():
    t = GroupTable()
    t.record(msg(E.CLI_CONNECT, 0, size=3))
    assert t.record(msg(E.CLI_CONNECT, 1, size=3)) == S.OK
    assert t.ranks_done(O.MPI, 1, E.CLI_CONNECT) == 2
