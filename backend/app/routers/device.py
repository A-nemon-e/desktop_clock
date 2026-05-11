import datetime
from fastapi import APIRouter, Request, HTTPException, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from app.database import get_db
from app.models.device import Device
from app.models.card import Card
from app.schemas.device import DeviceRegister, DeviceUpdate, DeviceResponse, DeviceStatusReport
from app.schemas.card import CardResponse, CardSyncPayload
from app.services.mqtt_bridge import mqtt_bridge

router = APIRouter()


@router.post("/register", response_model=DeviceResponse)
async def register_device(body: DeviceRegister, db: AsyncSession = Depends(get_db)):
    existing = await db.execute(select(Device).where(Device.mac == body.mac))
    device = existing.scalar_one_or_none()

    if device:
        device.fw_version = body.fw_version
        device.ip = body.ip
        device.last_seen = datetime.datetime.utcnow()
        await db.commit()
        await db.refresh(device)
        return device

    device = Device(
        mac=body.mac,
        hw_rev=body.hw_rev,
        fw_version=body.fw_version,
        ip=body.ip,
    )
    db.add(device)
    await db.commit()
    await db.refresh(device)
    return device


@router.get("/{mac}", response_model=DeviceResponse)
async def get_device(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    device_mac = getattr(request.state, "device_mac", None)
    if device_mac and device_mac != mac:
        raise HTTPException(status_code=403, detail="MAC mismatch")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    return device


@router.patch("/{mac}")
async def update_device(mac: str, body: DeviceUpdate, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    if body.region is not None:
        device.region = body.region
    if body.timezone is not None:
        device.timezone = body.timezone
    if body.backend_urls is not None:
        device.backend_urls = body.backend_urls
    if body.latitude is not None:
        device.latitude = body.latitude
    if body.longitude is not None:
        device.longitude = body.longitude
    if body.thin_mode is not None:
        device.thin_mode = body.thin_mode

    await db.commit()
    await db.refresh(device)
    return DeviceResponse.model_validate(device)


@router.post("/status")
async def update_status(body: DeviceStatusReport, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Device).where(Device.mac == body.mac))
    device = result.scalar_one_or_none()
    if device:
        device.fw_version = body.fw_version
        device.ip = body.ip
        device.thin_mode = body.thin_mode
        device.last_seen = datetime.datetime.utcnow()
    else:
        device = Device(
            mac=body.mac,
            fw_version=body.fw_version,
            ip=body.ip,
            thin_mode=body.thin_mode,
            last_seen=datetime.datetime.utcnow(),
        )
        db.add(device)
    await db.commit()
    return {"status": "ok"}


@router.post("/config_report")
async def config_report(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    mac = body.get("mac")
    version = body.get("version")
    if not mac:
        raise HTTPException(status_code=400, detail="MAC required")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if device:
        device.last_seen = datetime.datetime.utcnow()
        await db.commit()

    return {"status": "ok"}


@router.post("/ota_status")
async def ota_status_report(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    mac = body.get("mac")
    progress = body.get("progress", 0)
    success = body.get("success")
    error = body.get("error")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if device:
        device.last_seen = datetime.datetime.utcnow()
        await db.commit()

    return {"status": "ok", "mac": mac, "progress": progress}


@router.get("/{mac}/cards")
async def get_device_cards(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Card).where(Card.device_mac == mac).order_by(Card.position))
    cards = result.scalars().all()
    return {"cards": [CardResponse.model_validate(c) for c in cards]}


@router.post("/{mac}/cards/sync")
async def sync_cards(mac: str, payload: CardSyncPayload, request: Request, db: AsyncSession = Depends(get_db)):
    existing = await db.execute(select(Card).where(Card.device_mac == mac))
    for card in existing.scalars().all():
        await db.delete(card)

    for card_data in payload.cards:
        card = Card(
            device_mac=mac,
            position=card_data.position,
            bg_type=card_data.bg_type,
            fg_config=card_data.fg_config,
            duration=card_data.duration,
            rel_brightness=card_data.rel_brightness,
        )
        db.add(card)

    await db.commit()

    topic = f"clock/{mac}/config"
    mqtt_bridge.publish(topic, {"cards": [c.model_dump() for c in payload.cards], "version": payload.version})

    return {"message": "Cards synced and pushed to device"}
