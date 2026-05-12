import os
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    DATABASE_URL: str = "postgresql+asyncpg://clock_user:clock_pass@localhost:5432/desktop_clock"
    TESTING: bool = os.environ.get("TESTING", "0") == "1"
    SECRET_KEY: str = "change-me-in-production-use-random-string"
    SESSION_EXPIRE_HOURS: int = 24
    QWEATHER_API_KEY: str = ""
    QWEATHER_BASE_URL: str = "https://devapi.qweather.com/v7"
    MQTT_BROKER: str = "localhost"
    MQTT_PORT: int = 1883
    MQTT_KEEPALIVE: int = 60
    ADMIN_DEFAULT_USERNAME: str = "admin"
    ADMIN_DEFAULT_PASSWORD: str = "admin123"
    FIRMWARE_STORAGE_PATH: str = "/data/firmware"

    model_config = {"env_file": ".env", "env_file_encoding": "utf-8"}

    @property
    def effective_database_url(self) -> str:
        if self.TESTING:
            return "sqlite+aiosqlite:///./test.db"
        return self.DATABASE_URL


settings = Settings()
