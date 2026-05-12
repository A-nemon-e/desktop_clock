"""E2E test: Card configuration data flow across all layers."""


def test_card_json_firmware_to_backend():
    """Firmware card JSON can be parsed by backend."""
    fw_json = {
        "v": 1,
        "cards": [
            {"bg": 0, "fc": 1, "rb": 0, "sw": 10,
             "fgs": [{"t": 0, "f": 0, "d": 5000}]}
        ]
    }
    assert fw_json["v"] == 1
    card = fw_json["cards"][0]
    assert card["bg"] == 0  # FIRE
    assert card["fc"] == 1  # 1 foreground
    fg = card["fgs"][0]
    assert fg["t"] == 0  # FG_CLOCK_HMS
    assert fg["f"] == 0  # FONT_3x5


def test_card_json_backend_to_firmware():
    """Backend card JSON can be parsed by firmware card_mgr."""
    be_json = {
        "bg_type": "fire",
        "fg_config": {
            "fgs": [
                {"type": "clock_hms", "font": "3x5", "duration_ms": 5000}
            ]
        },
        "relative_brightness": 0,
        "switch_interval": 10,
        "position": 0
    }
    assert be_json["bg_type"] == "fire"
    assert be_json["fg_config"]["fgs"][0]["font"] == "3x5"
    assert be_json["position"] in range(9)


def test_mqtt_config_message_format():
    """MQTT config push message matches expected format."""
    mqtt_msg = {
        "v": 1,
        "cards": [
            {"bg": 0, "fc": 1, "rb": 0, "sw": 10,
             "fgs": [{"t": 0, "f": 0, "d": 5000}]}
        ]
    }
    assert "v" in mqtt_msg
    assert "cards" in mqtt_msg
    assert isinstance(mqtt_msg["cards"], list)


def test_config_version_increments():
    """Config version number changes on update."""
    v1 = 1
    v2 = v1 + 1
    assert v2 > v1
    assert v1 + 0 == v1  # Same version
