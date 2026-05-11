import uuid
import os
import hashlib
from fastapi import APIRouter, Request, HTTPException, Depends, UploadFile, File, Form
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func, delete
from passlib.hash import bcrypt
from app.database import get_db
from app.models.user import User
from app.models.device import Device
from app.models.card import Card
from app.models.firmware import Firmware
from app.schemas.user import UserCreate, UserResponse, UserListResponse
from app.config import settings

router = APIRouter()


@router.get("/users", response_model=UserListResponse)
async def list_users(request: Request, db: AsyncSession = Depends(get_db), skip: int = 0, limit: int = 50):
    count_result = await db.execute(select(func.count()).select_from(User))
    total = count_result.scalar()

    result = await db.execute(select(User).offset(skip).limit(limit))
    users = result.scalars().all()

    return UserListResponse(users=[UserResponse.model_validate(u) for u in users], total=total)


@router.post("/users", response_model=UserResponse)
async def create_user(body: UserCreate, request: Request, db: AsyncSession = Depends(get_db)):
    existing = await db.execute(select(User).where(User.username == body.username))
    if existing.scalar_one_or_none():
        raise HTTPException(status_code=409, detail="Username already exists")

    user = User(
        id=uuid.uuid4(),
        username=body.username,
        pwd_hash=bcrypt.hash(body.password),
        is_admin=False,
    )
    db.add(user)
    await db.commit()
    await db.refresh(user)
    return user


