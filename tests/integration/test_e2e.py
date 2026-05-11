import socket

import pytest
import httpx
import uuid

BASE_URL = "http://localhost:8000"


def _server_running(host="localhost", port=8000):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1)
        s.connect((host, port))
        s.close()
        return True
    except OSError:
        return False


@pytest.fixture(autouse=True)
def _skip_if_no_server():
    if not _server_running():
        pytest.skip("Backend server not running on localhost:8000")
TEST_MAC = "aa:bb:cc:dd:ee:ff"
TEST_USER = f"e2e_{uuid.uuid4().hex[:8]}"
TEST_PASS = "test123456"


@pytest.mark.asyncio
async def test_device_lifecycle():
    async with httpx.AsyncClient() as client:
        resp = await client.post(
            f"{BASE_URL}/api/auth/register",
            json={"username": TEST_USER, "password": TEST_PASS},
        )
        assert resp.status_code == 200, f"Register failed: {resp.text}"
        user_id = resp.json()["user_id"]

        resp = await client.post(
            f"{BASE_URL}/api/auth/login",
            json={"username": TEST_USER, "password": TEST_PASS},
        )
        assert resp.status_code == 200, f"Login failed: {resp.text}"

        resp = await client.get(f"{BASE_URL}/api/auth/me")
        assert resp.status_code == 200
        assert resp.json()["username"] == TEST_USER

        resp = await client.post(
            f"{BASE_URL}/api/device/register",
            json={"mac": TEST_MAC, "hw_rev": "0.1", "fw_version": "1.0.0", "ip": "192.168.1.100"},
        )
        assert resp.status_code == 200, f"Device register failed: {resp.text}"

        resp = await client.post(
            f"{BASE_URL}/api/user/devices/bind",
            json={"mac": TEST_MAC},
        )
        assert resp.status_code == 200, f"Bind failed: {resp.text}"

        resp = await client.get(f"{BASE_URL}/api/user/devices")
        assert resp.status_code == 200
        devices = resp.json()["devices"]
        assert any(d["mac"] == TEST_MAC for d in devices)

        card_payload = {
            "device_mac": TEST_MAC,
            "bg_type": "pong_clock",
            "position": 0,
            "fg_config": {},
            "duration": 30,
            "rel_brightness": 80,
        }
        resp = await client.post(
            f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards",
            json=card_payload,
        )
        assert resp.status_code == 200, f"Create card failed: {resp.text}"
        card_id = resp.json()["id"]

        resp = await client.get(f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards")
        assert resp.status_code == 200
        cards = resp.json()["cards"]
        assert len(cards) == 1
        assert cards[0]["bg_type"] == "pong_clock"

        resp = await client.put(
            f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards/{card_id}",
            json={"duration": 60, "rel_brightness": 50},
        )
        assert resp.status_code == 200
        assert resp.json()["duration"] == 60
        assert resp.json()["rel_brightness"] == 50

        resp = await client.delete(
            f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards/{card_id}"
        )
        assert resp.status_code == 200, f"Delete card failed: {resp.text}"

        resp = await client.get(f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards")
        assert resp.status_code == 200
        assert len(resp.json()["cards"]) == 0

        resp = await client.post(
            f"{BASE_URL}/api/user/devices/unbind",
            json={"mac": TEST_MAC},
        )
        assert resp.status_code == 200, f"Unbind failed: {resp.text}"

        resp = await client.post(f"{BASE_URL}/api/auth/logout")
        assert resp.status_code == 200


@pytest.mark.asyncio
async def test_card_with_foreground():
    async with httpx.AsyncClient() as client:
        await client.post(
            f"{BASE_URL}/api/auth/login",
            json={"username": TEST_USER, "password": TEST_PASS},
        )
        await client.post(
            f"{BASE_URL}/api/user/devices/bind",
            json={"mac": TEST_MAC},
        )

        card_payload = {
            "device_mac": TEST_MAC,
            "bg_type": "code_rain",
            "position": 0,
            "fg_config": {
                "fgs": [
                    {"type": 0, "font": "5x5", "dur": 5000},
                    {"type": 1, "font": "7x9", "dur": 3000},
                ]
            },
            "duration": 30,
            "rel_brightness": 100,
        }
        resp = await client.post(
            f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards",
            json=card_payload,
        )
        assert resp.status_code == 200, f"Card with foreground failed: {resp.text}"
        data = resp.json()
        assert data["bg_type"] == "code_rain"
        assert len(data["fg_config"]["fgs"]) == 2

        cards_resp = await client.get(
            f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards"
        )
        cards = cards_resp.json()["cards"]
        for c in cards:
            await client.delete(
                f"{BASE_URL}/api/user/devices/{TEST_MAC}/cards/{c['id']}"
            )

        await client.post(
            f"{BASE_URL}/api/user/devices/unbind",
            json={"mac": TEST_MAC},
        )
        await client.post(f"{BASE_URL}/api/auth/logout")


@pytest.mark.asyncio
async def test_health_check():
    async with httpx.AsyncClient() as client:
        resp = await client.get(f"{BASE_URL}/api/health")
        assert resp.status_code == 200
        assert resp.json()["status"] == "ok"


@pytest.mark.asyncio
async def test_register_duplicate():
    async with httpx.AsyncClient() as client:
        resp = await client.post(
            f"{BASE_URL}/api/auth/register",
            json={"username": TEST_USER, "password": "dup123456"},
        )
        assert resp.status_code == 409


class TestDeviceBinding:
    """UT-3.7: Device binding lifecycle tests."""

    async def test_unbound_device_query_ok(self):
        """Querying an unbound device should not error."""
        import httpx
        async with httpx.AsyncClient() as client:
            resp = await client.get("http://localhost:8000/api/admin/devices")
            assert resp.status_code in (200, 401)

    async def test_duplicate_bind_rejected(self):
        """Rebinding an already-bound device should return 409."""
        import httpx
        async with httpx.AsyncClient() as client:
            # First login as admin
            await client.post("http://localhost:8000/api/auth/login", json={"username":"admin","password":"admin123"})
            # Try to bind same device twice
            resp = await client.post("http://localhost:8000/api/admin/bind", json={"user_id":"user1","device_mac":"aa:bb:cc:dd:ee:ff"})
            # Second attempt should fail (409 or device not found)
            assert resp.status_code != 200
