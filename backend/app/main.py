from fastapi import FastAPI
from starlette.middleware.sessions import SessionMiddleware
from app.database import engine, Base
from app.config import settings
from app.middleware.auth import auth_middleware
from app.routers import auth, admin, user, device, weather
from app.services.mqtt_bridge import mqtt_bridge

app = FastAPI(title="Desktop Clock Backend", version="0.1.0")

app.add_middleware(SessionMiddleware, secret_key=settings.SECRET_KEY, max_age=settings.SESSION_EXPIRE_HOURS * 3600)
app.middleware("http")(auth_middleware)


@app.on_event("startup")
async def startup():
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    mqtt_bridge.connect()


@app.on_event("shutdown")
async def shutdown():
    mqtt_bridge.disconnect()
    await engine.dispose()


@app.get("/api/health")
async def health_check():
    return {"status": "ok"}


app.include_router(auth.router, prefix="/api/auth", tags=["auth"])
app.include_router(admin.router, prefix="/api/admin", tags=["admin"])
app.include_router(user.router, prefix="/api/user", tags=["user"])
app.include_router(device.router, prefix="/api/device", tags=["device"])
app.include_router(weather.router, prefix="/api/weather", tags=["weather"])
