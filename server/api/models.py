from pydantic import BaseModel
from typing import List, Optional


class GenerateRequest(BaseModel):
    prompt: str
    bpm: float
    key: Optional[str] = None
    measures: Optional[int] = 4
    preferred_stems: Optional[List[str]] = None
    generation_duration: Optional[float] = 6.0
    sample_rate: Optional[float] = 48000.00


class VariationRequest(BaseModel):
    prompt: Optional[str] = None
    seed: Optional[int] = None
    variation_strength: Optional[float] = 0.5
    bpm: Optional[float] = None
    key: Optional[str] = None
    preferred_stems: Optional[List[str]] = None
    generation_duration: Optional[float] = None
    sample_rate: Optional[float] = None
