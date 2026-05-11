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
MAC_A = "11:11:11:11:11:11"
MAC_B = "22:22:22:22:22:22"


async def _register_and_login(client: httpx.AsyncClient, username: str, password: str):
    await client.post(
        f"{BASE_URL}/api/auth/register",
        json={"username": username, "password": password},
    )
    await client.post(
        f"{BASE_URL}/api/auth/login",
        json={"username": username, "password": password},
    )


async def _register_device(client: httpx.AsyncClient, mac: str):
    await client.post(
        f"{BASE_URL}/api/device/register",
        json={"mac": mac, "hw_rev": "0.1", "fw_version": "1.0.0", "ip": "10.0.0.1"},
    )


async def _bind_device(client: httpx.AsyncClient, mac: str):
    await client.post(f"{BASE_URL}/api/user/devices/bind", json={"mac": mac})


def _make_card(mac: str, pos: int, bg: str, fg_config: dict | None = None, **overrides):
    payload = {
        "device_mac": mac,
        "bg_type": bg,
        "position": pos,
        "fg_config": fg_config or {},
        "duration": 30,
        "rel_brightness": 80,
    }
    payload.update(overrides)
    return payload


class TestInvalidCardConstraints:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_fire_bg_rejects_5x5_font(self):
        async with httpx.AsyncClient() as client:
            user = f"err_fire_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:01"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(
                    mac, 0, "fire",
                    fg_config={"fgs": [{"type": 0, "font": "5x5", "dur": 5000}]},
                ),
            )
            assert resp.status_code == 400
            assert "3x5" in resp.json()["detail"]

    @pytest.mark.asyncio
    async def test_water_ripple_rejects_foregrounds(self):
        async with httpx.AsyncClient() as client:
            user = f"err_wr_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:02"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(
                    mac, 0, "water_ripple",
                    fg_config={"fgs": [{"type": 0, "font": "3x5", "dur": 5000}]},
                ),
            )
            assert resp.status_code == 400
            assert "foreground" in resp.json()["detail"].lower()

    @pytest.mark.asyncio
    async def test_game_of_life_rejects_foregrounds(self):
        async with httpx.AsyncClient() as client:
            user = f"err_gol_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:03"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(
                    mac, 0, "game_of_life",
                    fg_config={"fgs": [{"type": 0, "font": "3x5", "dur": 5000}]},
                ),
            )
            assert resp.status_code == 400

    @pytest.mark.asyncio
    async def test_hourglass_rejects_foregrounds(self):
        async with httpx.AsyncClient() as client:
            user = f"err_hg_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:04"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(
                    mac, 0, "hourglass",
                    fg_config={"fgs": [{"type": 0, "font": "3x5", "dur": 5000}]},
                ),
            )
            assert resp.status_code == 400

    @pytest.mark.asyncio
    async def test_invalid_bg_type(self):
        async with httpx.AsyncClient() as client:
            user = f"err_bg_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:05"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(mac, 0, "invalid_bg"),
            )
            assert resp.status_code == 400

    @pytest.mark.asyncio
    async def test_invalid_font_size(self):
        async with httpx.AsyncClient() as client:
            user = f"err_font_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:06"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(
                    mac, 0, "code_rain",
                    fg_config={"fgs": [{"type": 0, "font": "10x10", "dur": 5000}]},
                ),
            )
            assert resp.status_code == 400
            assert "font" in resp.json()["detail"].lower()


class TestMaxCards:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_max_9_cards_rejects_10th(self):
        async with httpx.AsyncClient() as client:
            user = f"err_max_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:10"
            await _register_device(client, mac)
            await _bind_device(client, mac)

            for pos in range(9):
                resp = await client.post(
                    f"{BASE_URL}/api/user/devices/{mac}/cards",
                    json=_make_card(mac, pos, "pong_clock"),
                )
                assert resp.status_code == 200, f"Card {pos} creation failed: {resp.text}"

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(mac, 7, "weather"),
            )
            assert resp.status_code == 400
            assert "9" in resp.json()["detail"]


class TestBindConflict:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_bind_already_bound_device(self):
        mac = "aa:bb:cc:dd:ee:20"
        async with httpx.AsyncClient() as client_a:
            user_a = f"bind_a_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client_a, user_a, "pass123456")
            await _register_device(client_a, mac)
            await _bind_device(client_a, mac)

        async with httpx.AsyncClient() as client_b:
            user_b = f"bind_b_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client_b, user_b, "pass123456")
            resp = await client_b.post(
                f"{BASE_URL}/api/user/devices/bind",
                json={"mac": mac},
            )
            assert resp.status_code == 409
            assert "already bound" in resp.json()["detail"].lower()


