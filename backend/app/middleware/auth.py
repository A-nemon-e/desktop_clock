from fastapi import Request, HTTPException
from sqlalchemy import select
from app.database import async_session
from app.models.device import Device

PUBLIC_PATHS = {
    "/api/auth/login",
    "/api/auth/logout",
    "/api/health",
    "/api/device/register",
    "/api/device/status",
    "/api/device/config_report",
    "/api/device/ota_status",
    "/api/weather/query",
    "/api/weather/push",
}
ADMIN_PATHS_PREFIX = "/api/admin"
DEVICE_PATHS_PREFIX = "/api/device"


async def auth_middleware(request: Request, call_next):
    path = request.url.path

    if path in PUBLIC_PATHS or path.startswith("/docs") or path.startswith("/openapi.json"):
        return await call_next(request)

    # Device auth via X-Device-MAC header
    if path.startswith(DEVICE_PATHS_PREFIX):
        device_mac = request.headers.get("X-Device-MAC")
        if not device_mac:
            raise HTTPException(status_code=401, detail="X-Device-MAC header required")

        async with async_session() as session:
            result = await session.execute(select(Device).where(Device.mac == device_mac))
            device = result.scalar_one_or_none()

        if not device:
            raise HTTPException(status_code=403, detail="Device not registered")

        request.state.device_mac = device_mac
        request.state.device = device
        return await call_next(request)

    # Admin auth via session
    if path.startswith(ADMIN_PATHS_PREFIX):
        user_id = request.session.get("user_id")
        is_admin = request.session.get("is_admin", False)
        if not user_id or not is_admin:
            raise HTTPException(status_code=403, detail="Admin access required")
        request.state.user_id = user_id
        return await call_next(request)

    # General user auth via session
    user_id = request.session.get("user_id")
    if not user_id:
        raise HTTPException(status_code=401, detail="Not authenticated")

    request.state.user_id = user_id
    return await call_next(request)
