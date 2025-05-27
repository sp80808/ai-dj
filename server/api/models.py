from pydantic import BaseModel
from typing import List, Optional


class GenerateRequest(BaseModel):
    prompt: str
    style: str
    bpm: float
    key: Optional[str] = None
    measures: Optional[int] = 4
    preferred_stems: Optional[List[str]] = None
    generation_duration: Optional[int] = 6


class GenerateResponse(BaseModel):
    audio_data: str
    duration: float
    bpm: float
    key: str
    stems_used: Optional[List[str]] = None
    llm_reasoning: Optional[str] = None
    sample_rate: int
