import datetime
from sqlalchemy import Column, String, DateTime, Float, JSON, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from app.database import Base


class Device(Base):
    __tablename__ = "devices"

    mac = Column(String(17), primary_key=True)
    hw_rev = Column(String(8), default="0.1")
    fw_version = Column(String(16))
    ip = Column(String(45))  # IPv6-compatible
    region = Column(String(128))
    timezone = Column(String(64))
    owner_id = Column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=True)
    backend_urls = Column(JSON, default={"primary": "", "backup": ""})
    latitude = Column(Float, nullable=True)
    longitude = Column(Float, nullable=True)
    thin_mode = Column(String(16), default="fat")  # fat / thin
    last_seen = Column(DateTime, default=datetime.datetime.utcnow)
