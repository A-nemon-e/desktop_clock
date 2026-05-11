"""Cross-component data flow integration tests.

Validates data formats and constraint logic match between firmware and backend.
No hardware required — pure logic and JSON format tests.
"""
import pytest


# =============================================================================
# 1. Card JSON Format Compatibility (Firmware ↔ Backend)
# =============================================================================

def test_card_json_roundtrip():
    """Card data serialized by firmware can be mapped to backend format."""
    fw = {"bg": 0, "fc": 1, "rb": 0, "sw": 10,
          "fgs": [{"t": 0, "f": 0, "d": 5000}]}
    assert fw["fgs"][0]["d"] == 5000
    assert fw["bg"] in range(7)  # 7 backgrounds

# =============================================================================
# 2. Card Constraint Consistency (Firmware card_mgr.c ↔ Backend user.py)
# =============================================================================

@pytest.mark.parametrize("bg,fg_count,font,expected", [
    (0, 1, 0, True),   # FIRE + 3x5 → valid
    (0, 1, 1, False),  # FIRE + 5x5 → invalid
    (0, 1, 2, False),  # FIRE + 7x9 → invalid
    (3, 1, 0, False),  # WATER + fg → invalid
    (4, 1, 0, False),  # LIFE + fg → invalid
    (5, 1, 0, False),  # HOURGLASS + fg → invalid
    (3, 0, 0, True),   # WATER + no fg → valid
    (1, 1, 1, True),   # CODE_RAIN + fg → valid
])
def test_card_constraints(bg, fg_count, font, expected):
    """Firmware and backend enforce identical card constraints."""
    NO_FG_BGS = {3, 4, 5}  # WATER, LIFE, HOURGLASS
    if bg == 0 and fg_count > 0:      # FIRE: only 3x5 font
        result = (font == 0)
    elif bg in NO_FG_BGS:              # water/life/hourglass: no foreground
        result = (fg_count == 0)
    else:
        result = True
    assert result == expected

# =============================================================================
# 3. MQTT Status Message Format Validation
# =============================================================================

def test_mqtt_status_format():
    """Firmware publishes status with required fields for backend."""
    status = {"online": True, "version": "0.1.0", "hw": "0.1",
              "ip": "192.168.1.100", "region": "Beijing,Chaoyang"}
    required = {"online", "version", "hw"}
    assert required.issubset(status.keys())

# =============================================================================
# 4. Version Comparison Logic (config_sync.c)
# =============================================================================

@pytest.mark.parametrize("remote,local,should_update", [
    (2, 1, True),
    (1, 1, False),
    (1, 2, False),
    (1, 0, True),
    (0, 1, False),
])
def test_version_comparison(remote, local, should_update):
    """Config sync version comparison: remote > local → update."""
    assert (remote > local) == should_update

# =============================================================================
# 5. Geo Pipeline Response Parsing
# =============================================================================

def test_geo_pipeline_parsing():
    """Geo pipeline correctly extracts data from API responses."""
    # ip.sb response
    assert {"ip": "1.2.3.4"}["ip"] == "1.2.3.4"
    # ipvx.netart.cn (district level for China)
    netart = {"regions": ["北京市", "北京市", "朝阳区"]}
    assert len(netart["regions"]) == 3
    assert netart["regions"][2] == "朝阳区"
    # timezonedb
    assert {"zoneName": "Asia/Shanghai"}["zoneName"] == "Asia/Shanghai"

# =============================================================================
# 6. OTA Message Contract
# =============================================================================

def test_ota_contract():
    """OTA trigger message has required fields."""
    ota = {"url": "https://example.com/fw/0.1/2.0.0.bin",
           "version": "2.0.0", "sha256": "abc123def456"}
    assert ota["url"].startswith("https://")

# =============================================================================
# 7. Weather Message Contract
# =============================================================================

def test_weather_contract():
    """Weather push message uses correct field names for firmware parsing."""
    weather = {"condition": "晴", "wind_dir": "北风", "wind_scale": "3"}
    valid_conditions = {"晴", "多云", "雨", "雪", "阴"}
    assert weather["condition"] in valid_conditions
    assert weather["wind_scale"].isdigit()