@router.delete("/users/{user_id}")
async def delete_user(user_id: uuid.UUID, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalar_one_or_none()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    if user.is_admin:
        raise HTTPException(status_code=403, detail="Cannot delete admin user")

    await db.delete(user)
    await db.commit()
    return {"message": "User deleted"}


@router.get("/devices")
async def list_all_devices(request: Request, db: AsyncSession = Depends(get_db), skip: int = 0, limit: int = 50):
    count_result = await db.execute(select(func.count()).select_from(Device))
    total = count_result.scalar()

    result = await db.execute(select(Device).offset(skip).limit(limit))
    devices = result.scalars().all()

    return {"devices": [{"mac": d.mac, "hw_rev": d.hw_rev, "fw_version": d.fw_version, "ip": d.ip, "region": d.region, "thin_mode": d.thin_mode, "last_seen": d.last_seen.isoformat() if d.last_seen else None} for d in devices], "total": total}


@router.post("/bind")
async def admin_bind_device(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    user_id = body.get("user_id")
    mac = body.get("mac")
    if not user_id or not mac:
        raise HTTPException(status_code=400, detail="user_id and mac required")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if device.owner_id and str(device.owner_id) != user_id:
        raise HTTPException(status_code=409, detail="Device already bound to another user")

    device.owner_id = user_id
    await db.commit()
    return {"message": "Device bound successfully", "mac": mac, "user_id": user_id}


@router.get("/devices/{mac}")
async def admin_get_device(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return {
        "mac": device.mac, "hw_rev": device.hw_rev, "fw_version": device.fw_version,
        "ip": device.ip, "region": device.region, "timezone": device.timezone,
        "owner_id": str(device.owner_id) if device.owner_id else None,
        "backend_urls": device.backend_urls,
        "latitude": device.latitude, "longitude": device.longitude,
        "thin_mode": device.thin_mode,
        "last_seen": device.last_seen.isoformat() if device.last_seen else None,
    }


@router.delete("/devices/{mac}")
async def admin_delete_device(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    await db.execute(delete(Card).where(Card.device_mac == mac))
    await db.delete(device)
    await db.commit()
    return {"message": "Device and associated cards deleted", "mac": mac}


@router.put("/backend-urls")
async def admin_set_backend_urls(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    primary = body.get("primary", "")
    backup = body.get("backup", "")
    mac = body.get("mac")

    if mac:
        result = await db.execute(select(Device).where(Device.mac == mac))
        device = result.scalar_one_or_none()
        if device:
            device.backend_urls = {"primary": primary, "backup": backup}
            await db.commit()
            mqtt_bridge.push_config(mac, {"backend_urls": {"primary": primary, "backup": backup}})
            return {"message": "Backend URLs updated for device", "mac": mac}

    devices_result = await db.execute(select(Device))
    devices = devices_result.scalars().all()
    for d in devices:
        d.backend_urls = {"primary": primary, "backup": backup}
        mqtt_bridge.push_config(d.mac, {"backend_urls": {"primary": primary, "backup": backup}})
    await db.commit()
    return {"message": f"Backend URLs broadcast to {len(devices)} devices", "primary": primary, "backup": backup}


@router.post("/firmwares")
async def upload_firmware(
    request: Request,
    file: UploadFile = File(...),
    hw_rev: str = Form(...),
    version: str = Form(...),
    changelog: str = Form(""),
    db: AsyncSession = Depends(get_db),
):
    if not file.filename or not file.filename.endswith(".bin"):
        raise HTTPException(status_code=400, detail="Only .bin firmware files accepted")

    os.makedirs(settings.FIRMWARE_STORAGE_PATH, exist_ok=True)
    safe_name = f"{hw_rev}_{version}_{file.filename}"
    dest_path = os.path.join(settings.FIRMWARE_STORAGE_PATH, safe_name)

    contents = await file.read()
    with open(dest_path, "wb") as f:
        f.write(contents)

    sha256_hash = hashlib.sha256(contents).hexdigest()

    result = await db.execute(
        select(Firmware).where(Firmware.hw_rev == hw_rev, Firmware.version == version)
    )
    existing = result.scalar_one_or_none()
    if existing:
        existing.file_path = safe_name
        existing.sha256 = sha256_hash
        existing.changelog = changelog
    else:
        firmware = Firmware(
            id=uuid.uuid4(),
            hw_rev=hw_rev,
            version=version,
            file_path=safe_name,
            sha256=sha256_hash,
            changelog=changelog,
        )
        db.add(firmware)

    await db.commit()
    return {
        "message": "Firmware uploaded",
        "hw_rev": hw_rev,
        "version": version,
        "file_path": safe_name,
        "sha256": sha256_hash,
        "size_bytes": len(contents),
    }


@router.get("/firmwares")
async def list_firmwares(request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Firmware).order_by(Firmware.created_at.desc()))
    firmwares = result.scalars().all()

    by_hw_rev: dict[str, list] = {}
    for fw in firmwares:
        by_hw_rev.setdefault(fw.hw_rev, []).append({
            "id": str(fw.id), "version": fw.version,
            "file_path": fw.file_path, "sha256": fw.sha256,
            "changelog": fw.changelog,
            "created_at": fw.created_at.isoformat() if fw.created_at else None,
        })

    return {"firmwares": by_hw_rev, "total": len(firmwares)}


@router.post("/devices/{mac}/ota")
async def admin_trigger_ota(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json() if request.headers.get("content-type") == "application/json" else {}
    target_version = body.get("version")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if not device.owner_id:
        raise HTTPException(status_code=400, detail="Device not yet bound to a user")

    update_info = await check_for_update(device.hw_rev, device.fw_version or "0.0.0", db)
    if not update_info["has_update"] and not target_version:
        return {"status": "up_to_date", "mac": mac, "current_version": device.fw_version}

    ota_payload = {
        "version": target_version or update_info["version"],
        "url": update_info["firmware_url"],
        "sha256": update_info["sha256"],
        "size": update_info["file_size"],
    }
    mqtt_bridge.push_ota(mac, ota_payload)
    return {"status": "ota_triggered", "mac": mac, "payload": ota_payload}


@router.get("/devices/{mac}/ota-status")
async def admin_ota_status(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    update_info = await check_for_update(device.hw_rev, device.fw_version or "0.0.0", db)
    return {
        "mac": mac,
        "current_version": device.fw_version,
        "latest_version": update_info.get("version"),
        "has_update": update_info["has_update"],
        "last_seen": device.last_seen.isoformat() if device.last_seen else None,
        "status": "idle" if not update_info["has_update"] else "update_available",
    }
