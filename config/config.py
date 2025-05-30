import os
import asyncio
from fastapi.security.api_key import APIKeyHeader
from dotenv import load_dotenv

load_dotenv()

BEATS_PER_BAR = 4
API_KEY_HEADER = APIKeyHeader(name="X-API-Key")
API_KEYS = (
    os.getenv("DJ_IA_API_KEYS", "").split(",") if os.getenv("DJ_IA_API_KEYS") else []
)
ENVIRONMENT = os.environ.get("ENVIRONMENT") or "dev"
llm_lock = asyncio.Lock()
audio_lock = asyncio.Lock()
