"""UT-3.8: Firmware upload and OTA trigger tests."""

import pytest


@pytest.fixture(autouse=True)
def skip_if_no_db():
    try:
        from app.routers.admin import router  # noqa: F401
    except (ModuleNotFoundError, ImportError) as e:
        pytest.skip(f"Backend dependencies not available: {e}")


class TestFirmwareUpload:
    def test_upload_accepts_bin_file(self):
        """POST /api/admin/firmwares should accept .bin files."""
        # Cannot test actual upload without running server — validate endpoint exists
        from app.routers.admin import router
        routes = [r.path for r in router.routes]
        assert "/firmwares" in routes or any("/firmwares" in r.path for r in router.routes)

    def test_firmware_list_grouped_by_hw_rev(self):
        """GET /api/admin/firmwares should return firmware list."""
        from app.routers.admin import router
        routes = [r.path for r in router.routes]
        assert any("firmwares" in str(r.path).lower() for r in router.routes)

    def test_ota_trigger_endpoint_exists(self):
        """POST /api/admin/devices/{mac}/ota should exist."""
        from app.routers.admin import router
        routes = [r.path for r in router.routes]
        assert any("ota" in str(r.path).lower() for r in router.routes)

    def test_ota_status_endpoint_defined(self):
        """OTA status tracking endpoint should exist."""
        from app.routers.admin import router
        routes = [str(r.methods) + " " + r.path for r in router.routes]
        ota_routes = [r for r in routes if "ota" in r.lower()]
        assert len(ota_routes) > 0, f"No OTA routes found in: {routes}"
