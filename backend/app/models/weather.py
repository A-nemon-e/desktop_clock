import datetime
from sqlalchemy import Column, String, DateTime, JSON
from app.database import Base


class WeatherCache(Base):
    __tablename__ = "weather_cache"

    region = Column(String(128), primary_key=True)
    location_id = Column(String(32), nullable=False)
    weather_json = Column(JSON, nullable=False)
    updated_at = Column(DateTime, default=datetime.datetime.utcnow)
