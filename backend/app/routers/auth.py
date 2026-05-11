import uuid
from fastapi import APIRouter, Request, HTTPException, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from passlib.hash import bcrypt
from app.database import get_db
from app.models.user import User
from app.schemas.user import UserCreate, UserLogin, UserResponse
from app.config import settings

router = APIRouter()


@router.post("/login")
async def login(body: UserLogin, request: Request, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(User).where(User.username == body.username))
    user = result.scalar_one_or_none()

    if not user or not bcrypt.verify(body.password, user.pwd_hash):
        raise HTTPException(status_code=401, detail="Invalid credentials")

    request.session["user_id"] = str(user.id)
    request.session["username"] = user.username
    request.session["is_admin"] = user.is_admin

    return {"message": "Login successful", "user_id": str(user.id)}


@router.post("/logout")
async def logout(request: Request):
    request.session.clear()
    return {"message": "Logged out"}


@router.post("/register")
async def register(body: UserCreate, db: AsyncSession = Depends(get_db)):
    existing = await db.execute(select(User).where(User.username == body.username))
    if existing.scalar_one_or_none():
        raise HTTPException(status_code=409, detail="Username already exists")

    user = User(
        id=uuid.uuid4(),
        username=body.username,
        pwd_hash=bcrypt.hash(body.password),
        is_admin=False,
    )
    db.add(user)
    await db.commit()

    return {"message": "Registration successful", "user_id": str(user.id)}


@router.get("/me", response_model=UserResponse)
async def get_me(request: Request, db: AsyncSession = Depends(get_db)):
    user_id = request.session.get("user_id")
    if not user_id:
        raise HTTPException(status_code=401, detail="Not authenticated")

    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalar_one_or_none()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")

    return user
