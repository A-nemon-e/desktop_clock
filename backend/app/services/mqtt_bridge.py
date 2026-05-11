import json
import threading
import logging
import paho.mqtt.client as mqtt
from app.config import settings

logger = logging.getLogger("mqtt_bridge")


class MQTTBridge:
    def __init__(self):
        self.client = mqtt.Client(client_id="backend_bridge")
        self._connected = False
        self._lock = threading.Lock()

    def connect(self):
        try:
            self.client.on_connect = self._on_connect
            self.client.on_message = self._on_message
            self.client.on_disconnect = self._on_disconnect
            self.client.connect(settings.MQTT_BROKER, settings.MQTT_PORT, settings.MQTT_KEEPALIVE)
            self.client.loop_start()
            logger.info(f"MQTT bridge connecting to {settings.MQTT_BROKER}:{settings.MQTT_PORT}")
        except Exception as e:
            logger.error(f"MQTT connection failed: {e}")

    def disconnect(self):
        if self._connected:
            self.client.loop_stop()
            self.client.disconnect()

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self._connected = True
            logger.info("MQTT bridge connected")
            client.subscribe("clock/+/status")
            client.subscribe("clock/+/config_report")
            client.subscribe("clock/+/ota_status")
        else:
            logger.error(f"MQTT connection failed with rc={rc}")

    def _on_disconnect(self, client, userdata, rc):
        self._connected = False
        logger.warning(f"MQTT disconnected (rc={rc}), will auto-reconnect")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            logger.debug(f"MQTT received: {msg.topic} -> {payload}")
            self._handle_device_message(msg.topic, payload)
        except json.JSONDecodeError:
            logger.warning(f"Invalid JSON from {msg.topic}")

    def _handle_device_message(self, topic, payload):
        import asyncio
        from app.models.device import Device
        from app.database import async_session
        import datetime

        parts = topic.split("/")
        if len(parts) < 3:
            return
        mac = parts[1]
        msg_type = parts[2]

        async def update_device():
            async with async_session() as session:
                from sqlalchemy import select
                result = await session.execute(select(Device).where(Device.mac == mac))
                device = result.scalar_one_or_none()
                now = datetime.datetime.utcnow()

                if msg_type == "status":
                    if device:
                        device.fw_version = payload.get("fw_version", device.fw_version)
                        device.ip = payload.get("ip", device.ip)
                        device.region = payload.get("region", device.region)
                        device.timezone = payload.get("timezone", device.timezone)
                        device.thin_mode = payload.get("thin_mode", device.thin_mode)
                        device.last_seen = now
                    else:
                        device = Device(
                            mac=mac,
                            hw_rev=payload.get("hw_rev", "0.1"),
                            fw_version=payload.get("fw_version", ""),
                            ip=payload.get("ip", ""),
                            region=payload.get("region", ""),
                            timezone=payload.get("timezone", ""),
                            thin_mode=payload.get("thin_mode", "fat"),
                            last_seen=now,
                        )
                        session.add(device)
                elif msg_type == "config_report":
                    if device:
                        device.last_seen = now
                        region = payload.get("region")
                        timezone = payload.get("timezone")
                        if region is not None:
                            device.region = region
                        if timezone is not None:
                            device.timezone = timezone
                elif msg_type == "ota_status":
                    if device:
                        device.last_seen = now
                        progress = payload.get("progress")
                        success = payload.get("success")
                        error = payload.get("error")
                        fw_version = payload.get("fw_version")
                        if fw_version is not None:
                            device.fw_version = fw_version

                if device is not None:
                    await session.commit()

        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        loop.run_until_complete(update_device())

    def publish(self, topic, payload):
        if not self._connected:
            logger.warning(f"MQTT not connected, skipping publish to {topic}")
            return
        try:
            self.client.publish(topic, json.dumps(payload), qos=1, retain=True)
            logger.debug(f"MQTT published to {topic}")
        except Exception as e:
            logger.error(f"MQTT publish failed: {e}")

    def push_weather(self, mac, weather_data):
        topic = f"clock/{mac}/weather"
        self.publish(topic, weather_data)

    def push_config(self, mac, config_data):
        topic = f"clock/{mac}/config"
        self.publish(topic, config_data)

    def push_ota(self, mac, ota_data):
        topic = f"clock/{mac}/ota"
        self.publish(topic, ota_data)


mqtt_bridge = MQTTBridge()
