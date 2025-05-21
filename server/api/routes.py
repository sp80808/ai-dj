from fastapi import APIRouter, HTTPException, Depends, Security, Request
from .models import GenerateRequest, GenerateResponse, InitConfig
from fastapi.security.api_key import APIKeyHeader
import os
import time
import librosa
from core.music_generator import MusicGenerator
from dotenv import load_dotenv
from config.vst_prompts import (
    VST_STYLE_PARAMS,
    create_vst_system_prompt,
)

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
        request_timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
        request_id = int(time.time())
        print(f"\n===== NOUVELLE REQUÊTE {request_id} - {request_timestamp} =====")
        print(f"Style demandé: {request.style}")
        print(f"BPM: {request.bpm}")
        print(f"Clé: {request.key}")
        print(f"Prompt: '{request.prompt}'")
        print(f"[{request_id}] 🔄 Réinitialisation complète du modèle LLM...")
        print("")
        dj_system.dj_brain._init_model()
        print("")
        initial_state = {
            "mode": "vst",
            "current_tempo": request.bpm,
            "current_key": request.key,
            "phase": "generation",
            "energy_level": 7,
            "last_action_time": time.time(),
            "session_duration": 0,
            "active_layers": {},
            "time_elapsed": 0,
            "samples_generated": 0,
            "special_instruction": "",
            "request_id": request_id,
        }

        print(f"[{request_id}] Réinitialisation partielle de l'état de session...")
        if not hasattr(dj_system.dj_brain, "session_state"):
            # Première initialisation
            dj_system.dj_brain.session_state = initial_state.copy()
            dj_system.dj_brain.session_state["history"] = []
            print(f"[{request_id}] Première initialisation de la session")
        else:
            # Conserver l'historique existant
            old_history = dj_system.dj_brain.session_state.get("history", [])
            old_session_duration = dj_system.dj_brain.session_state.get(
                "session_duration", 0
            )

            print(
                f"[{request_id}] Mise à jour de la session. Historique conservé: {len(old_history)} éléments"
            )

            # Mettre à jour l'état de la session mais conserver l'historique
            dj_system.dj_brain.session_state.update(initial_state)

            # Conserver explicitement l'historique et la durée de session
            dj_system.dj_brain.session_state["history"] = old_history
            dj_system.dj_brain.session_state["session_duration"] = old_session_duration

            # Ne conserver que les 10 dernières décisions au maximum
            if len(dj_system.dj_brain.session_state["history"]) > 10:
                dj_system.dj_brain.session_state["history"] = (
                    dj_system.dj_brain.session_state["history"][-10:]
                )
                print(f"[{request_id}] Historique tronqué aux 10 dernières décisions")

        # Afficher un résumé des 3 dernières décisions si disponibles
        history = dj_system.dj_brain.session_state.get("history", [])
        if history:
            print(
                f"[{request_id}] Résumé des dernières décisions ({len(history)} au total):"
            )
            for i, action in enumerate(
                history[-3:]
            ):  # Afficher au maximum les 3 dernières
                # Calcul de l'index correct
                action_index = len(history) - len(history[-3:]) + i

                # Extraction des informations clés
                action_type = action.get("action_type", "unknown")
                reasoning_full = action.get("reasoning", "")
                reasoning = reasoning_full[:50] + (
                    "..." if len(reasoning_full) > 50 else ""
                )

                # Extraction des détails du sample
                sample_details = action.get("parameters", {}).get("sample_details", {})
                sample_type = sample_details.get("type", "unknown")

                # Affichage formaté
                print(f"  [{action_index + 1}] {action_type}: {sample_type}")
                print(f"      Raison: {reasoning}")

                keywords = sample_details.get("musicgen_prompt_keywords", [])
                if keywords:
                    keywords_str = ", ".join(keywords[:5])
                    print(f"      Mots-clés: {keywords_str}")
        else:
            print(f"[{request_id}] Aucun historique de décision disponible")
        layer_manager = dj_system.layer_manager
        use_stems = (
            request.preferred_stems is not None and len(request.preferred_stems) > 0
        )
        system_prompt = create_vst_system_prompt(
            style=request.style, include_stems=use_stems
        )

        if request.prompt != "":
            instruction = f"L'utilisateur demande explicitement: '{request.prompt}'. Tu DOIS générer un sample qui correspond EXACTEMENT à cette demande."
            dj_system.dj_brain.session_state["special_instruction"] = instruction
            print(f"[{request_id}] 🔊 Instruction spéciale définie: '{instruction}'")
        else:
            print(f"[{request_id}] 🔇 Aucune instruction spéciale définie")

        # 1. Préparer l'état pour le LLM
        print(f"[{request_id}] Configuration de l'état pour le LLM...")
        dj_system.dj_brain.session_state.update(
            {
                "mode": "vst",
                "current_tempo": request.bpm,
                "current_key": request.key,
                "style": request.style,
                "user_request": {"text": request.prompt, "timestamp": time.time()},
                "vst_params": VST_STYLE_PARAMS.get(
                    request.style, VST_STYLE_PARAMS["techno_minimal"]
                ),
                "stems_enabled": use_stems,
            }
        )

        special_instruction = dj_system.dj_brain.session_state.get(
            "special_instruction", ""
        )
        print(
            f"[{request_id}] Vérification post-configuration - Instruction spéciale: '{special_instruction}'"
        )

        if dj_system.dj_brain.system_prompt != system_prompt:
            dj_system.dj_brain.system_prompt = system_prompt

        # 2. Obtenir la décision du LLM
        print(f"[{request_id}] Demande de décision au LLM...")
        llm_decision = dj_system.dj_brain.get_next_decision()

        action_type = llm_decision.get("action_type", "")
        params = llm_decision.get("parameters", {})
        reasoning = llm_decision.get("reasoning", "N/A")

        print(f"\n🤖 Action LLM: {action_type}")
        print(f"💭 Raison: {reasoning}")
        print(f"\n⚙️  Paramètres: {params}\n")

        # 3. Extraire les paramètres de génération du LLM
        sample_details = llm_decision.get("parameters", {}).get("sample_details", {})

        # 4. Générer le sample avec MusicGen en utilisant les paramètres du LLM
        audio, _ = dj_system.music_gen.generate_sample(
            sample_type=sample_details.get("type", request.style),
            tempo=request.bpm,
            key=request.key,
            intensity=sample_details.get("intensity", 7),
            musicgen_prompt_keywords=sample_details.get(
                "musicgen_prompt_keywords", request.prompt.split()
            ),
            genre=sample_details.get("genre", "electronic"),
        )

        # 5. Sauvegarder le sample brut temporairement
        temp_path = os.path.join(
            dj_system.output_dir_base, f"temp_raw_{int(time.time())}.wav"
        )
        dj_system.music_gen.save_sample(audio, temp_path)

        # 6. Préparer la loop avec les fonctions existantes
        processed_path = layer_manager._prepare_sample_for_loop(
            original_audio_path=temp_path,
            layer_id=f"vst_loop_{int(time.time())}",
            measures=sample_details.get("measures", request.measures),
        )

        if not processed_path:
            raise HTTPException(
                status_code=500, detail="Échec de la préparation de la loop"
            )

        # 7. Si des stems sont demandés (soit par l'user soit par le LLM)
        preferred_stems = request.preferred_stems or sample_details.get(
            "preferred_stems"
        )
        used_stems = None

        if preferred_stems:
            spectral_profile, separated_path = (
                dj_system.stems_manager._analyze_sample_with_demucs(
                    processed_path, os.path.join(dj_system.output_dir_base, "temp")
                )
            )

            if spectral_profile and separated_path:
                final_path, used_stems = (
                    dj_system.stems_manager._extract_multiple_stems(
                        spectral_profile,
                        separated_path,
                        f"vst_loop_{int(time.time())}",
                        preferred_stems,
                    )
                )
                if final_path:
                    processed_path = final_path

        # 8. Nettoyer les fichiers temporaires
        if os.path.exists(temp_path) and temp_path != processed_path:
            os.remove(temp_path)

        # 9. Obtenir la durée finale
        audio_data, sr = librosa.load(processed_path, sr=None)
        duration = len(audio_data) / sr

        with open(processed_path, "rb") as audio_file:
            audio_data = audio_file.read()

        import base64

        audio_base64 = base64.b64encode(audio_data).decode("utf-8")

        return {
            "audio_data": audio_base64,
            "duration": duration,
            "bpm": request.bpm,
            "key": request.key,
            "stems_used": used_stems,
            "sample_rate": layer_manager.sample_rate,
        }

    except Exception as e:
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
            "config": config.dict(),
            "updates": updates,
            "system_info": {
                "current_model": dj_system.music_gen.model_name,
                "output_dir": dj_system.output_dir_base,
                "sample_rate": dj_system.layer_manager.sample_rate,
            },
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
