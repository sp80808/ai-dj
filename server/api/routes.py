import base64
import time
import librosa
from fastapi import APIRouter, HTTPException, Depends, Security, Request
from .models import GenerateRequest, GenerateResponse
from config.config import API_KEY, API_KEY_HEADER, ENVIRONMENT
from server.api.api_request_handler import APIRequestHandler


router = APIRouter()


def get_dj_system(request: Request):
    """Récupère l'instance DJ System à partir de la requête"""
    # Vérifier d'abord app.dj_system (méthode principale)
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system

    # Vérifier ensuite app.state.dj_system (méthode alternative)
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system

    # Si aucune instance n'est trouvée, c'est une erreur grave
    raise RuntimeError("Aucune instance DJSystem trouvée dans l'application FastAPI!")


async def verify_api_key(api_key: str = Security(API_KEY_HEADER)):
    """Vérifie la validité de la clé d'API"""
    if ENVIRONMENT == "dev":
        return "dev-bypass"
    if api_key != API_KEY:
        raise HTTPException(status_code=403, detail="Clé d'API invalide")
    return api_key


@router.post("/verify_key")
async def verify_key(_: str = Depends(verify_api_key)):
    """Vérifie si une clé d'API est valide"""
    return {"status": "valid", "message": "Ok"}


@router.post("/generate", response_model=GenerateResponse)
async def generate_loop(
    request: GenerateRequest,
    _: str = Depends(verify_api_key),
    dj_system=Depends(get_dj_system),
):
    try:
        request_id = int(time.time())
        print(f"\n===== 🎵 REQUÊTE #{request_id} =====")
        print(f"📝 '{request.prompt}' | {request.bpm} BPM | {request.key}")

        # Initialiser le gestionnaire
        handler = APIRequestHandler(dj_system)

        # 1. 🧠 SETUP LLM
        handler.setup_llm_session(request, request_id)

        # 2. 🤖 DÉCISION LLM
        llm_decision = handler.get_llm_decision(request_id)

        # 3. 🎹 GÉNÉRATION ADAPTÉE (LLM + GenreDetector)
        audio, _ = handler.generate_simple(request, llm_decision, request_id)

        # 4. 🔧 PIPELINE AUDIO COMPLET
        processed_path, used_stems = handler.process_audio_pipeline(
            audio, request, request_id
        )

        # 5. 📤 RETOUR FINAL
        audio_data, sr = librosa.load(processed_path, sr=None)
        duration = len(audio_data) / sr

        with open(processed_path, "rb") as f:
            audio_bytes = f.read()

        audio_base64 = base64.b64encode(audio_bytes).decode("utf-8")

        print(f"[{request_id}] ✅ SUCCÈS: {duration:.1f}")

        return {
            "audio_data": audio_base64,
            "duration": duration,
            "bpm": request.bpm,
            "key": request.key,
            "stems_used": used_stems,
            "sample_rate": 48000,
            "llm_reasoning": llm_decision.get("reasoning", ""),
        }

    except Exception as e:
        print(f"❌ ERREUR #{request_id}: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))
