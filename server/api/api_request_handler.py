import os
import time
from fastapi import HTTPException
from core.dj_system import DJSystem
from server.api.models import GenerateRequest


class APIRequestHandler:
    def __init__(self, dj_system):
        self.dj_system: DJSystem = dj_system

    def setup_llm_session(self, request: GenerateRequest, request_id, user_id):
        print(f"🔄 Minimal LLM Setup...")

        self.dj_system.dj_brain.init_model()

        self.dj_system.dj_brain.session_state = {
            "current_tempo": request.bpm,
            "current_key": request.key or "C minor",
            "user_prompt": request.prompt,
            "request_id": request_id,
            "last_action_time": time.time(),
            "user_id": user_id,
        }

    def get_llm_decision(self):

        print(f"🧠 LLM Consultation...")

        llm_decision = self.dj_system.dj_brain.get_next_decision()

        reasoning = llm_decision.get("reasoning", "N/A")
        sample_details = llm_decision.get("parameters", {}).get("sample_details", {})
        musicgen_prompt = sample_details.get("musicgen_prompt", "")

        print(f"💭 LLM Reasoning: {reasoning}")
        print(f"🎵 MusicGen Prompt: '{musicgen_prompt}'")

        self.dj_system.dj_brain.destroy_model()

        return llm_decision

    def generate_simple(self, request: GenerateRequest, llm_decision: dict):
        if isinstance(llm_decision, str):
            musicgen_prompt = llm_decision
        else:
            sample_details = llm_decision.get("parameters", {}).get(
                "sample_details", {}
            )
            musicgen_prompt = sample_details.get("musicgen_prompt", request.prompt)

        model_choice = getattr(request, "model", "stable_audio") or "stable_audio"

        if model_choice == "musicgen":
            # Lazy import to avoid hard dependency if not used
            try:
                from core.musicgen_wrapper import MusicGenWrapper  # type: ignore
            except Exception as e:
                raise HTTPException(status_code=500, detail=f"MusicGen not available: {e}")

            device = "cuda" if os.environ.get("FORCE_CPU", "0") != "1" else "cpu"
            try:
                wrapper = MusicGenWrapper(model_name=os.environ.get("MUSICGEN_MODEL", "facebook/musicgen-small"), device=device)
                duration = float(request.generation_duration or 6.0)
                wrapper.set_params(duration=duration, temperature=1.0, top_k=250, cfg_coef=3.0)
                output_path = os.path.join(self.dj_system.output_dir_base, f"mg_{int(time.time())}.wav")
                path = wrapper.generate_audio(
                    prompt=musicgen_prompt,
                    output_path=output_path,
                    conditioning_audio_path=request.conditioning_audio_path,
                    conditioning_midi_base64=request.conditioning_midi_base64,
                    conditioning_strength=float(getattr(request, "conditioning_strength", 0.5) or 0.5),
                    seed=request.seed,
                )
                # Load wav back into numpy for the existing pipeline expectations
                import soundfile as sf  # type: ignore
                audio, sr = sf.read(path)
                sample_info = {"type": "musicgen", "tempo": request.bpm, "prompt": musicgen_prompt}
                return audio, sample_info
            except HTTPException:
                raise
            except Exception as e:
                raise HTTPException(status_code=500, detail=f"MusicGen error: {e}")
        else:
            # Stable Audio path (existing behavior)
            self.dj_system.music_gen.init_model()
            audio, sample_info = self.dj_system.music_gen.generate_sample(
                musicgen_prompt=musicgen_prompt,
                tempo=request.bpm,
                generation_duration=request.generation_duration or 6,
                sample_rate=int(request.sample_rate),
            )
            self.dj_system.music_gen.destroy_model()
            return audio, sample_info

    def process_audio_pipeline(self, audio, request: GenerateRequest, request_id):
        temp_path = os.path.join(
            self.dj_system.output_dir_base, f"temp_raw_{request_id}.wav"
        )
        self.dj_system.music_gen.save_sample(
            audio, temp_path, sample_rate=int(request.sample_rate)
        )

        self.dj_system.layer_manager.master_tempo = request.bpm
        processed_path = self.dj_system.layer_manager._prepare_sample_for_loop(
            original_audio_path=temp_path,
            layer_id=f"simple_loop_{request_id}",
            sample_rate=int(request.sample_rate),
        )

        if not processed_path:
            raise HTTPException(status_code=500, detail="Loop preparation failure")

        used_stems = None
        if request.preferred_stems:
            print(f"🎚️  Extraction stems: {', '.join(request.preferred_stems)}")

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
                        sample_rate=int(request.sample_rate),
                    )
                )
                if final_path:
                    abs_processed_path = os.path.abspath(processed_path)
                    if os.path.exists(abs_processed_path):
                        os.remove(abs_processed_path)
                        print(
                            f"🗑️  Removed original processed file: {abs_processed_path}"
                        )
                    processed_path = final_path

        if os.path.exists(temp_path) and temp_path != processed_path:
            os.remove(temp_path)

        return processed_path, used_stems
