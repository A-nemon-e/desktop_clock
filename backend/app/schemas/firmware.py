import uuid
import datetime
from pydantic import BaseModel, Field


class FirmwareUpload(BaseModel):
    hw_rev: str = Field(max_length=8)
    version: str = Field(max_length=16)
    changelog: str = Field(default="", max_length=1024)


class FirmwareResponse(BaseModel):
    id: uuid.UUID
    hw_rev: str
    version: str
    file_path: str
    sha256: str
    changelog: str
    created_at: datetime.datetime

    model_config = {"from_attributes": True}


class OTACheckRequest(BaseModel):
    mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
    hw_rev: str = Field(max_length=8)
    current_version: str = Field(max_length=16)


class OTACheckResponse(BaseModel):
    has_update: bool
    version: str | None = None
    firmware_url: str | None = None
    sha256: str | None = None
    file_size: int | None = None
