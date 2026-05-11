import uuid
import datetime
from sqlalchemy import Column, String, DateTime
from sqlalchemy.dialects.postgresql import UUID
from app.database import Base


class Firmware(Base):
    __tablename__ = "firmwares"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    hw_rev = Column(String(8), nullable=False, index=True)
    version = Column(String(16), nullable=False)
    file_path = Column(String(512), nullable=False)
    sha256 = Column(String(64), nullable=False)
    changelog = Column(String(1024), default="")
    created_at = Column(DateTime, default=datetime.datetime.utcnow)
