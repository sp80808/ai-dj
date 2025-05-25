import os
import time
from fastapi import HTTPException
from core.genre_detector import GenreDetector
from config.vst_prompts import (
    VST_STYLE_PARAMS,
    create_vst_system_prompt,
)
from server.api.models import GenerateRequest

class APIRequestHandler:
    """Gestionnaire propre pour les requêtes API"""

    def __init__(self, dj_system):
        self.dj_system = dj_system

    def setup_llm_session(self, request, request_id):
        """Configure la session LLM avec tout le bordel nécessaire"""
        print(f"[{request_id}] 🔄 Setup session LLM bourrin mode...")

        # Reset du modèle (obligé, sinon ça merde)
        self.dj_system.dj_brain._init_model()

        # État initial de session
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

        # Gestion de l'historique (parce que c'est important pour la cohérence)
        if not hasattr(self.dj_system.dj_brain, "session_state"):
            self.dj_system.dj_brain.session_state = initial_state.copy()
            self.dj_system.dj_brain.session_state["history"] = []
            print(f"[{request_id}] Première session LLM initialisée")
        else:
            # Conserver l'historique mais mettre à jour le reste
            old_history = self.dj_system.dj_brain.session_state.get("history", [])
            old_duration = self.dj_system.dj_brain.session_state.get(
                "session_duration", 0
            )

            self.dj_system.dj_brain.session_state.update(initial_state)
            self.dj_system.dj_brain.session_state["history"] = old_history[
                -10:
            ]  # Garder que les 10 derniers
            self.dj_system.dj_brain.session_state["session_duration"] = old_duration

            print(
                f"[{request_id}] Session mise à jour, historique: {len(old_history)} -> {len(self.dj_system.dj_brain.session_state['history'])}"
            )

        # Instruction spéciale si prompt custom
        if request.prompt.strip():
            instruction = f"L'utilisateur demande explicitement: '{request.prompt}'. Tu DOIS générer un sample qui correspond EXACTEMENT à cette demande."
            self.dj_system.dj_brain.session_state["special_instruction"] = instruction
            print(f"[{request_id}] 🔊 Instruction spéciale: '{instruction}'")

        # Mise à jour avec params de la requête
        self.dj_system.dj_brain.session_state.update(
            {
                "style": request.style,
                "user_request": {"text": request.prompt, "timestamp": time.time()},
                "vst_params": VST_STYLE_PARAMS.get(
                    request.style, VST_STYLE_PARAMS["techno_minimal"]
                ),
                "stems_enabled": bool(request.preferred_stems),
            }
        )

        # Setup du prompt système
        use_stems = bool(request.preferred_stems)
        system_prompt = create_vst_system_prompt(
            style=request.style, include_stems=use_stems
        )
        self.dj_system.dj_brain.system_prompt = system_prompt

    def get_llm_decision(self, request_id):
        """Obtient la décision du LLM et l'affiche proprement"""
        print(f"[{request_id}] 🧠 Consultation du LLM bourrin...")

        llm_decision = self.dj_system.dj_brain.get_next_decision()

        action_type = llm_decision.get("action_type", "unknown")
        params = llm_decision.get("parameters", {})
        reasoning = llm_decision.get("reasoning", "N/A")
        sample_details = params.get("sample_details", {})

        print(f"[{request_id}] 🤖 LLM Action: {action_type}")
        print(f"[{request_id}] 💭 LLM Reasoning: {reasoning}")
        print(
            f"[{request_id}] 🎵 LLM Sample Type: {sample_details.get('type', 'unknown')}"
        )
        print(
            f"[{request_id}] 🔑 LLM Keywords: {sample_details.get('musicgen_prompt_keywords', [])}"
        )

        return llm_decision

    def generate_with_adaptation(self, request, llm_decision, request_id):
        """Version avec sélection intelligente du sample type"""
        sample_details = llm_decision.get("parameters", {}).get("sample_details", {})

        llm_sample_type = sample_details.get("type", "unknown")
        llm_keywords = sample_details.get("musicgen_prompt_keywords", [])

        print(
            f"[{request_id}] 🎯 Génération avec sélection intelligente de sample type:"
        )
        print(f"    User prompt: '{request.prompt}'")
        print(f"    LLM keywords: {llm_keywords}")
        print(f"    LLM type: {llm_sample_type}")
        print(f"    Style VST: {request.style}")

        # 🎯 ADAPTATION avec sélection intelligente du sample type
        adaptation = GenreDetector.detect_and_adapt(
            user_prompt=request.prompt,
            llm_keywords=llm_keywords,
            default_genre=request.style,
            tempo=request.bpm,
            key=request.key,
            intensity=sample_details.get("intensity", 7),
            llm_sample_type=llm_sample_type,
            vst_style_params=VST_STYLE_PARAMS,
        )

        print(f"[{request_id}] ✅ Adaptation finale intelligente:")
        print(f"    Genre: {adaptation['genre']}")
        print(f"    Sample type: {adaptation['sample_type']} ← (choisi intelligemment)")
        print(f"    Prompt MusicGen: '{adaptation['musicgen_prompt']}'")
        print(f"    Mots-clés nettoyés: {adaptation.get('cleaned_keywords', [])}")

        # 🎹 GÉNÉRATION DIRECTE
        audio, sample_info = self.dj_system.music_gen.generate_sample(
            musicgen_prompt=adaptation["musicgen_prompt"],
            tempo=request.bpm,
            sample_type=adaptation["sample_type"],
        )

        return audio, sample_info, adaptation

    def process_audio_pipeline(self, audio, request: GenerateRequest, request_id, adaptation):
        """Pipeline de traitement audio complet"""
        # 1. Sauvegarde temporaire
        temp_path = os.path.join(
            self.dj_system.output_dir_base, f"temp_raw_{request_id}.wav"
        )
        self.dj_system.music_gen.save_sample(audio, temp_path)
        self.dj_system.layer_manager.master_tempo = request.bpm if hasattr(request, "bpm") else 126
        # 2. Préparation loop
        processed_path = self.dj_system.layer_manager._prepare_sample_for_loop(
            original_audio_path=temp_path,
            layer_id=f"vst_loop_{request_id}",
            measures=request.measures or 2,
            model_name=self.dj_system.audio_model,
        )

        if not processed_path:
            raise HTTPException(status_code=500, detail="Échec préparation loop")

        # 3. Extraction stems si demandés
        used_stems = None
        if request.preferred_stems:
            print(f"[{request_id}] 🎚️ Extraction stems: {request.preferred_stems}")

            spectral_profile, separated_path = (
                self.dj_system.stems_manager._analyze_sample_with_demucs(
                    processed_path, os.path.join(self.dj_system.output_dir_base, "temp")
                )
            )

            if spectral_profile and separated_path:
                final_path, used_stems = (
                    self.dj_system.stems_manager._extract_multiple_stems(
                        spectral_profile,
                        separated_path,
                        f"vst_loop_{request_id}",
                        request.preferred_stems,
                    )
                )
                if final_path:
                    processed_path = final_path

        # 4. Nettoyage
        if os.path.exists(temp_path) and temp_path != processed_path:
            os.remove(temp_path)

        return processed_path, used_stems
