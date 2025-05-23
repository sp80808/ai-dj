import os
import time
import numpy as np
import soundfile as sf
from core.layer_manager import LayerManager
from core.midi_clock_manager import MidiClockManager


class AudioLoopPlayer:
    """Système de lecture de boucles audio avec transitions douces basé sur LayerManager"""

    def __init__(self, sample_rate=48000, output_device=None, adjust_bpm=False):
        """
        Initialise le lecteur de boucles audio

        Args:
            sample_rate: Taux d'échantillonnage en Hz
            output_device: Périphérique de sortie audio (None = défaut)
        """
        self.sample_rate = sample_rate
        self.output_device = output_device
        self.playing = False
        self.adjust_bpm = adjust_bpm
        self.midi_clock_manager = MidiClockManager()
        self.midi_clock_manager.start()
        # Créer un répertoire temporaire pour les boucles
        self.temp_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "temp_loops"
        )
        os.makedirs(self.temp_dir, exist_ok=True)

        # Initialiser le LayerManager
        self.layer_manager = LayerManager(
            sample_rate=sample_rate,
            output_dir=self.temp_dir,
            num_channels=4,  # Moins de canaux nécessaires pour le client
        )

        # ID du layer actuel
        self.current_layer_id = "current_loop"
        self.next_layer_id = "next_loop"
        self.is_using_current = True  # Pour alterner entre les deux layers

    def start(self):
        """Démarre le lecteur audio"""
        if self.playing:
            return

        # Initialiser pygame.mixer via le LayerManager
        self.playing = True

        print(
            f"🔊 Lecteur audio démarré (Sortie: {self.output_device or 'défaut'}, SR: {self.sample_rate}Hz)"
        )

    def stop(self):
        """Arrête le lecteur audio"""
        self.playing = False

        # Arrêter tous les layers
        self.layer_manager.stop_all_layers(fade_ms=500)

        print("⏹️  Lecteur audio arrêté")

    def add_loop(self, audio_data, sample_rate):
        """
        Ajoute une nouvelle boucle au lecteur et remplace la précédente sans créer de silence

        Args:
            audio_data: Données audio (numpy array)
            sample_rate: Taux d'échantillonnage des données
        """
        if audio_data is None or len(audio_data) == 0:
            print("❌ Erreur: Données audio invalides ou vides")
            return False
        if not self.playing:
            self.start()

        try:
            # Appliquer un léger fade-in/fade-out pour éviter les clics
            fade_ms = 2
            fade_samples = int(sample_rate * fade_ms / 1000)

            # S'assurer que l'audio est assez long pour le fade
            if len(audio_data) > 2 * fade_samples:
                # Créer les rampes de fade
                if (
                    len(audio_data.shape) > 1 and audio_data.shape[1] >= 2
                ):  # Audio stéréo
                    # Créer des rampes pour audio stéréo
                    fade_in = np.linspace(0.0, 1.0, fade_samples).reshape(-1, 1)
                    fade_out = np.linspace(1.0, 0.0, fade_samples).reshape(-1, 1)

                    if audio_data.shape[1] == 2:
                        # Répéter pour les deux canaux
                        fade_in = np.tile(fade_in, (1, 2))
                        fade_out = np.tile(fade_out, (1, 2))

                    # Appliquer les fades
                    audio_data[:fade_samples] = audio_data[:fade_samples] * fade_in
                    audio_data[-fade_samples:] = audio_data[-fade_samples:] * fade_out

                else:  # Audio mono
                    fade_in = np.linspace(0.0, 1.0, fade_samples)
                    fade_out = np.linspace(1.0, 0.0, fade_samples)

                    # Appliquer les fades
                    audio_data[:fade_samples] = audio_data[:fade_samples] * fade_in
                    audio_data[-fade_samples:] = audio_data[-fade_samples:] * fade_out

                print(
                    f"✓ Fade-in/fade-out de {fade_ms}ms appliqué pour éliminer les clics"
                )
        except Exception as e:
            print(f"⚠️  Avertissement: Impossible d'appliquer le fade: {str(e)}")

        try:
            # 1. Déterminer l'ID du layer à utiliser
            # Utiliser toujours le même ID pour assurer un remplacement fluide
            layer_id = "current_loop"

            # 2. Sauvegarder temporairement l'audio dans un fichier
            timestamp = int(time.time())
            temp_file = os.path.join(self.temp_dir, f"temp_audio_{timestamp}.wav")
            sf.write(temp_file, audio_data, sample_rate)

            # 3. Préparer les paramètres pour le LayerManager
            sample_details = {
                "original_file_path": temp_file,
                "measures": 1,  # Nombre de mesures par défaut
                "type": "generated_loop",
                "key": "unknown",  # La tonalité n'est pas critique ici
            }

            playback_params = {"volume": 0.9, "pan": 0.0, "start_behavior": "next_bar"}

            # 4. Utiliser directement "add_replace" qui gère le remplacement fluide sans silence
            self.layer_manager.manage_layer(
                layer_id,
                "add_replace",  # Cette opération remplace automatiquement le layer existant
                sample_details,
                playback_params,
                [],  # Pas d'effets
                prepare_sample_for_loop=False,
                use_sync=True,
                midi_clock_manager=self.midi_clock_manager,
            )

            print(f"🎵 Nouvelle boucle chargée et synchronisée")
            return True
        except Exception as e:
            print(f"❌ Erreur lors de l'ajout de la boucle: {str(e)}")
            return False
