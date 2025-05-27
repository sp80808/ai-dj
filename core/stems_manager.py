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

    # Tous les modèles Demucs disponibles avec leurs stems
    DEMUCS_MODELS = {
        "htdemucs": ["drums", "bass", "other", "vocals"],  # 4 stems standard
        "htdemucs_ft": ["drums", "bass", "other", "vocals"],  # 4 stems fine-tuned
        "htdemucs_6s": [
            "drums",
            "bass",
            "other",
            "vocals",
            "guitar",
            "piano",
        ],  # 6 stems !
        "hdemucs_mmi": ["drums", "bass", "other", "vocals"],  # 4 stems
        "mdx": ["drums", "bass", "other", "vocals"],  # 4 stems
        "mdx_extra": ["drums", "bass", "other", "vocals"],  # 4 stems
        "mdx_q": ["drums", "bass", "other", "vocals"],  # 4 stems qualité
        "mdx_extra_q": ["drums", "bass", "other", "vocals"],  # 4 stems qualité extra
    }

    def __init__(self, preferred_model="htdemucs_6s"):
        """
        Args:
            preferred_model: Modèle Demucs préféré (htdemucs_6s pour 6 stems)
        """
        self.preferred_model = preferred_model
        self.available_stems = self.DEMUCS_MODELS.get(
            preferred_model, ["drums", "bass", "other", "vocals"]
        )
        print(f"🎛️ StemsManager initialisé avec modèle {preferred_model}")
        print(f"   Stems disponibles: {', '.join(self.available_stems)}")

    def create_default_profile(self, sample_type):
        """Crée un profil spectral par défaut avec TOUS les stems possibles"""
        # Profil par défaut pour tous les stems
        profile = {
            "drums": 0.05,
            "bass": 0.05,
            "other": 0.6,
            "vocals": 0.05,
            "guitar": 0.05,  # ← Nouveau
            "piano": 0.05,  # ← Nouveau
        }

        # Ajuster en fonction du type de sample
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
            profile["guitar"] = 0.7  # ← Utilise le stem guitar dédié
            profile["other"] = 0.1
        elif "piano" in sample_type or "keys" in sample_type or "chord" in sample_type:
            profile["piano"] = 0.7  # ← Utilise le stem piano dédié
            profile["other"] = 0.2
        elif "pad" in sample_type or "ambient" in sample_type:
            profile["other"] = 0.8
            profile["piano"] = 0.1
        elif "fx" in sample_type or "effect" in sample_type:
            profile["other"] = 0.9

        return profile

    def _extract_multiple_stems(
        self, spectral_profile, separated_path, layer_id, preferred_stems=None
    ):
        """Extrait plusieurs stems avec support pour guitar et piano"""
        if not spectral_profile or not separated_path:
            print(
                "❌ Impossible d'extraire des stems: profil spectral ou chemin non disponible"
            )
            return None, None

        available_stems = list(spectral_profile.keys())
        print(f"🔍 Stems disponibles: {', '.join(available_stems)}")

        # Déterminer les stems à extraire
        selected_stems = []
        selection_method = "auto"

        if preferred_stems == "all":
            selected_stems = available_stems
            selection_method = "tous les stems"
        elif isinstance(preferred_stems, list) and preferred_stems:
            selected_stems = [
                stem for stem in preferred_stems if stem in spectral_profile
            ]
            if not selected_stems:
                dominant_stem = max(spectral_profile, key=spectral_profile.get)
                selected_stems = [dominant_stem]
                selection_method = "dominant (fallback)"
            else:
                selection_method = "préférences utilisateur"
        else:
            # Fallback au stem dominant
            dominant_stem = max(spectral_profile, key=spectral_profile.get)
            selected_stems = [dominant_stem]
            selection_method = "dominant (fallback)"

        print(f"🎯 Sélection {selection_method}: stems {selected_stems}")

        # Vérifier que les stems existent
        valid_stems = []
        for stem in selected_stems:
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                valid_stems.append(stem)
            else:
                print(f"❌ Stem {stem} introuvable à {stem_path}")

        if not valid_stems:
            print("❌ Aucun stem trouvé")
            return None, None

        # Mixer les stems
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

                # Traitement spécifique par stem
                if stem == "drums":
                    # Compression drums
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
                    # Compression bass
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
                    # Traitement spécial pour guitar
                    target_level = 0.7
                    print(f"🎸 Traitement spécial pour stem 'guitar'")
                elif stem == "piano":
                    # Traitement spécial pour piano
                    target_level = 0.65
                    print(f"🎹 Traitement spécial pour stem 'piano'")
                elif stem == "vocals":
                    target_level = 0.6
                else:  # other
                    target_level = 0.6

                # Normalisation
                current_peak = np.max(np.abs(audio))
                if current_peak > 0:
                    target_gain = target_level / current_peak
                    target_gain = min(target_gain, 5.0)
                    audio = audio * target_gain
                    gain_db = 20 * np.log10(target_gain) if target_gain > 0 else 0
                    print(f"🔊 Normalisation stem '{stem}' (gain: {gain_db:.1f} dB)")

                # Mixer
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

            # Normalisation finale
            if mixed_audio is not None:
                final_peak = np.max(np.abs(mixed_audio))
                if final_peak > 0.95:
                    mixed_audio = mixed_audio * (0.95 / final_peak)

                sf.write(mixed_output_path, mixed_audio, sr)
                print(
                    f"✅ Stems mixés sauvegardés: {os.path.basename(mixed_output_path)}"
                )
                return mixed_output_path, valid_stems
            else:
                return None, None

        except Exception as e:
            print(f"⚠️ Erreur mixage stems: {e}")
            return None, None

    def _analyze_sample_with_demucs(self, sample_path, temp_output_dir):
        """Analyse avec le modèle Demucs configuré (6 stems par défaut)"""
        os.makedirs(temp_output_dir, exist_ok=True)

        # Commande Demucs avec le modèle choisi
        cmd = [
            sys.executable,
            "-m",
            "demucs",
            "--out",
            temp_output_dir,
            "-n",
            self.preferred_model,  # ← Utilise le modèle configuré
            sample_path,
        ]

        print(f"🔊 Analyse avec modèle {self.preferred_model}: {' '.join(cmd)}")
        process = subprocess.run(cmd, capture_output=True, text=True)

        if process.returncode != 0:
            print(f"❌ Erreur Demucs: {process.stderr}")
            return self.create_default_profile(os.path.basename(sample_path)), None

        # Analyser les résultats
        sample_name = Path(sample_path).stem
        separated_path = Path(temp_output_dir) / self.preferred_model / sample_name

        if not separated_path.exists():
            print(f"❌ Répertoire non trouvé: {separated_path}")
            return self.create_default_profile(os.path.basename(sample_path)), None

        # Analyser TOUS les stems disponibles
        available_stems = [f.stem for f in separated_path.glob("*.wav")]
        print(f"🔍 Stems trouvés: {', '.join(available_stems)}")

        # Calculer l'énergie pour tous les stems
        stem_energy = {}
        total_energy = 0

        for stem in self.available_stems:  # ← Utilise les stems du modèle
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                try:
                    audio, sr = librosa.load(str(stem_path), sr=None, mono=True)
                    energy = np.sum(audio**2)
                    stem_energy[stem] = energy
                    total_energy += energy
                    print(f"📊 {stem}: {energy:.2e}")
                except Exception as e:
                    print(f"⚠️ Erreur analyse {stem}: {e}")
                    stem_energy[stem] = 0.01
            else:
                print(f"⚠️ Stem {stem} non trouvé")
                stem_energy[stem] = 0.01

        # Normaliser
        if total_energy > 0:
            for stem in stem_energy:
                stem_energy[stem] /= total_energy

        # Afficher le résumé
        if stem_energy:
            dominant_stem = max(stem_energy, key=stem_energy.get)
            print(
                f"🎯 Stem dominant: {dominant_stem} ({stem_energy[dominant_stem]:.2%})"
            )

            # Afficher tous les pourcentages
            for stem, percentage in sorted(
                stem_energy.items(), key=lambda x: x[1], reverse=True
            ):
                print(f"   {stem}: {percentage:.1%}")

        return stem_energy, separated_path
