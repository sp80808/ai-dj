from pydantic import BaseModel
from typing import List, Optional


class GenerateRequest(BaseModel):
    prompt: str
    bpm: float
    key: Optional[str] = None
    measures: Optional[int] = 4
    preferred_stems: Optional[List[str]] = None
    generation_duration: Optional[int] = 6
    server_side_pre_treatment: Optional[bool] = False


class GenerateResponse(BaseModel):
    audio_data: str
    duration: float
    bpm: float
    key: str
    stems_used: Optional[List[str]] = None
    llm_reasoning: Optional[str] = None
    sample_rate: int
