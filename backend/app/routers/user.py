import uuid
import datetime
from fastapi import APIRouter, Request, HTTPException, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func, delete
from app.database import get_db
from app.models.user import User
from app.models.device import Device
from app.models.card import Card
from app.schemas.user import UserResponse
from app.schemas.device import DeviceResponse
from app.schemas.card import CardCreate, CardUpdate, CardResponse
from app.services.mqtt_bridge import mqtt_bridge

router = APIRouter()

VALID_BG_TYPES = {"fire", "code_rain", "water_ripple", "game_of_life", "hourglass", "pong_clock", "weather"}
BG_NO_FOREGROUND = {"water_ripple", "game_of_life", "hourglass"}
BG_FONT_RESTRICTED = {"fire": "3x5"}
MAX_CARDS = 9


def validate_card_constraints(mac: str, bg_type: str, fg_config: dict, position: int, existing_count: int):
    if bg_type not in VALID_BG_TYPES:
        raise HTTPException(status_code=400, detail=f"Invalid bg_type: {bg_type}. Must be one of {sorted(VALID_BG_TYPES)}")

    fgs = fg_config.get("fgs", []) if isinstance(fg_config, dict) else []
    fg_count = len(fgs)

    if existing_count >= MAX_CARDS:
        raise HTTPException(status_code=400, detail=f"Maximum {MAX_CARDS} cards per device reached")

    if bg_type in BG_NO_FOREGROUND and fg_count > 0:
        raise HTTPException(status_code=400, detail=f"bg_type '{bg_type}' does not allow foreground elements")

    if bg_type in BG_FONT_RESTRICTED and fg_count > 0:
        required_font = BG_FONT_RESTRICTED[bg_type]
        for fg in fgs:
            font = fg.get("font", "")
            if font != required_font:
                raise HTTPException(
                    status_code=400,
                    detail=f"bg_type '{bg_type}' requires all foreground fonts to be '{required_font}'",
                )

    for fg in fgs:
        font = fg.get("font", "")
        if font and font not in {"3x5", "5x5", "7x9", "long"}:
            raise HTTPException(status_code=400, detail=f"Invalid font size: {font}")


async def push_cards_to_device(mac: str, db: AsyncSession):
    result = await db.execute(select(Card).where(Card.device_mac == mac).order_by(Card.position))
    cards = result.scalars().all()
    cards_payload = [CardResponse.model_validate(c).model_dump(mode="json") for c in cards]
    mqtt_bridge.push_config(mac, {"cards": cards_payload, "version": int(datetime.datetime.utcnow().timestamp())})


@router.get("/profile", response_model=UserResponse)
async def get_profile(request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalar_one_or_none()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return user


@router.get("/devices")
async def list_my_devices(request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.owner_id == user_id))
    devices = result.scalars().all()
    return {"devices": [DeviceResponse.model_validate(d) for d in devices]}


@router.post("/devices/bind")
async def bind_device(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    mac = body.get("mac")
    if not mac:
        raise HTTPException(status_code=400, detail="MAC address required")

    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if device.owner_id and str(device.owner_id) != user_id:
        raise HTTPException(status_code=409, detail="Device already bound to another user")

    device.owner_id = user_id
    await db.commit()
    return {"message": "Device bound successfully"}


@router.post("/devices/unbind")
async def unbind_device(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    mac = body.get("mac")
    if not mac:
        raise HTTPException(status_code=400, detail="MAC address required")

    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if str(device.owner_id) != user_id:
        raise HTTPException(status_code=403, detail="Device not bound to this user")

    device.owner_id = None
    await db.commit()
    return {"message": "Device unbound"}


@router.get("/devices/{mac}/cards")
async def list_cards(mac: str, request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if str(device.owner_id) != user_id:
        raise HTTPException(status_code=403, detail="You do not own this device")

    result = await db.execute(select(Card).where(Card.device_mac == mac).order_by(Card.position))
    cards = result.scalars().all()
    return {"cards": [CardResponse.model_validate(c) for c in cards]}


@router.post("/devices/{mac}/cards", response_model=CardResponse)
async def create_card(mac: str, body: CardCreate, request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if str(device.owner_id) != user_id:
        raise HTTPException(status_code=403, detail="You do not own this device")

    count_result = await db.execute(select(func.count()).select_from(Card).where(Card.device_mac == mac))
    existing_count = count_result.scalar() or 0

    validate_card_constraints(mac, body.bg_type, body.fg_config, body.position, existing_count)

    card = Card(
        id=uuid.uuid4(),
        device_mac=mac,
        position=body.position,
        bg_type=body.bg_type,
        fg_config=body.fg_config,
        duration=body.duration,
        rel_brightness=body.rel_brightness,
    )
    db.add(card)
    await db.commit()
    await db.refresh(card)

    await push_cards_to_device(mac, db)
    return card


@router.put("/devices/{mac}/cards/{card_id}", response_model=CardResponse)
async def update_card(mac: str, card_id: uuid.UUID, body: CardUpdate, request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if str(device.owner_id) != user_id:
        raise HTTPException(status_code=403, detail="You do not own this device")

    result = await db.execute(select(Card).where(Card.id == card_id, Card.device_mac == mac))
    card = result.scalar_one_or_none()
    if not card:
        raise HTTPException(status_code=404, detail="Card not found")

    if body.position is not None:
        card.position = body.position
    if body.bg_type is not None:
        count_result = await db.execute(
            select(func.count()).select_from(Card).where(Card.device_mac == mac, Card.id != card_id)
        )
        existing_count = count_result.scalar() or 0
        validate_card_constraints(mac, body.bg_type, body.fg_config if body.fg_config is not None else card.fg_config, body.position if body.position is not None else card.position, existing_count)
        card.bg_type = body.bg_type
    if body.fg_config is not None:
        card.fg_config = body.fg_config
    if body.duration is not None:
        card.duration = body.duration
    if body.rel_brightness is not None:
        card.rel_brightness = body.rel_brightness

    await db.commit()
    await db.refresh(card)

    await push_cards_to_device(mac, db)
    return card


@router.delete("/devices/{mac}/cards/{card_id}")
async def delete_card(mac: str, card_id: uuid.UUID, request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.state.user_id
    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    if str(device.owner_id) != user_id:
        raise HTTPException(status_code=403, detail="You do not own this device")

    result = await db.execute(select(Card).where(Card.id == card_id, Card.device_mac == mac))
    card = result.scalar_one_or_none()
    if not card:
        raise HTTPException(status_code=404, detail="Card not found")

    await db.delete(card)
    await db.commit()

    await push_cards_to_device(mac, db)
    return {"message": "Card deleted"}
