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
    def create_default_profile(self, sample_type):
        """Crée un profil spectral par défaut basé sur le type de sample"""
        profile = {
            "drums": 0.05,
            "bass": 0.05,
            "other": 0.6,
            "vocals": 0.05,
            "guitar": 0.05,
            "piano": 0.05,
        }

        # Ajuster en fonction du type
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
        elif (
            "pad" in sample_type
            or "ambient" in sample_type
            or "atmosphere" in sample_type
        ):
            profile["other"] = 0.8
            profile["piano"] = 0.1
        elif "fx" in sample_type or "effect" in sample_type:
            profile["other"] = 0.9

        return profile

    def _extract_multiple_stems(
        self, spectral_profile, separated_path, layer_id, preferred_stems=None
    ):
        """
        Extrait plusieurs stems préférés par le LLM et les mixe ensemble

        Args:
            spectral_profile: Profil spectral du sample
            separated_path: Chemin vers les stems séparés
            layer_id: ID du layer
            preferred_stems: Liste des stems préférés ["drums", "bass", etc.] ou "all" pour tous

        Returns:
            tuple: (chemin_vers_fichier_mixé, liste_des_stems_utilisés)
        """
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

        # Cas où on demande tous les stems
        if preferred_stems == "all":
            selected_stems = available_stems
            selection_method = "tous les stems"

        # Cas où on demande des stems spécifiques
        elif isinstance(preferred_stems, list) and preferred_stems:
            # Filtrer uniquement les stems qui existent
            selected_stems = [
                stem for stem in preferred_stems if stem in spectral_profile
            ]

            # Si aucun des stems demandés n'existe, fallback sur le stem dominant
            if not selected_stems:
                dominant_stem = max(spectral_profile, key=spectral_profile.get)
                selected_stems = [dominant_stem]
                selection_method = "dominant (fallback, aucun stem demandé disponible)"
            else:
                selection_method = "préférences LLM"

        # Cas où on demande un stem aléatoire
        elif preferred_stems == "random":
            # Sélection aléatoire pondérée de 1 à 3 stems
            import random

            stems = list(spectral_profile.keys())
            weights = list(spectral_profile.values())

            # Déterminer combien de stems sélectionner (1-3)
            num_stems = min(random.randint(1, 3), len(stems))

            # Sélectionner les stems sans remplacement
            selected_stems = []
            for _ in range(num_stems):
                if not stems:
                    break
                selected = random.choices(stems, weights=weights, k=1)[0]
                selected_stems.append(selected)
                # Retirer le stem sélectionné
                idx = stems.index(selected)
                stems.pop(idx)
                weights.pop(idx)

            selection_method = "aléatoire"

        # Cas par défaut ou stem unique spécifié
        else:
            # Si un seul stem est demandé (chaîne de caractères)
            if isinstance(preferred_stems, str) and preferred_stems in spectral_profile:
                selected_stems = [preferred_stems]
                selection_method = "préférence LLM (stem unique)"
            else:
                # Fallback au stem dominant
                dominant_stem = max(spectral_profile, key=spectral_profile.get)
                selected_stems = [dominant_stem]
                selection_method = "dominant (fallback)"

        # S'assurer qu'il y a au moins un stem sélectionné
        if not selected_stems:
            dominant_stem = max(spectral_profile, key=spectral_profile.get)
            selected_stems = [dominant_stem]
            selection_method = "dominant (fallback, sélection vide)"

        print(f"🎯 Sélection {selection_method}: stems {selected_stems}")

        # Vérifier que les stems sélectionnés existent
        valid_stems = []
        for stem in selected_stems:
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                valid_stems.append(stem)
            else:
                print(f"❌ Stem sélectionné {stem} introuvable à {stem_path}")

        if not valid_stems:
            print("❌ Aucun des stems sélectionnés n'a été trouvé")
            return None, None

        # Créer un nouveau fichier pour le mixage des stems
        output_dir = os.path.dirname(os.path.dirname(str(separated_path)))
        stems_str = "_".join(valid_stems)
        mixed_output_path = os.path.join(
            output_dir, f"{layer_id}_{stems_str}_mixed_{int(time.time())}.wav"
        )

        try:
            print("")
            # Mixer les stems sélectionnés
            mixed_audio = None
            sr = None

            for stem in valid_stems:
                stem_path = separated_path / f"{stem}.wav"

                # Charger l'audio
                audio, sr_curr = librosa.load(str(stem_path), sr=None)

                # Traitement spécifique selon le type de stem (comme avant)
                if stem == "drums":
                    # Compression pour les drums
                    threshold = 0.4
                    ratio = 2.0
                    amplitude = np.abs(audio)
                    attenuation = np.ones_like(amplitude)
                    mask = amplitude > threshold
                    attenuation[mask] = (
                        threshold + (amplitude[mask] - threshold) / ratio
                    ) / amplitude[mask]
                    audio = audio * attenuation
                    print(
                        f"🥁 Compression appliquée au stem 'drums' pour plus d'impact"
                    )

                elif stem == "bass":
                    # Compression pour la basse
                    threshold = 0.3
                    ratio = 2.5
                    amplitude = np.abs(audio)
                    attenuation = np.ones_like(amplitude)
                    mask = amplitude > threshold
                    attenuation[mask] = (
                        threshold + (amplitude[mask] - threshold) / ratio
                    ) / amplitude[mask]
                    audio = audio * attenuation
                    print(
                        f"🔊 Compression appliquée au stem 'bass' pour plus de présence"
                    )

                # Normaliser le volume du stem
                current_peak = np.max(np.abs(audio))
                if current_peak > 0:
                    # Ajuster le niveau selon le type de stem
                    # Les valeurs peuvent être ajustées selon l'importance souhaitée de chaque stem
                    target_level = 0.7  # Valeur par défaut

                    if stem == "drums":
                        target_level = 0.8  # Drums légèrement plus forts
                    elif stem == "bass":
                        target_level = 0.75  # Basse forte aussi
                    elif stem == "other":
                        target_level = 0.6  # Autres éléments un peu plus bas

                    target_gain = target_level / current_peak
                    max_gain = 5.0
                    target_gain = min(target_gain, max_gain)
                    audio = audio * target_gain

                    gain_db = 20 * np.log10(target_gain) if target_gain > 0 else 0
                    print(f"🔊 Normalisation du stem '{stem}' (gain: {gain_db:.1f} dB)")

                # Ajouter au mix
                if mixed_audio is None:
                    mixed_audio = audio
                    sr = sr_curr
                else:
                    # Assurer que les deux audios ont la même longueur
                    if len(audio) > len(mixed_audio):
                        audio = audio[: len(mixed_audio)]
                    elif len(audio) < len(mixed_audio):
                        # Padding avec des zéros
                        audio = np.pad(audio, (0, len(mixed_audio) - len(audio)))

                    # Mixer avec une pondération pour éviter l'écrêtage
                    mixing_weight = 1.0 / len(valid_stems)
                    mixed_audio = (
                        mixed_audio * (1.0 - mixing_weight) + audio * mixing_weight
                    )

            # Normalisation finale du mix combiné
            if mixed_audio is not None:
                final_peak = np.max(np.abs(mixed_audio))
                if final_peak > 0.95:  # Éviter l'écrêtage
                    mixed_audio = mixed_audio * (0.95 / final_peak)

                # Enregistrer le stem mixé
                sf.write(mixed_output_path, mixed_audio, sr)
                print(
                    f"✅ Stems mixés et sauvegardés: {os.path.basename(mixed_output_path)}"
                )
                return mixed_output_path, valid_stems
            else:
                print("❌ Aucun audio n'a pu être mixé")
                return None, None

        except Exception as e:
            print(f"⚠️ Erreur lors du mixage des stems: {e}")
            import traceback

            traceback.print_exc()
            return None, None

    def _extract_preferred_stem(
        self, spectral_profile, separated_path, layer_id, preferred_stem=None
    ):
        """Version compatible de l'ancienne méthode, qui redirige vers la nouvelle"""
        if isinstance(preferred_stem, list):
            return self._extract_multiple_stems(
                spectral_profile, separated_path, layer_id, preferred_stem
            )
        else:
            return self._extract_multiple_stems(
                spectral_profile, separated_path, layer_id, [preferred_stem]
            )

    def _analyze_sample_with_demucs(self, sample_path, temp_output_dir):
        """Analyse un sample avec Demucs et retourne la composition spectrale tout en préservant les stems"""

        # Créer un dossier temporaire pour l'analyse
        os.makedirs(temp_output_dir, exist_ok=True)

        # Exécuter Demucs avec le modèle htdemucs (4 stems)
        cmd = [
            sys.executable,
            "-m",
            "demucs",
            "--out",
            temp_output_dir,
            "-n",
            "htdemucs",  # Modèle standard
            sample_path,
        ]

        print(f"🔊 Analyse audio : Exécution de la commande Demucs: {' '.join(cmd)}")
        process = subprocess.run(cmd, capture_output=True, text=True)

        if process.returncode != 0:
            print(f"❌ Erreur Demucs: {process.stderr}")
            # En cas d'erreur, retourner un profil par défaut
            return self.create_default_profile(os.path.basename(sample_path)), None

        # Analyser les résultats
        sample_name = Path(sample_path).stem
        model_name = "htdemucs"
        separated_path = Path(temp_output_dir) / model_name / sample_name

        if not separated_path.exists():
            print(f"❌ Répertoire d'analyse non trouvé: {separated_path}")
            return self.create_default_profile(os.path.basename(sample_path)), None

        # Vérifier les stems disponibles
        available_stems = [f.stem for f in separated_path.glob("*.wav")]
        print(f"🔍 Stems disponibles: {', '.join(available_stems)}")

        # Calculer l'énergie relative de chaque stem
        stem_energy = {}
        total_energy = 0

        for stem in [
            "drums",
            "bass",
            "other",
            "vocals",
        ]:  # Modèle htdemucs standard a 4 stems
            stem_path = separated_path / f"{stem}.wav"
            if stem_path.exists():
                # Charger et analyser le stem
                try:
                    audio, sr = librosa.load(str(stem_path), sr=None, mono=True)
                    energy = np.sum(audio**2)
                    stem_energy[stem] = energy
                    total_energy += energy
                except Exception as e:
                    print(f"⚠️ Erreur lors de l'analyse du stem {stem}: {e}")
                    stem_energy[stem] = 0.01  # Valeur par défaut minimale

        # Normaliser l'énergie
        if total_energy > 0:
            for stem in stem_energy:
                stem_energy[stem] /= total_energy

        # Trouver le stem dominant
        if stem_energy:
            dominant_stem = max(stem_energy, key=stem_energy.get)
            dominant_value = stem_energy[dominant_stem]
            print(f"📊 Stem dominant: {dominant_stem} ({dominant_value:.2%})")
        else:
            dominant_stem = None
            print("⚠️ Aucun stem n'a pu être analysé")

        # Retourner à la fois le profil spectral et le chemin vers les stems
        return stem_energy, separated_path
