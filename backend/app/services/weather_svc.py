import datetime
import logging
import httpx
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from app.config import settings
from app.models.weather import WeatherCache

logger = logging.getLogger("weather_svc")

QWEATHER_URL = f"{settings.QWEATHER_BASE_URL}/weather/now"
CACHE_TTL = datetime.timedelta(minutes=15)
QWEATHER_API_KEY = settings.QWEATHER_API_KEY


async def get_weather(region: str, location_id: str, db: AsyncSession) -> dict:
    result = await db.execute(select(WeatherCache).where(WeatherCache.region == region))
    cache = result.scalar_one_or_none()

    if cache and (datetime.datetime.utcnow() - cache.updated_at) < CACHE_TTL:
        return {
            "region": region,
            "location_id": cache.location_id,
            "weather": cache.weather_json,
            "cached": True,
            "updated_at": cache.updated_at,
        }

    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.get(
                QWEATHER_URL,
                params={"location": location_id, "key": QWEATHER_API_KEY},
            )
            resp.raise_for_status()
            data = resp.json()

        if data.get("code") != "200":
            logger.error(f"HeFeng API error: {data}")
            if cache:
                return {
                    "region": region,
                    "location_id": cache.location_id,
                    "weather": cache.weather_json,
                    "cached": True,
                    "updated_at": cache.updated_at,
                }
            raise RuntimeError(f"Weather API error: {data}")

        now = data["now"]
        weather_json = {
            "condition": now.get("text", ""),
            "temp": now.get("temp", ""),
            "feels_like": now.get("feelsLike", ""),
            "wind_dir": now.get("windDir", ""),
            "wind_scale": now.get("windScale", ""),
            "humidity": now.get("humidity", ""),
            "icon": now.get("icon", ""),
        }

        if not cache:
            cache = WeatherCache(region=region, location_id=location_id, weather_json=weather_json)
            db.add(cache)
        else:
            cache.location_id = location_id
            cache.weather_json = weather_json
            cache.updated_at = datetime.datetime.utcnow()
        await db.commit()

        return {
            "region": region,
            "location_id": location_id,
            "weather": weather_json,
            "cached": False,
            "updated_at": cache.updated_at if hasattr(cache, "updated_at") else datetime.datetime.utcnow(),
        }

    except httpx.HTTPError as e:
        logger.error(f"HTTP error fetching weather: {e}")
        if cache:
            return {
                "region": region,
                "location_id": cache.location_id,
                "weather": cache.weather_json,
                "cached": True,
                "updated_at": cache.updated_at,
            }
        raise
