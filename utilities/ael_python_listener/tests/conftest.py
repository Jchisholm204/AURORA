#!/usr/bin/env python3
"""
@file conftest.py
@brief Pytest fixtures for AEL testing with client lifecycle management.

Fixtures handle spawning/tearing down both metrics server and C client.
"""

from typing import Callable, Set
import pytest
from ael_client import MultiClient, find_multi_client, free_udp_port
from core.ael_metrics import AELMetrics


@pytest.fixture(scope="session")
def multi_client_binary() -> str:
    """Session-scoped: find the C client binary once."""
    path = find_multi_client()
    if not path:
        pytest.skip("[!] multi_client binary not found. Build first:\n"
                    "  make BUILD_DIR=/tmp/opencode/aurora-build clean all\n")
    return path


@pytest.fixture
def metrics_server() -> AELMetrics:
    """Function-scoped: start AELMetrics server on a free port."""
    port = free_udp_port()
    metrics = AELMetrics(port=port)
    metrics.start()
    try:
        yield metrics
    finally:
        metrics.stop()


@pytest.fixture
def client_factory(multi_client_binary: str) -> Callable[[int], MultiClient]:
    """Function-scoped: return a factory that creates MultiClient instances."""
    return lambda port: MultiClient(multi_client_binary, port=port)


@pytest.fixture
def client_run(metrics_server: AELMetrics, client_factory: Callable[[int], MultiClient]) -> MultiClient:
    """Function-scoped: spawn C client bound to the metrics server's port."""
    client = client_factory(metrics_server.port)
    try:
        client.start()
        yield client
    finally:
        client.stop()

