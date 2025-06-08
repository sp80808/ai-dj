import os
import sys
import shutil
import subprocess
from pathlib import Path
import time
import librosa
import numpy as np
import soundfile as sf


class StemsManager:
    DEMUCS_MODELS = {
        "htdemucs": ["drums", "bass", "other", "vocals"],
        "htdemucs_ft": ["drums", "bass", "other", "vocals"],
        "htdemucs_6s": [
            "drums",
            "bass",
            "other",
            "vocals",
            "guitar",
            "piano",
        ],
        "hdemucs_mmi": ["drums", "bass", "other", "vocals"],
        "mdx": ["drums", "bass", "other", "vocals"],
        "mdx_extra": ["drums", "bass", "other", "vocals"],
        "mdx_q": ["drums", "bass", "other", "vocals"],
        "mdx_extra_q": ["drums", "bass", "other", "vocals"],
    }

    def __init__(self, preferred_model="htdemucs_6s"):
        self.preferred_model = preferred_model
        self.available_stems = self.DEMUCS_MODELS.get(
            preferred_model, ["drums", "bass", "other", "vocals"]
        )
        print(f"👌 StemsManager initialized with model {preferred_model}")
        print(f"ℹ️  Available stems: {', '.join(self.available_stems)}")

    def create_default_profile(self, sample_type):
        profile = {
            "drums": 0.05,
            "bass": 0.05,
            "other": 0.6,
            "vocals": 0.05,
            "guitar": 0.05,
            "piano": 0.05,
        }
        if (
            "kick" in sample_type
            or "drum" in sample_type
            or "percussion" in sample_type
        ):
            profile["drums"] = 0.7
            profile["other"] = 0.2
        elif "bass" in sample_type:
            profile["bass"] = 0.7
            profile["other"] = 0.2
        elif "vocal" in sample_type or "voice" in sample_type:
            profile["vocals"] = 0.7
            profile["other"] = 0.2
        elif "guitar" in sample_type:
            profile["guitar"] = 0.7
            profile["other"] = 0.1
        elif "piano" in sample_type or "keys" in sample_type or "chord" in sample_type:
            profile["piano"] = 0.7
            profile["other"] = 0.2
        elif "pad" in sample_type or "ambient" in sample_type:
            profile["other"] = 0.8
            profile["piano"] = 0.1
        elif "fx" in sample_type or "effect" in sample_type:
            profile["other"] = 0.9

        return profile

    def _cleanup_separated_files(self, separated_path):
        if separated_path and os.path.exists(separated_path):

            try:
                shutil.rmtree(separated_path, ignore_errors=True)
                print(f"🗑️  Cleaned stems directory: {separated_path}")
                parent_dir = separated_path.parent
                if parent_dir.exists() and not any(parent_dir.iterdir()):
                    parent_dir.rmdir()
                    print(f"🗑️  Cleaned empty parent: {parent_dir}")

            except Exception as e:
                print(f"⚠️ Cleanup warning: {e}")

    def _extract_multiple_stems(
        self, spectral_profile, separated_path, layer_id, preferred_stems=None
    ):
        if not spectral_profile or not separated_path:
            print("❌ Unable to extract stems: spectral profile or path unavailable")
            return None, None

        available_stems = list(spectral_profile.keys())
        print(f"🔍 Available stems: {', '.join(available_stems)}")

        selected_stems = []
        selection_method = "auto"

        if preferred_stems == "all":
            selected_stems = available_stems
            selection_method = "all stems"
        elif isinstance(preferred_stems, list) and preferred_stems:
            selected_stems = [
                stem for stem in preferred_stems if stem in spectral_profile
            ]
            if not selected_stems:
                dominant_stem = max(spectral_profile, key=spectral_profile.get)
                selected_stems = [dominant_stem]
                selection_method = "dominant (fallback)"
            else:
                selection_method = "user preferences"
        else:
            dominant_stem = max(spectral_profile, key=spectral_profile.get)
            selected_stems = [dominant_stem]
            selection_method = "dominant (fallback)"

        print(f"🎯 Selection {selection_method}: stems {selected_stems}")

        valid_stems = []
        for stem in selected_stems:
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                valid_stems.append(stem)
            else:
                print(f"❌ Stem {stem} not found at {stem_path}")

        if not valid_stems:
            print("❌ No stem found")
            return None, None

        output_dir = os.path.dirname(os.path.dirname(str(separated_path)))
        stems_str = "_".join(valid_stems)
        mixed_output_path = os.path.join(
            output_dir, f"{layer_id}_{stems_str}_mixed_{int(time.time())}.wav"
        )

        try:
            mixed_audio = None
            sr = None

            for stem in valid_stems:
                stem_path = separated_path / f"{stem}.wav"
                audio, sr_curr = librosa.load(str(stem_path), sr=None)

                if sr_curr != 48000:
                    audio = librosa.resample(audio, orig_sr=sr_curr, target_sr=48000)
                    sr_curr = 48000

                if stem == "drums":
                    threshold = 0.4
                    ratio = 2.0
                    amplitude = np.abs(audio)
                    attenuation = np.ones_like(amplitude)
                    mask = amplitude > threshold
                    attenuation[mask] = (
                        threshold + (amplitude[mask] - threshold) / ratio
                    ) / amplitude[mask]
                    audio = audio * attenuation
                    target_level = 0.8
                elif stem == "bass":
                    threshold = 0.3
                    ratio = 2.5
                    amplitude = np.abs(audio)
                    attenuation = np.ones_like(amplitude)
                    mask = amplitude > threshold
                    attenuation[mask] = (
                        threshold + (amplitude[mask] - threshold) / ratio
                    ) / amplitude[mask]
                    audio = audio * attenuation
                    target_level = 0.75
                elif stem == "guitar":
                    target_level = 0.7
                    print(f"🎸 Special treatment for stem 'guitar'")
                elif stem == "piano":
                    target_level = 0.65
                    print(f"🎹 Special treatment for stem 'piano'")
                elif stem == "vocals":
                    target_level = 0.6
                else:
                    target_level = 0.6
                current_peak = np.max(np.abs(audio))
                if current_peak > 0:
                    target_gain = target_level / current_peak
                    target_gain = min(target_gain, 5.0)
                    audio = audio * target_gain
                    gain_db = 20 * np.log10(target_gain) if target_gain > 0 else 0
                    print(f"🔊 Stem normalization '{stem}' (gain: {gain_db:.1f} dB)")

                if mixed_audio is None:
                    mixed_audio = audio
                    sr = sr_curr
                else:
                    if len(audio) > len(mixed_audio):
                        audio = audio[: len(mixed_audio)]
                    elif len(audio) < len(mixed_audio):
                        audio = np.pad(audio, (0, len(mixed_audio) - len(audio)))

                    mixing_weight = 1.0 / len(valid_stems)
                    mixed_audio = (
                        mixed_audio * (1.0 - mixing_weight) + audio * mixing_weight
                    )

            if mixed_audio is not None:
                final_peak = np.max(np.abs(mixed_audio))
                if final_peak > 0.95:
                    mixed_audio = mixed_audio * (0.95 / final_peak)

                sf.write(mixed_output_path, mixed_audio, sr)
                self._cleanup_separated_files(separated_path)
                print(f"✅ Mixed stems saved: {os.path.basename(mixed_output_path)}")
                return mixed_output_path, valid_stems
            else:
                self._cleanup_separated_files(separated_path)
                return None, None

        except Exception as e:
            self._cleanup_separated_files(separated_path)
            print(f"⚠️ Error on stems mixing: {e}")
            return None, None

    def _analyze_sample_with_demucs(self, sample_path, temp_output_dir):
        os.makedirs(temp_output_dir, exist_ok=True)
        cmd = [
            sys.executable,
            "-m",
            "demucs",
            "--out",
            temp_output_dir,
            "-n",
            self.preferred_model,
            sample_path,
        ]

        print(f"🔊 Analysis with model {self.preferred_model}: {' '.join(cmd)}")
        process = subprocess.run(cmd, capture_output=True, text=True)

        if process.returncode != 0:
            print(f"❌ Demucs Error: {process.stderr}")
            return self.create_default_profile(os.path.basename(sample_path)), None

        sample_name = Path(sample_path).stem
        separated_path = Path(temp_output_dir) / self.preferred_model / sample_name

        if not separated_path.exists():
            print(f"❌ Directory not found: {separated_path}")
            return self.create_default_profile(os.path.basename(sample_path)), None

        available_stems = [f.stem for f in separated_path.glob("*.wav")]
        print(f"🔍 Stems found: {', '.join(available_stems)}")

        stem_energy = {}
        total_energy = 0

        for stem in self.available_stems:
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                try:
                    audio, _ = librosa.load(str(stem_path), sr=None, mono=True)
                    energy = np.sum(audio**2)
                    stem_energy[stem] = energy
                    total_energy += energy
                    print(f"📊 {stem}: {energy:.2e}")
                except Exception as e:
                    print(f"⚠️ Error parsing {stem}: {e}")
                    stem_energy[stem] = 0.01
            else:
                print(f"⚠️ Stem {stem} not found")
                stem_energy[stem] = 0.01

        if total_energy > 0:
            for stem in stem_energy:
                stem_energy[stem] /= total_energy

        if stem_energy:
            dominant_stem = max(stem_energy, key=stem_energy.get)
            print(
                f"🎯 Stem dominant: {dominant_stem} ({stem_energy[dominant_stem]:.2%})"
            )

            for stem, percentage in sorted(
                stem_energy.items(), key=lambda x: x[1], reverse=True
            ):
                print(f"   {stem}: {percentage:.1%}")

        return stem_energy, separated_path
