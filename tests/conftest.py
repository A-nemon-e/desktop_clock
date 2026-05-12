import pytest
import pytest_asyncio
import os

# Enable TESTING mode for all tests — must be set before any app imports
os.environ["TESTING"] = "1"


@pytest_asyncio.fixture
async def db():
    """Create all tables in test SQLite database, then drop after test."""
    from app.database import get_engine, Base
    engine = await get_engine()
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.drop_all)
