import asyncio
from fastapi.security.api_key import APIKeyHeader
from typing import List

API_KEY_HEADER = APIKeyHeader(name="X-API-Key")
API_KEYS: List[str] = []
ENVIRONMENT: str = "dev"
llm_lock = asyncio.Lock()
audio_lock = asyncio.Lock()


def init_config(api_keys: List[str] = None, environment: str = "dev"):
    """Initialize configuration with parameters from main.py"""
    global API_KEYS, ENVIRONMENT
    API_KEYS = api_keys or []
    ENVIRONMENT = environment
