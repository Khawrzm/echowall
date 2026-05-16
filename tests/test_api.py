"""Tests for the ECHOWALL REST API."""
import pytest
from fastapi.testclient import TestClient
from echowall.api.server import build_app


@pytest.fixture
def client():
    app = build_app(pipeline=None)
    return TestClient(app)


def test_health(client):
    r = client.get("/health")
    assert r.status_code == 200
    assert r.json()["status"] == "ok"


def test_presence_no_pipeline(client):
    r = client.get("/presence")
    assert r.status_code == 200
    data = r.json()
    assert "presence" in data
    assert "count" in data
    assert "posture" in data
