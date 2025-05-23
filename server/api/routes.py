from fastapi import APIRouter, HTTPException, Depends, Security, Request
from .models import GenerateRequest, GenerateResponse, InitConfig
from fastapi.security.api_key import APIKeyHeader
import os
import base64
import time
import librosa
from core.music_generator import MusicGenerator
from server.api.api_request_handler import APIRequestHandler
from dotenv import load_dotenv

load_dotenv()

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


# Définir le header pour l'API key
API_KEY_HEADER = APIKeyHeader(name="X-API-Key")

# Récupérer la clé d'API depuis .env
API_KEY = os.getenv("DJ_IA_API_KEY")
if not API_KEY:
    raise ValueError("DJ_IA_API_KEY non définie dans le fichier .env")


async def verify_api_key(api_key: str = Security(API_KEY_HEADER)):
    """Vérifie la validité de la clé d'API"""
    if api_key != API_KEY:
        raise HTTPException(status_code=403, detail="Clé d'API invalide")
    return api_key


@router.get("/")
async def hello_world():
    return {"status": "valid", "message": "Hello world"}


@router.post("/verify_key")
async def verify_key(_: str = Depends(verify_api_key)):
    """Vérifie si une clé d'API est valide"""
    return {"status": "valid", "message": "Clé d'API valide"}


@router.post("/generate", response_model=GenerateResponse)
async def generate_loop(
    request: GenerateRequest,
    _: str = Depends(verify_api_key),
    dj_system=Depends(get_dj_system),
):
    try:
        request_id = int(time.time())
        print(f"\n===== 🎵 REQUÊTE #{request_id} =====")
        print(
            f"📝 '{request.prompt}' | {request.style} | {request.bpm} BPM | {request.key}"
        )

        # Initialiser le gestionnaire
        handler = APIRequestHandler(dj_system)

        # 1. 🧠 SETUP LLM BOURRIN COMPLET
        handler.setup_llm_session(request, request_id)

        # 2. 🤖 DÉCISION LLM
        llm_decision = handler.get_llm_decision(request_id)

        # 3. 🎹 GÉNÉRATION ADAPTÉE (LLM + GenreDetector)
        audio, _, adaptation = handler.generate_with_adaptation(
            request, llm_decision, request_id
        )

        # 4. 🔧 PIPELINE AUDIO COMPLET
        processed_path, used_stems = handler.process_audio_pipeline(
            audio, request, request_id, adaptation
        )

        # 5. 📤 RETOUR FINAL
        audio_data, sr = librosa.load(processed_path, sr=None)
        duration = len(audio_data) / sr

        with open(processed_path, "rb") as f:
            audio_bytes = f.read()

        audio_base64 = base64.b64encode(audio_bytes).decode("utf-8")

        print(
            f"[{request_id}] ✅ SUCCÈS: {duration:.1f}s, genre: {adaptation['genre']}"
        )

        return {
            "audio_data": audio_base64,
            "duration": duration,
            "bpm": request.bpm,
            "key": request.key,
            "stems_used": used_stems,
            "sample_rate": dj_system.music_gen.sample_rate,
            "detected_genre": adaptation["genre"],
            "llm_reasoning": llm_decision.get("reasoning", ""),
        }

    except Exception as e:
        print(f"❌ ERREUR #{request_id}: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/health")
async def health_check(
    _: str = Depends(verify_api_key), dj_system=Depends(get_dj_system)
):
    """Vérifie l'état du système et des modèles"""
    try:
        return {
            "status": "healthy",
            "models": {
                "musicgen": {"name": dj_system.music_gen.model_name, "status": "ok"},
                "llm": {
                    "path": dj_system.dj_brain.model.model_path,
                    "status": "ok",
                },
            },
            "system": {
                "output_dir": dj_system.output_dir_base,
                "sample_rate": dj_system.layer_manager.sample_rate,
            },
            "timestamp": time.time(),
        }
    except Exception as e:
        return {"status": "unhealthy", "error": str(e), "timestamp": time.time()}


@router.post("/initialize")
async def initialize(
    config: InitConfig,
    _: str = Depends(verify_api_key),
    dj_system=Depends(get_dj_system),
):
    """Initialise ou reconfigure le système"""
    try:

        # Mettre à jour la configuration
        updates = {
            "model_updated": False,
            "output_dir_updated": False,
            "api_key_updated": False,
        }

        # 1. Mettre à jour le modèle audio si nécessaire
        if config.model_name and config.model_name != dj_system.music_gen.model_name:
            try:
                dj_system.music_gen = MusicGenerator(
                    model_name=config.model_name,
                    default_duration=dj_system.music_gen.default_duration,
                )
                updates["model_updated"] = True
            except Exception as e:
                raise HTTPException(
                    status_code=400,
                    detail=f"Erreur lors du chargement du modèle {config.model_name}: {str(e)}",
                )

        # 2. Mettre à jour le répertoire de sortie
        if config.output_dir and config.output_dir != dj_system.output_dir_base:
            try:
                os.makedirs(config.output_dir, exist_ok=True)
                dj_system.output_dir_base = config.output_dir
                dj_system.layer_manager.output_dir = os.path.join(
                    config.output_dir, "layers"
                )
                updates["output_dir_updated"] = True
            except Exception as e:
                raise HTTPException(
                    status_code=400,
                    detail=f"Erreur lors de la création du répertoire {config.output_dir}: {str(e)}",
                )

        # 3. Mettre à jour l'API key si nécessaire
        if config.api_key:
            # Ici on pourrait implémenter la validation de l'API key
            updates["api_key_updated"] = True

        return {
            "status": "initialized",
            "config": config.model_dump(),
            "updates": updates,
            "system_info": {
                "current_model": dj_system.music_gen.model_name,
                "output_dir": dj_system.output_dir_base,
                "sample_rate": dj_system.layer_manager.sample_rate,
            },
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
