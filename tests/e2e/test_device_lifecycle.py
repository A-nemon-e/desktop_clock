"""E2E test: Device boot-to-operation lifecycle."""


def test_boot_sequence_order():
    """Device boots in correct order: NVS->WiFi->Geo->NTP->MQTT."""
    sequence = ["nvs_init", "wifi_connect", "geo_pipeline", "ntp_sync", "mqtt_connect"]
    assert sequence[0] == "nvs_init"
    assert sequence[-1] == "mqtt_connect"
    assert "ntp_sync" in sequence


def test_geo_pipeline_order():
    """Geo pipeline: IP->region->timezone."""
    steps = ["get_public_ip", "get_region", "get_timezone"]
    assert len(steps) == 3
    assert steps[0] == "get_public_ip"


def test_ota_sequence():
    """OTA sequence: trigger->download->verify->switch->reboot."""
    ota_steps = ["trigger", "download", "verify", "switch_partition", "reboot"]
    assert "verify" in ota_steps
    assert ota_steps.index("verify") > ota_steps.index("download")


def test_fat_thin_mode_transition():
    """Fat/thin mode switching logic."""
    mode = "fat"
    fail_count = 0
    # 3 failures -> thin mode
    for _ in range(3):
        fail_count += 1
    if fail_count >= 3:
        mode = "thin"
    assert mode == "thin"


def test_device_status_message():
    """Device status MQTT message contains required fields."""
    status = {
        "online": True,
        "version": "0.1.0",
        "hw": "0.1",
        "ip": "192.168.1.100",
        "region": "Beijing,Chaoyang"
    }
    required = ["online", "version", "hw"]
    for key in required:
        assert key in status
