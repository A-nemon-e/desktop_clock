import uuid
import datetime
from pydantic import BaseModel, Field


class CardCreate(BaseModel):
    device_mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
    position: int = Field(ge=0, le=8)
    bg_type: str = Field(max_length=32)
    fg_config: dict = {}
    duration: int = Field(default=30, ge=5, le=300)
    rel_brightness: int = Field(default=100, ge=0, le=100)


class CardUpdate(BaseModel):
    position: int | None = Field(default=None, ge=0, le=8)
    bg_type: str | None = Field(default=None, max_length=32)
    fg_config: dict | None = None
    duration: int | None = Field(default=None, ge=5, le=300)
    rel_brightness: int | None = Field(default=None, ge=0, le=100)


class CardResponse(BaseModel):
    id: uuid.UUID
    device_mac: str
    position: int
    bg_type: str
    fg_config: dict
    duration: int
    rel_brightness: int
    created_at: datetime.datetime
    updated_at: datetime.datetime

    model_config = {"from_attributes": True}


class CardSyncPayload(BaseModel):
    cards: list[CardResponse]
    device_mac: str
    version: int
