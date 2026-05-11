FROM python:3.12-slim

WORKDIR /app

# System dependencies for asyncpg + paho-mqtt
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc libpq-dev libc-dev \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

# Create non-root user
RUN useradd -m -u 1000 clock && chown -R clock:clock /app
USER clock

EXPOSE 8000

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
