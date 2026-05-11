from fastapi import APIRouter, Request, HTTPException, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from app.database import get_db
from app.models.device import Device
from app.schemas.weather import WeatherRequest, WeatherResponse
from app.services.weather_svc import get_weather, QWEATHER_API_KEY

router = APIRouter()


@router.get("/query", response_model=WeatherResponse)
async def query_weather(request: Request, db: AsyncSession = Depends(get_db)):
    mac = request.query_params.get("mac")
    region = request.query_params.get("region")
    location_id = request.query_params.get("location_id")

    if not mac:
        raise HTTPException(status_code=400, detail="MAC required")

    if not QWEATHER_API_KEY:
        raise HTTPException(status_code=503, detail="Weather API key not configured")

    result = await db.execute(select(Device).where(Device.mac == mac))
    device = result.scalar_one_or_none()

    effective_region = region or (device.region if device else None)
    effective_location_id = location_id

    if not effective_location_id:
        raise HTTPException(status_code=400, detail="location_id required")

    weather_result = await get_weather(effective_region or "unknown", effective_location_id, db)

    return WeatherResponse(
        region=weather_result["region"],
        location_id=weather_result["location_id"],
        weather=weather_result["weather"],
        cached=weather_result["cached"],
        updated_at=weather_result["updated_at"],
    )


@router.post("/push")
async def push_weather(request: Request, db: AsyncSession = Depends(get_db)):
    body = await request.json()
    mac = body.get("mac")
    weather_data = body.get("weather")

    if not mac or not weather_data:
        raise HTTPException(status_code=400, detail="MAC and weather data required")

    topic = f"clock/{mac}/weather"
    from app.services.mqtt_bridge import mqtt_bridge
    mqtt_bridge.publish(topic, weather_data)

    return {"status": "ok"}
