import time
import os
from os import walk
import random
import librosa
import hashlib
from fastapi import APIRouter, HTTPException, Depends, Request
from fastapi.security import APIKeyHeader
from fastapi.responses import Response
from .models import GenerateRequest
from config.config import API_KEYS, ENVIRONMENT, audio_lock, llm_lock, IS_TEST
from server.api.api_request_handler import APIRequestHandler
from core.api_keys_manager import check_api_key_status, increment_api_key_usage

router = APIRouter()


def get_user_id_from_api_key(api_key):
    return hashlib.sha256(api_key.encode()).hexdigest()[:16]


def get_dj_system(request: Request):
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system
    raise RuntimeError("No DJSystem instance found in FastAPI application!")


api_key_header = APIKeyHeader(name="X-API-Key", auto_error=False)


def create_error_response(
    error_code: str, message: str, status_code: int = 400
) -> HTTPException:
    return HTTPException(
        status_code=status_code,
        detail={"error": {"code": error_code, "message": message}},
    )


async def verify_api_key(api_key: str = Depends(api_key_header)):
    if ENVIRONMENT == "dev":
        return "dev-bypass"

    if not API_KEYS:
        raise create_error_response("SERVER_ERROR", "No API keys configured", 500)

    if not api_key:
        raise create_error_response("INVALID_KEY", "API Key required", 401)

    is_valid, error_code, key_info = check_api_key_status(api_key)

    if not is_valid:
        if error_code == "INVALID_KEY":
            raise create_error_response("INVALID_KEY", "Invalid API key", 403)
        elif error_code == "KEY_EXPIRED":
            raise create_error_response(
                "KEY_EXPIRED",
                "Your API key has expired. Please contact support to renew your access.",
                403,
            )
        elif error_code == "CREDITS_EXHAUSTED":
            used = key_info.get("credits_used", 0)
            total = key_info.get("total_credits", 50)
            raise create_error_response(
                "CREDITS_EXHAUSTED",
                f"No generation credits remaining. You have used {used}/{total} generations for this API key.",
                403,
            )

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
    processed_path = None
    try:
        request_id = int(time.time())
        print(f"===== 🎵 QUERY #{request_id} =====")
        print(
            f"📝 '{request.prompt}' | {request.bpm} BPM | {request.key} | SAMPLE RATE {str(int(request.sample_rate))}"
        )
        if not request.prompt or len(request.prompt.strip()) < 3:
            raise create_error_response(
                "INVALID_PROMPT", "Prompt must be at least 3 characters long"
            )
        user_id = get_user_id_from_api_key(api_key)
        handler = APIRequestHandler(dj_system)
        if not dj_system:
            raise create_error_response(
                "GPU_UNAVAILABLE",
                "Audio generation system is currently unavailable. Please try again later.",
                503,
            )
        if not IS_TEST:
            async with llm_lock:
                handler.setup_llm_session(request, request_id, user_id)
                llm_decision = handler.get_llm_decision()
            async with audio_lock:
                audio, _ = handler.generate_simple(request, llm_decision)
                processed_path, used_stems = handler.process_audio_pipeline(
                    audio, request, request_id
                )
        else:
            test_files_path = "./testfiles"
            test_files = []
            for _, _, filenames in walk(test_files_path):
                test_files.extend(filenames)
                break
            test_files.remove(".gitkeep")
            processed_path = dj_system.layer_manager._prepare_sample_for_loop(
                original_audio_path="./testfiles/" + random.choice(test_files),
                layer_id=f"simple_loop_{request_id}",
                sample_rate=int(request.sample_rate),
            )
            used_stems = None
            time.sleep(3)
        if not processed_path or not os.path.exists(processed_path):
            raise create_error_response(
                "SERVER_ERROR", "Audio generation completed but file not found", 500
            )
        audio_data, sr = librosa.load(processed_path, sr=None)
        duration = len(audio_data) / sr
        if duration < 0.1:
            raise create_error_response(
                "SERVER_ERROR", "Generated audio is too short or empty", 500
            )
        with open(processed_path, "rb") as f:
            wav_data = f.read()
        increment_api_key_usage(api_key)
        _, _, key_info = check_api_key_status(api_key)
        remaining_credits = "unlimited"
        if key_info.get("is_limited"):
            remaining_credits = str(
                key_info.get("total_credits", 0) - key_info.get("credits_used", 0)
            )
        print(f"✅ SUCCESS: {duration:.1f}")
        headers = {
            "X-Duration": str(duration),
            "X-BPM": str(request.bpm),
            "X-Key": str(request.key or ""),
            "X-Stems-Used": ",".join(used_stems) if used_stems else "",
            "X-Credits-Remaining": remaining_credits,
        }
        if key_info.get("is_limited") and key_info.get("date_of_expiration"):
            headers["X-Key-Expires"] = key_info["date_of_expiration"]
        return Response(
            content=wav_data,
            media_type="audio/wav",
            headers=headers,
        )

    except HTTPException:
        raise
    except Exception as e:
        print(f"❌ UNEXPECTED ERROR #{request_id}: {str(e)}")
        raise create_error_response(
            "SERVER_ERROR",
            "An unexpected error occurred. Please try again or contact support.",
            500,
        )
    finally:
        if processed_path and os.path.exists(processed_path):
            os.remove(processed_path)
