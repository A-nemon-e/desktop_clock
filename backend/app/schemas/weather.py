import datetime
from pydantic import BaseModel, Field


class WeatherRequest(BaseModel):
    mac: str = Field(pattern=r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
    region: str | None = Field(default=None, max_length=128)
    location_id: str | None = Field(default=None, max_length=32)


class WeatherResponse(BaseModel):
    region: str
    location_id: str
    weather: dict
    cached: bool
    updated_at: datetime.datetime
