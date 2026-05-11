from app.schemas.user import UserCreate, UserLogin, UserResponse, UserListResponse
from app.schemas.device import DeviceRegister, DeviceUpdate, DeviceBind, DeviceResponse, DeviceStatusReport
from app.schemas.card import CardCreate, CardUpdate, CardResponse, CardSyncPayload
from app.schemas.firmware import FirmwareUpload, FirmwareResponse, OTACheckRequest, OTACheckResponse
from app.schemas.weather import WeatherRequest, WeatherResponse

__all__ = [
    "UserCreate", "UserLogin", "UserResponse", "UserListResponse",
    "DeviceRegister", "DeviceUpdate", "DeviceBind", "DeviceResponse", "DeviceStatusReport",
    "CardCreate", "CardUpdate", "CardResponse", "CardSyncPayload",
    "FirmwareUpload", "FirmwareResponse", "OTACheckRequest", "OTACheckResponse",
    "WeatherRequest", "WeatherResponse",
]
