from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from sqlalchemy.orm import DeclarativeBase
from app.config import settings


class Base(DeclarativeBase):
    pass


_engine = None
_async_session_maker = None


async def get_engine():
    global _engine
    if _engine is None:
        _engine = create_async_engine(
            settings.DATABASE_URL,
            echo=False,
            pool_size=20,
            max_overflow=10,
        )
    return _engine


async def get_session_maker():
    global _async_session_maker
    if _async_session_maker is None:
        engine = await get_engine()
        _async_session_maker = async_sessionmaker(
            engine, class_=AsyncSession, expire_on_commit=False
        )
    return _async_session_maker


async def get_db():
    maker = await get_session_maker()
    async with maker() as session:
        try:
            yield session
        finally:
            await session.close()
