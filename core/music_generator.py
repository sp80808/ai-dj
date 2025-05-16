import torch
import numpy as np
from audiocraft.models import MusicGen
import tempfile
import os
from config.music_prompts import MUSICGEN_TEMPLATES, SAMPLE_PARAMS


class MusicGenerator:
    """Générateur de samples musicaux avec MusicGen"""

    def __init__(self, model_size="medium"):
        """
        Initialise le générateur de musique

        Args:
            model_size (str): Taille du modèle MusicGen ('small', 'medium', 'large')
        """
        print(f"Initialisation de MusicGen ({model_size})...")
        self.model = MusicGen.get_pretrained(model_size)

        # Définir les paramètres par défaut
        self.model.set_generation_params(
            duration=8,  # 8 secondes par défaut
            temperature=1.0,  # Contrôle la randomité
            top_k=250,  # Filtrage top-k
            top_p=0.0,  # Pas de nucleus sampling par défaut
            use_sampling=True,
        )
        print("MusicGen initialisé!")

        # Stockage des samples générés
        self.sample_cache = {}

    def generate_sample(
        self,
        sample_type,
        tempo,
        key=None,
        intensity=5,
        style_tag=None,
        musicgen_prompt_keywords=None,
        genre=None,
    ):
        """
        Génère un sample audio basé sur les paramètres fournis

        Args:
            sample_type (str): Type de sample (ex: techno_kick, techno_bass)
            tempo (int): Tempo en BPM
            key (str, optional): Tonalité (ex: C minor, A major)
            intensity (int): Intensité/énergie de 1 à 10
            style_tag (str, optional): Tag de style spécifique
            musicgen_prompt_keywords (list, optional): Liste de mots-clés pour affiner le prompt

        Returns:
            tuple: (sample_audio, sample_info)
        """
        try:
            # Récupérer les paramètres pour ce type de sample
            params = SAMPLE_PARAMS.get(
                sample_type,
                {
                    "duration": 8,
                    "should_start_with_kick": False,
                    "key_sensitive": False,
                },
            )

            # Définir la durée de génération
            self.model.set_generation_params(duration=params["duration"])

            if not genre:
                # Déduire le genre du type de sample
                if sample_type.startswith("techno_"):
                    genre = "techno"
                elif sample_type.startswith("hiphop_") or sample_type.startswith(
                    "hip_hop_"
                ):
                    genre = "hip-hop"
                elif sample_type.startswith("rock_"):
                    genre = "rock"
                elif sample_type.startswith("classical_") or sample_type.startswith(
                    "orchestral_"
                ):
                    genre = "classical"
                elif sample_type.startswith("ambient_") or sample_type.startswith(
                    "downtempo_"
                ):
                    genre = "ambient"
                elif sample_type.startswith("dub_") or sample_type.startswith(
                    "reggae_"
                ):
                    genre = "dub"
                elif sample_type.startswith("jungle_") or sample_type.startswith(
                    "dnb_"
                ):
                    genre = "jungle_dnb"
                elif sample_type.startswith("house_") or sample_type.startswith(
                    "deep_house_"
                ):
                    genre = "deep_house"
                elif sample_type.startswith("triphop_"):
                    genre = "trip-hop"
                else:
                    genre = "electronic"

            if genre == "hip-hop":
                template = "A {style_tag} hip-hop sound at {tempo} BPM, {key}"
            elif genre == "rock":
                template = "A {style_tag} rock sound at {tempo} BPM, {key}"
            elif genre == "classical":
                template = "A {style_tag} orchestral sound at {tempo} BPM, {key}"
            elif genre == "ambient":
                template = "A {style_tag} ambient atmosphere at {tempo} BPM, {key}"
            elif genre == "dub":
                template = "A {style_tag} dub reggae sound at {tempo} BPM, {key}"
            elif genre == "jungle_dnb":
                template = "A {style_tag} drum and bass sound at {tempo} BPM, {key}"
            elif genre == "deep_house":
                template = "A {style_tag} deep house sound at {tempo} BPM, {key}"
            elif genre == "trip-hop":
                template = "A {style_tag} trip-hop sound at {tempo} BPM, {key}"
            else:
                template = MUSICGEN_TEMPLATES.get(
                    sample_type, "A {style_tag} sound at {tempo} BPM, {key}"
                )

            if not style_tag:
                if genre == "hip-hop":
                    style_tag = "boom bap beats"
                elif genre == "rock":
                    style_tag = "guitar rock"
                elif genre == "classical":
                    style_tag = "orchestral cinematic"
                elif genre == "ambient":
                    style_tag = "atmospheric ethereal"
                elif genre == "dub":
                    style_tag = "deep reggae dub"
                elif genre == "jungle_dnb":
                    style_tag = "breakbeat jungle"
                elif genre == "deep_house":
                    style_tag = "soulful jazzy house"
                elif genre == "trip-hop":
                    style_tag = "cinematic melancholic downtempo"
                else:
                    style_tag = "minimal techno"

            # Ajuster l'intensité
            intensity_words = [
                "very soft",
                "soft",
                "gentle",
                "moderate",
                "medium",
                "energetic",
                "driving",
                "powerful",
                "intense",
                "very intense",
            ]
            intensity_desc = intensity_words[
                min(intensity - 1, len(intensity_words) - 1)
            ]

            # Traiter les mots-clés supplémentaires
            keyword_str = ""
            if musicgen_prompt_keywords and isinstance(musicgen_prompt_keywords, list):
                keyword_str = ", ".join(musicgen_prompt_keywords)

            # Construire le prompt final
            base_prompt = template.format(
                tempo=tempo,
                key=key if key and params["key_sensitive"] else "",
                style_tag=f"{intensity_desc} {style_tag}",
            )

            # Ajouter les mots-clés si présents
            if keyword_str:
                prompt = f"{base_prompt}, {keyword_str}"
            else:
                prompt = base_prompt

            print(f"🔮 Génération sample avec prompt: '{prompt}'")

            # Générer le sample
            self.model.set_generation_params(
                duration=params["duration"],
                temperature=0.7
                + (intensity * 0.03),  # Plus d'intensité = plus de randomité
            )

            # Génération du sample
            print("\n🎵 Génération audio en cours...")
            wav = self.model.generate([prompt])
            print(f"✅ Génération terminée !")
            # Convertir en numpy array de manière sécurisée
            with torch.no_grad():
                wav_np = wav.cpu().detach().numpy()

            # Extraire le premier sample du batch
            sample_audio = wav_np[0, 0]  # [batch, channel, sample]

            # Créer les métadonnées du sample
            sample_info = {
                "type": sample_type,
                "tempo": tempo,
                "key": key,
                "intensity": intensity,
                "duration": params["duration"],
                "prompt": prompt,
                "should_start_with_kick": params["should_start_with_kick"],
                "keywords": musicgen_prompt_keywords,
            }

            return sample_audio, sample_info

        except Exception as e:
            print(f"Erreur lors de la génération du sample: {str(e)}")
            # En cas d'erreur, retourner un silence et des infos de base
            silence = np.zeros(44100 * 4)  # 4 secondes de silence
            error_info = {"type": sample_type, "tempo": tempo, "error": str(e)}
            return silence, error_info

    def save_sample(self, sample_audio, filename, sample_rate=32000):
        """Sauvegarde un sample généré sur disque"""
        try:
            # Sauvegarder l'audio en tant que WAV
            if filename.endswith(".wav"):
                path = filename
            else:
                temp_dir = tempfile.gettempdir()
                path = os.path.join(temp_dir, filename)

            # Vérifier que sample_audio est un numpy array
            if not isinstance(sample_audio, np.ndarray):
                sample_audio = np.array(sample_audio)

            # Normaliser
            max_val = np.max(np.abs(sample_audio))
            if max_val > 0:
                sample_audio = sample_audio / max_val * 0.9
            import soundfile as sf

            sf.write(path, sample_audio, sample_rate)

            return path
        except Exception as e:
            print(f"Erreur lors de la sauvegarde du sample: {e}")
            return None
