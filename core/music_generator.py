import torch
import numpy as np
import tempfile
import os
import random
import gc
from config.music_prompts import MUSICGEN_TEMPLATES, SAMPLE_PARAMS


class MusicGenerator:
    """Générateur de samples musicaux avec différents modèles (MusicGen ou Stable Audio)"""

    def __init__(self, model_name="musicgen-medium", default_duration=8.0):
        """
        Initialise le générateur de musique

        Args:
            model_name (str): Nom du modèle à utiliser ('musicgen-small', 'musicgen-medium',
                              'musicgen-large', 'stable-audio-open', 'stable-audio-pro')
            default_duration (float): Durée par défaut des générations en secondes
        """
        self.model_name = model_name
        self.default_duration = default_duration
        self.model = None
        self.sample_rate = 32000

        if "musicgen" in model_name:
            size = model_name.split("-")[1] if "-" in model_name else "medium"
            print(f"Initialisation de MusicGen ({size})...")
            from audiocraft.models import MusicGen

            self.model = MusicGen.get_pretrained(size)
            self.model_type = "musicgen"

            # Définir les paramètres par défaut pour MusicGen
            self.model.set_generation_params(
                duration=default_duration,
                temperature=1.0,
                top_k=250,
                top_p=0.0,
                use_sampling=True,
            )
            print("MusicGen initialisé!")

        elif "stable-audio" in model_name:
            print(f"Initialisation de Stable Audio ({model_name})...")
            try:
                # Utiliser l'approche du code fourni plutôt que StableAudio
                import torch
                from stable_audio_tools import get_pretrained_model

                # Définir le device
                device = "cuda" if torch.cuda.is_available() else "cpu"
                print(f"Utilisation du dispositif: {device}")

                # Télécharger le modèle et le déplacer sur GPU
                model_id = "stabilityai/stable-audio-open-1.0"
                if model_name == "stable-audio-pro":
                    model_id = "stabilityai/stable-audio-pro-1.0"

                self.model, self.model_config = get_pretrained_model(model_id)
                self.sample_rate = self.model_config["sample_rate"]
                self.sample_size = self.model_config["sample_size"]
                self.model = self.model.to(device)
                self.device = device

                self.model_type = "stable-audio"
                print(f"Stable Audio initialisé (sample rate: {self.sample_rate}Hz)!")

            except ImportError as e:
                print(f"⚠️ Erreur: {e}")
                print(
                    "Installation: pip install torch torchaudio stable-audio-tools einops"
                )
                # Fallback à MusicGen si Stable Audio n'est pas disponible
                print("Fallback à MusicGen medium")
                from audiocraft.models import MusicGen

                self.model = MusicGen.get_pretrained("medium")
                self.sample_rate = self.model.sample_rate
                self.model_type = "musicgen"

        else:
            # Modèle inconnu, fallback à MusicGen
            print(f"⚠️ Modèle {model_name} inconnu, fallback à MusicGen medium")
            from audiocraft.models import MusicGen

            self.model = MusicGen.get_pretrained("medium")
            self.model_type = "musicgen"

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
            custom_prompt = None
            if musicgen_prompt_keywords and isinstance(musicgen_prompt_keywords, list):
                # Détecter si c'est une demande utilisateur complète
                if (
                    any(len(kw.split()) > 2 for kw in musicgen_prompt_keywords)
                    or len(musicgen_prompt_keywords) > 5
                ):
                    # Si c'est un prompt utilisateur complet, l'utiliser directement
                    full_text = " ".join(musicgen_prompt_keywords)
                    if len(full_text) > 15:  # Une phrase complète, probablement
                        custom_prompt = f"Generate this: {full_text}, at {tempo} BPM"
                        if key:
                            custom_prompt += f", in {key}"
                        print(
                            f"🔄 Utilisation du prompt utilisateur complet: '{custom_prompt}'"
                        )
            # Récupérer les paramètres pour ce type de sample
            if custom_prompt:
                prompt = custom_prompt
            else:
                params = SAMPLE_PARAMS.get(
                    sample_type,
                    {
                        "duration": 8,
                        "should_start_with_kick": False,
                        "key_sensitive": False,
                    },
                )

                # Définir la durée de génération
                if self.model_type == "musicgen":
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
                if musicgen_prompt_keywords and isinstance(
                    musicgen_prompt_keywords, list
                ):
                    keyword_str = ", ".join(musicgen_prompt_keywords)

                # Construire le prompt final
                base_prompt = template.format(
                    tempo=tempo,
                    key=key if key and params["key_sensitive"] else "",
                    style_tag=f"{intensity_desc} {style_tag}",
                )

                # Ajouter les mots-clés si présents
                if keyword_str:
                    prompt = f"{base_prompt} {keyword_str}"
                else:
                    prompt = base_prompt

            print(f"🔮 Génération sample avec prompt: '{prompt}'")
            print("\n🎵 Génération audio en cours...")
            if self.model_type == "musicgen":
                # Paramètres spécifiques à MusicGen
                self.model.set_generation_params(
                    duration=params["duration"],
                    temperature=0.7
                    + (intensity * 0.03),  # Plus d'intensité = plus de randomité
                )

                # Génération avec MusicGen
                wav = self.model.generate([prompt])

                # Convertir en numpy array
                with torch.no_grad():
                    wav_np = wav.cpu().detach().numpy()

                # Extraire le premier sample du batch
                sample_audio = wav_np[0, 0]  # [batch, channel, sample]

            elif self.model_type == "stable-audio":
                # Importer les modules nécessaires pour Stable Audio
                from einops import rearrange
                from stable_audio_tools.inference.generation import (
                    generate_diffusion_cond,
                )

                seconds_total = 12
                # Créer le format de conditionnement attendu par Stable Audio
                conditioning = [
                    {
                        "prompt": prompt,
                        "seconds_start": 0,
                        "seconds_total": seconds_total,
                    }
                ]
                intensity = 5 if intensity is None else intensity
                # Paramètres ajustés selon l'intensité
                cfg_scale = min(5.0 + (intensity * 0.5), 9.0)  # 5.0-9.0
                steps_value = int(50 + (intensity * 5))  # 50-100 steps
                seed_value = random.randint(0, 2**31 - 1)

                print(
                    f"⚙️  Génération Stable Audio: steps={steps_value}, cfg_scale={cfg_scale}, seed={seed_value}"
                )

                # Générer l'audio
                output = generate_diffusion_cond(
                    self.model,
                    steps=steps_value,
                    cfg_scale=cfg_scale,
                    conditioning=conditioning,
                    sample_size=self.sample_size,
                    sigma_min=0.3,
                    sigma_max=500,
                    sampler_type="dpmpp-3m-sde",
                    device=self.device,
                    seed=seed_value,
                )

                # Calculer le nombre d'échantillons cible
                target_samples = int(seconds_total * self.sample_rate)

                # Formater l'audio
                output = rearrange(output, "b d n -> d (b n)")

                # Tronquer si nécessaire
                if output.shape[1] > target_samples:
                    output = output[:, :target_samples]

                # Normaliser en audio PCM standard
                output_normalized = (
                    output.to(torch.float32)
                    .div(torch.max(torch.abs(output) + 1e-8))
                    .cpu()
                    .numpy()
                )

                # Prendre le premier canal si stéréo
                sample_audio = (
                    output_normalized[0]
                    if output_normalized.shape[0] > 1
                    else output_normalized
                )
                del output, output_normalized
                if torch.cuda.is_available():
                    torch.cuda.empty_cache()
                gc.collect()

            print(f"✅ Génération terminée !\n")

            # Créer les métadonnées du sample
            sample_info = {
                "type": sample_type,
                "tempo": tempo,
                "key": key,
                "intensity": intensity,
                "duration": seconds_total,
                "prompt": prompt,
                "should_start_with_kick": params["should_start_with_kick"],
                "keywords": musicgen_prompt_keywords,
            }

            return sample_audio, sample_info

        except Exception as e:
            print(f"❌ Erreur lors de la génération du sample: {str(e)}")
            # En cas d'erreur, retourner un silence et des infos de base
            silence = np.zeros(44100 * 4)  # 4 secondes de silence
            error_info = {"type": sample_type, "tempo": tempo, "error": str(e)}
            return silence, error_info

    def save_sample(self, sample_audio, filename):
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

            sf.write(path, sample_audio, self.sample_rate)

            return path
        except Exception as e:
            print(f"❌ Erreur lors de la sauvegarde du sample: {e}")
            return None
