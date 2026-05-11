import uuid
import datetime
from sqlalchemy import Column, String, Integer, DateTime, JSON, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from app.database import Base


class Card(Base):
    __tablename__ = "cards"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    device_mac = Column(String(17), ForeignKey("devices.mac"), nullable=False)
    position = Column(Integer, nullable=False)  # 0-8 display order
    bg_type = Column(String(32), nullable=False)  # bg effect identifier
    fg_config = Column(JSON, default={})  # foreground layout config
    duration = Column(Integer, default=30)  # display seconds per card
    rel_brightness = Column(Integer, default=100)  # relative brightness %
    created_at = Column(DateTime, default=datetime.datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.datetime.utcnow, onupdate=datetime.datetime.utcnow)
