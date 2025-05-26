import torch
import numpy as np
import tempfile
import os
import random
import gc
from config.music_prompts import SAMPLE_PARAMS


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

    def generate_sample(self, musicgen_prompt, tempo, sample_type="custom"):
        """
        Version simplifiée qui prend juste un prompt MusicGen tout fait
        (remplace l'ancienne méthode compliquée)
        """
        try:
            print(f"🔮 Génération directe avec prompt: '{musicgen_prompt}'")

            # Paramètres par défaut
            params = SAMPLE_PARAMS.get(
                sample_type,
                {
                    "duration": 8,
                    "should_start_with_kick": False,
                    "key_sensitive": False,
                },
            )

            if self.model_type == "musicgen":
                self.model.set_generation_params(
                    duration=params["duration"],
                    temperature=0.8,  # Température fixe, plus besoin de calculer
                )

                # Génération directe avec le prompt fourni
                wav = self.model.generate([musicgen_prompt])

                with torch.no_grad():
                    wav_np = wav.cpu().detach().numpy()

                sample_audio = wav_np[0, 0]

            elif self.model_type == "stable-audio":
                from einops import rearrange
                from stable_audio_tools.inference.generation import (
                    generate_diffusion_cond,
                )

                seconds_total = 12
                conditioning = [
                    {
                        "prompt": musicgen_prompt,
                        "seconds_start": 0,
                        "seconds_total": seconds_total,
                    }
                ]

                # Paramètres fixes pour Stable Audio
                cfg_scale = 7.0
                steps_value = 75
                seed_value = random.randint(0, 2**31 - 1)

                print(f"⚙️  Stable Audio: steps={steps_value}, cfg_scale={cfg_scale}")

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

                target_samples = int(seconds_total * self.sample_rate)
                output = rearrange(output, "b d n -> d (b n)")

                if output.shape[1] > target_samples:
                    output = output[:, :target_samples]

                output_normalized = (
                    output.to(torch.float32)
                    .div(torch.max(torch.abs(output) + 1e-8))
                    .cpu()
                    .numpy()
                )

                sample_audio = (
                    output_normalized[0]
                    if output_normalized.shape[0] > 1
                    else output_normalized
                )

                del output, output_normalized
                if torch.cuda.is_available():
                    torch.cuda.empty_cache()
                gc.collect()

            print(f"✅ Génération terminée !")

            sample_info = {
                "type": sample_type,
                "tempo": tempo,
                "prompt": musicgen_prompt,
                "should_start_with_kick": params["should_start_with_kick"],
            }

            return sample_audio, sample_info

        except Exception as e:
            print(f"❌ Erreur génération: {str(e)}")
            silence = np.zeros(44100 * 4)
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
            
            # ✅ NOUVEAU : Resample vers 48kHz si nécessaire
            if self.sample_rate != 48000:
                print(f"🔄 Resampling {self.sample_rate}Hz → 48000Hz")
                import librosa
                sample_audio = librosa.resample(
                    sample_audio, 
                    orig_sr=self.sample_rate, 
                    target_sr=48000
                )
                # Mettre à jour le sample rate pour la sauvegarde
                save_sample_rate = 48000
            else:
                save_sample_rate = self.sample_rate
                
            # Normaliser
            max_val = np.max(np.abs(sample_audio))
            if max_val > 0:
                sample_audio = sample_audio / max_val * 0.9
                
            import soundfile as sf
            sf.write(path, sample_audio, save_sample_rate)  # ← Utilise le bon sample rate
            
            return path
        except Exception as e:
            print(f"❌ Erreur lors de la sauvegarde du sample: {e}")
            return None
