import time
import os
import librosa
import hashlib
from fastapi import APIRouter, HTTPException, Depends, Security, Request
from fastapi.responses import Response
from .models import GenerateRequest
from config.config import API_KEYS, API_KEY_HEADER, ENVIRONMENT, audio_lock, llm_lock
from server.api.api_request_handler import APIRequestHandler


router = APIRouter()


def get_user_id_from_api_key(api_key):
    return hashlib.sha256(api_key.encode()).hexdigest()[:16]


def get_dj_system(request: Request):
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system
    raise RuntimeError("No DJSystem instance found in FastAPI application!")


async def verify_api_key(api_key: str = Security(API_KEY_HEADER)):
    if ENVIRONMENT == "dev":
        return "dev-bypass"
    if not API_KEYS:
        raise HTTPException(status_code=500, detail="No API keys configured")
    if api_key not in API_KEYS:
        raise HTTPException(status_code=403, detail="Invalid API key")
    return api_key


@router.post("/verify_key")
async def verify_key(_: str = Depends(verify_api_key)):
    return {"status": "valid", "message": "API Key valid"}


@router.post("/generate")
async def generate_loop(
    request: GenerateRequest,
    api_key: str = Depends(verify_api_key),
    dj_system=Depends(get_dj_system),
):
    try:
        request_id = int(time.time())
        print(f"\n===== 🎵 QUERY #{request_id} =====")
        print(f"📝 '{request.prompt}' | {request.bpm} BPM | {request.key}")
        user_id = get_user_id_from_api_key(api_key)
        handler = APIRequestHandler(dj_system)
        async with llm_lock:
            handler.setup_llm_session(request, request_id, user_id)
            llm_decision = handler.get_llm_decision(request_id)
        async with audio_lock:
            audio, _ = handler.generate_simple(request, llm_decision, request_id)
            processed_path, used_stems = handler.process_audio_pipeline(
                audio, request, request_id
            )
        audio_data, sr = librosa.load(processed_path, sr=None)
        duration = len(audio_data) / sr

        with open(processed_path, "rb") as f:
            wav_data = f.read()

        print(f"[{request_id}] ✅ SUCCESS: {duration:.1f}")

        return Response(
            content=wav_data,
            media_type="audio/wav",
            headers={
                "X-Duration": str(duration),
                "X-BPM": str(request.bpm),
                "X-Key": str(request.key or ""),
                "X-Sample-Rate": "48000",
                "X-Stems-Used": ",".join(used_stems) if used_stems else "",
            },
        )

    except Exception as e:
        print(f"❌ ERROR #{request_id}: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

    finally:
        if os.path.exists(processed_path):
            os.remove(processed_path)
