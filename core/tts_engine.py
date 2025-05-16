import pyttsx3
import numpy as np
import tempfile
import os
import soundfile as sf


class DJSpeech:
    """Synthèse vocale pour le DJ"""

    def __init__(self, voice_id=None):
        """
        Initialise le moteur TTS

        Args:
            voice_id (str, optional): ID de la voix à utiliser
        """
        # Initialiser le moteur pyttsx3 (TTS local)
        self.engine = pyttsx3.init()

        # Définir la voix si spécifiée
        if voice_id:
            self.engine.setProperty("voice", voice_id)

        # Paramètres par défaut
        self.engine.setProperty("rate", 150)  # Vitesse de parole
        self.engine.setProperty("volume", 0.9)  # Volume

        # Liste les voix disponibles
        voices = self.engine.getProperty("voices")
        print(f"Voix disponibles: {len(voices)}")
        for i, voice in enumerate(voices):
            print(f"  {i}: {voice.id} - {voice.name}")

        # Utiliser la première voix par défaut
        if voices:
            self.engine.setProperty("voice", voices[0].id)

    def generate_speech(self, text, energy_level=5, rate_factor=1.0):
        """
        Génère un audio TTS

        Args:
            text (str): Texte à prononcer
            energy_level (int): Niveau d'énergie (1-10)
            rate_factor (float): Facteur de vitesse

        Returns:
            np.array: Audio de la voix
        """
        # Ajuster la vitesse en fonction de l'énergie
        # Plus d'énergie = parle plus vite
        rate = int(150 * rate_factor * (0.8 + energy_level * 0.04))
        self.engine.setProperty("rate", rate)

        # Ajuster le volume en fonction de l'énergie
        volume = min(1.0, 0.7 + energy_level * 0.03)
        self.engine.setProperty("volume", volume)

        # Générer dans un fichier temporaire
        temp_file = os.path.join(tempfile.gettempdir(), "dj_speech.wav")
        self.engine.save_to_file(text, temp_file)
        self.engine.runAndWait()

        # Lire le fichier
        audio, sr = sf.read(temp_file)

        # Convertir en mono si stéréo
        if len(audio.shape) > 1:
            audio = np.mean(audio, axis=1)

        return audio, sr

    def mix_with_music(
        self, speech_audio, music, speech_sr, music_sr, volume_ratio=0.8
    ):
        """
        Mixe la voix avec la musique

        Args:
            speech_audio (np.array): Audio de la voix
            music (np.array): Audio de la musique
            speech_sr (int): Taux d'échantillonnage de la voix
            music_sr (int): Taux d'échantillonnage de la musique
            volume_ratio (float): Ratio voix/musique

        Returns:
            np.array: Audio mixé
        """
        # Rééchantillonner la voix si nécessaire
        if speech_sr != music_sr:
            import librosa

            speech_audio = librosa.resample(
                speech_audio, orig_sr=speech_sr, target_sr=music_sr
            )

        # S'assurer que la musique est assez longue
        if len(music) < len(speech_audio):
            # Dupliquer la musique si nécessaire
            repeats = (len(speech_audio) // len(music)) + 1
            music = np.tile(music, repeats)[: len(speech_audio)]

        # Réduire le volume de la musique pendant la parole
        music_volume = 1.0 - (volume_ratio * 0.8)
        ducked_music = music[: len(speech_audio)] * music_volume

        # Mixer
        mixed = ducked_music + (speech_audio * volume_ratio)

        # Normaliser
        max_val = np.max(np.abs(mixed))
        if max_val > 0:
            mixed = mixed / max_val * 0.9

        return mixed, music_sr
