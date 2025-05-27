import os
import time
from fastapi import HTTPException
from core.dj_system import DJSystem
from server.api.models import GenerateRequest


class APIRequestHandler:
    """Gestionnaire simplifié pour les requêtes API"""

    def __init__(self, dj_system):
        self.dj_system: DJSystem = dj_system

    def setup_llm_session(self, request: GenerateRequest, request_id):
        """Setup minimal du LLM"""
        print(f"[{request_id}] 🔄 Setup LLM minimal...")

        # Réinitialiser le modèle
        self.dj_system.dj_brain.init_model()

        # État minimal
        self.dj_system.dj_brain.session_state = {
            "current_tempo": request.bpm,
            "current_key": request.key or "C minor",
            "user_prompt": request.prompt,
            "request_id": request_id,
            "last_action_time": time.time(),
        }

    def get_llm_decision(self, request_id):
        """Obtient la décision du LLM"""
        print(f"[{request_id}] 🧠 Consultation LLM...")

        llm_decision = self.dj_system.dj_brain.get_next_decision()

        reasoning = llm_decision.get("reasoning", "N/A")
        sample_details = llm_decision.get("parameters", {}).get("sample_details", {})
        musicgen_prompt = sample_details.get("musicgen_prompt", "")

        print(f"[{request_id}] 💭 LLM Reasoning: {reasoning}")
        print(f"[{request_id}] 🎵 MusicGen Prompt: '{musicgen_prompt}'")

        self.dj_system.dj_brain.destroy_model()

        return llm_decision

    def generate_simple(self, request: GenerateRequest, llm_decision: dict, request_id):
        """Génération directe sans adaptation complexe"""
        sample_details = llm_decision.get("parameters", {}).get("sample_details", {})

        # Récupérer le prompt MusicGen du LLM ou fallback sur le prompt user
        musicgen_prompt = sample_details.get("musicgen_prompt", request.prompt)
        key = sample_details.get("key", request.key or "C minor")

        print(f"[{request_id}] 🎹 Génération directe:")
        print(f"    Prompt final: '{musicgen_prompt}'")
        print(f"    Tonalité: {key}")

        # Génération directe
        audio, sample_info = self.dj_system.music_gen.generate_sample(
            musicgen_prompt=musicgen_prompt,
            tempo=request.bpm,
            generation_duration=request.generation_duration or 6,
        )

        return audio, sample_info

    def process_audio_pipeline(self, audio, request: GenerateRequest, request_id):
        """Pipeline de traitement audio simplifié"""
        # 1. Sauvegarde temporaire
        temp_path = os.path.join(
            self.dj_system.output_dir_base, f"temp_raw_{request_id}.wav"
        )
        self.dj_system.music_gen.save_sample(audio, temp_path)

        # 2. Préparation loop
        self.dj_system.layer_manager.master_tempo = request.bpm
        processed_path = self.dj_system.layer_manager._prepare_sample_for_loop(
            original_audio_path=temp_path,
            layer_id=f"simple_loop_{request_id}",
            measures=request.measures or 4,
        )

        if not processed_path:
            raise HTTPException(status_code=500, detail="Échec préparation loop")

        # 3. Extraction stems (optionnel)
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
                        f"simple_loop_{request_id}",
                        request.preferred_stems,
                    )
                )
                if final_path:
                    processed_path = final_path

        # 4. Nettoyage
        if os.path.exists(temp_path) and temp_path != processed_path:
            os.remove(temp_path)

        return processed_path, used_stems
