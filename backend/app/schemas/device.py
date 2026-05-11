import datetime
import uuid
from pydantic import BaseModel, Field


class DeviceRegister(BaseModel):
    mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
    hw_rev: str = Field(default="0.1", max_length=8)
    fw_version: str = Field(max_length=16)
    ip: str = Field(max_length=45)


class DeviceUpdate(BaseModel):
    region: str | None = Field(default=None, max_length=128)
    timezone: str | None = Field(default=None, max_length=64)
    backend_urls: dict | None = None
    latitude: float | None = None
    longitude: float | None = None
    thin_mode: str | None = Field(default=None, pattern=r"^(fat|thin)$")


class DeviceBind(BaseModel):
    mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


class DeviceResponse(BaseModel):
    mac: str
    hw_rev: str
    fw_version: str | None
    ip: str | None
    region: str | None
    timezone: str | None
    owner_id: uuid.UUID | None
    backend_urls: dict
    latitude: float | None
    longitude: float | None
    thin_mode: str
    last_seen: datetime.datetime

    model_config = {"from_attributes": True}


class DeviceStatusReport(BaseModel):
    mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
    fw_version: str = Field(max_length=16)
    ip: str = Field(max_length=45)
    thin_mode: str = Field(pattern=r"^(fat|thin)$")
    status: str = "online"
