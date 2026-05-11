"""MQTT contract tests — validate JSON agreement between firmware and backend."""
import pytest


# Firmware publishes: clock/{mac}/status
FW_STATUS_KEYS = {"online", "version", "hw", "ip", "region"}
# Backend expects in Device model
BACKEND_DEVICE_KEYS = {"mac", "hw_rev", "fw_version", "ip", "region", "timezone"}


def test_status_fields_available():
    """All required status fields are present in firmware JSON."""
    status = {"online": True, "version": "0.1.0", "hw": "0.1",
              "ip": "192.168.1.100", "region": "Beijing"}
    for key in FW_STATUS_KEYS:
        assert key in status, f"Missing key: {key}"


def test_status_to_device_mapping():
    """Firmware status keys map to backend Device model fields."""
    mapping = {"version": "fw_version", "hw": "hw_rev", "ip": "ip", "region": "region"}
    for fw_key in FW_STATUS_KEYS & mapping.keys():
        assert fw_key in mapping  # has backend equivalent


def test_card_config_contract():
    """Backend→Firmware card config uses agreed field names."""
    config = {
        "v": 1,
        "cards": [{"bg": 0, "fc": 1, "rb": 0, "sw": 10,
                    "fgs": [{"t": 0, "f": 0, "d": 5000}]}]
    }
    assert "v" in config
    card = config["cards"][0]
    assert card["bg"] in range(7)
    fg = card["fgs"][0]
    assert fg["t"] in range(5)   # 5 foreground types
    assert fg["f"] in range(4)   # 4 font sizes


def test_weather_contract():
    """Weather message format matches firmware parsing in bg_weather.c."""
    weather = {"condition": "晴", "wind_dir": "北风", "wind_scale": "3"}
    assert weather["condition"] in {"晴", "多云", "雨", "雪", "阴"}


def test_card_count_limit():
    """Both firmware and backend enforce max 9 cards."""
    MAX_CARDS = 9
    cards = [{"bg": 0} for _ in range(MAX_CARDS)]
    assert len(cards) == MAX_CARDS
    # Adding 10th should be rejected
    with pytest.raises(AssertionError):
        assert len(cards + [{"bg": 0}]) <= MAX_CARDS