class TestUnauthenticatedAccess:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_card_create_without_login(self):
        async with httpx.AsyncClient() as client:
            resp = await client.post(
                f"{BASE_URL}/api/user/devices/aa:bb:cc:dd:ee:30/cards",
                json=_make_card("aa:bb:cc:dd:ee:30", 0, "weather"),
            )
            assert resp.status_code == 401

    @pytest.mark.asyncio
    async def test_profile_without_login(self):
        async with httpx.AsyncClient() as client:
            resp = await client.get(f"{BASE_URL}/api/user/profile")
            assert resp.status_code == 401

    @pytest.mark.asyncio
    async def test_admin_endpoint_requires_admin(self):
        async with httpx.AsyncClient() as client:
            user = f"noauth_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            resp = await client.get(f"{BASE_URL}/api/admin/users")
            assert resp.status_code == 403


class TestInvalidCredentials:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_login_wrong_password(self):
        async with httpx.AsyncClient() as client:
            user = f"badpw_{uuid.uuid4().hex[:6]}"
            await client.post(
                f"{BASE_URL}/api/auth/register",
                json={"username": user, "password": "correct"},
            )
            resp = await client.post(
                f"{BASE_URL}/api/auth/login",
                json={"username": user, "password": "wrongpass"},
            )
            assert resp.status_code == 401

    @pytest.mark.asyncio
    async def test_login_nonexistent_user(self):
        async with httpx.AsyncClient() as client:
            resp = await client.post(
                f"{BASE_URL}/api/auth/login",
                json={"username": "no_such_user_xyz", "password": "anything"},
            )
            assert resp.status_code == 401


class TestCardOwnership:
    @pytest.fixture(autouse=True)
    def _skip_if_no_server(self):
        if not _server_running():
            pytest.skip("Backend server not running on localhost:8000")

    @pytest.mark.asyncio
    async def test_cannot_create_card_for_unowned_device(self):
        async with httpx.AsyncClient() as client:
            user = f"owner_{uuid.uuid4().hex[:6]}"
            await _register_and_login(client, user, "pass123456")
            mac = "aa:bb:cc:dd:ee:40"
            await _register_device(client, mac)

            resp = await client.post(
                f"{BASE_URL}/api/user/devices/{mac}/cards",
                json=_make_card(mac, 0, "pong_clock"),
            )
            assert resp.status_code == 403


class TestCardConstraints:
    """UT-3.6: Verify card constraint validation on backend matches firmware."""

    @pytest.fixture(autouse=True)
    def _skip_if_no_backend(self):
        try:
            from app.routers.user import validate_card_constraints  # noqa: F401
        except (ModuleNotFoundError, ImportError) as e:
            pytest.skip(f"Backend dependencies not available: {e}")

    def test_water_rejects_foreground(self):
        """bg_type='water_ripple' with fg_count>0 must be rejected."""
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "water_ripple", {"fgs": [{"type":"clock_hms"}]}, 0, 0)
        assert "does not allow foreground" in str(exc.value)

    def test_game_of_life_rejects_foreground(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "game_of_life", {"fgs": [{"type":"date_compact"}]}, 0, 0)
        assert "does not allow foreground" in str(exc.value)

    def test_hourglass_rejects_foreground(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "hourglass", {"fgs": [{"type":"temp_humid"}]}, 0, 0)
        assert "does not allow foreground" in str(exc.value)

    def test_fire_requires_3x5_font(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "fire", {"fgs": [{"type":"clock_hms","font":"5x5"}]}, 0, 0)
        assert "requires all foreground fonts to be" in str(exc.value)

    def test_fire_accepts_3x5_font(self):
        from app.routers.user import validate_card_constraints
        # Should NOT raise
        validate_card_constraints("aa:bb", "fire", {"fgs": [{"type":"clock_hms","font":"3x5"}]}, 0, 0)

    def test_max_9_cards_enforced(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "fire", {"fgs": []}, 0, existing_count=9)
        assert "Maximum 9 cards" in str(exc.value)

    def test_invalid_font_rejected(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "code_rain", {"fgs": [{"type":"clock_hms","font":"10x20"}]}, 0, 0)
        assert "Invalid font size" in str(exc.value)

    def test_invalid_bg_rejected(self):
        from app.routers.user import validate_card_constraints
        import pytest
        with pytest.raises(Exception) as exc:
            validate_card_constraints("aa:bb", "invalid_bg", {}, 0, 0)
        assert "Invalid bg_type" in str(exc.value)
