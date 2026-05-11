import os
import hashlib
import logging
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from app.config import settings
from app.models.firmware import Firmware
from app.models.device import Device
from app.services.mqtt_bridge import mqtt_bridge

logger = logging.getLogger("ota_svc")


async def check_for_update(hw_rev: str, current_version: str, db: AsyncSession) -> dict:
    result = await db.execute(
        select(Firmware)
        .where(Firmware.hw_rev == hw_rev)
        .order_by(Firmware.created_at.desc())
    )
    firmwares = result.scalars().all()

    for fw in firmwares:
        if _version_greater(fw.version, current_version):
            file_path = os.path.join(settings.FIRMWARE_STORAGE_PATH, fw.file_path)
            file_size = os.path.getsize(file_path) if os.path.exists(file_path) else 0
            return {
                "has_update": True,
                "version": fw.version,
                "firmware_url": f"/api/device/firmware/download/{fw.id}",
                "sha256": fw.sha256,
                "file_size": file_size,
            }

    return {"has_update": False, "version": None, "firmware_url": None, "sha256": None, "file_size": None}


async def trigger_ota(mac: str, db: AsyncSession) -> dict:
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise ValueError(f"Device {mac} not found")

    update_info = await check_for_update(device.hw_rev, device.fw_version or "0.0.0", db)
    if not update_info["has_update"]:
        return {"status": "up_to_date"}

    ota_payload = {
        "version": update_info["version"],
        "url": update_info["firmware_url"],
        "sha256": update_info["sha256"],
        "size": update_info["file_size"],
    }
    mqtt_bridge.push_ota(mac, ota_payload)
    return {"status": "ota_triggered", "version": update_info["version"]}


def compute_sha256(file_path: str) -> str:
    sha = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            sha.update(chunk)
    return sha.hexdigest()


def _version_greater(v1: str, v2: str) -> bool:
    parts1 = [int(x) for x in v1.split(".")]
    parts2 = [int(x) for x in v2.split(".")]
    max_len = max(len(parts1), len(parts2))
    parts1.extend([0] * (max_len - len(parts1)))
    parts2.extend([0] * (max_len - len(parts2)))
    for p1, p2 in zip(parts1, parts2):
        if p1 > p2:
            return True
        if p1 < p2:
            return False
    return False
