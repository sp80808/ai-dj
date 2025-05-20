from pydantic import BaseModel
from typing import List, Optional


class GenerateRequest(BaseModel):
    prompt: str
    style: str
    bpm: float
    key: Optional[str] = None
    measures: Optional[int] = 4
    preferred_stems: Optional[List[str]] = None


class GenerateResponse(BaseModel):
    file_path: str
    duration: float
    bpm: float
    key: str
    stems_used: Optional[List[str]]


class InitConfig(BaseModel):
    api_key: Optional[str] = None
    model_name: Optional[str] = None
    output_dir: Optional[str] = None
