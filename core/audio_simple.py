import numpy as np
import sounddevice as sd
import soundfile as sf
import time
import threading
import queue
import os


class SimpleAudioPlayer:
    """Système audio simplifié pour une lecture fluide"""

    def __init__(self, sample_rate=44100):
        self.sample_rate = sample_rate
        self.playing = False
        self.audio_queue = queue.Queue(maxsize=10)
        self.output_stream = None
        self.output_thread = None
        self.current_buffer = np.zeros(sample_rate)  # 1 seconde de silence

    def start(self):
        """Démarre la lecture audio"""
        if self.playing:
            return

        self.playing = True

        # Créer et démarrer le thread de lecture
        self.output_thread = threading.Thread(target=self._output_loop)
        self.output_thread.daemon = True
        self.output_thread.start()

        print("Lecture audio démarrée")

    def stop(self):
        """Arrête la lecture audio"""
        self.playing = False

        if self.output_stream:
            self.output_stream.stop()
            self.output_stream.close()
            self.output_stream = None

        if self.output_thread:
            self.output_thread.join(timeout=1)

        print("Lecture audio arrêtée")

    def _audio_callback(self, outdata, frames, time, status):
        """Callback pour la sortie audio"""
        if status:
            print(f"Statut audio: {status}")

        # Extraire les données du buffer courant
        if frames <= len(self.current_buffer):
            outdata[:] = self.current_buffer[:frames].reshape(-1, 1)
            # Décaler le buffer
            self.current_buffer = np.concatenate(
                [self.current_buffer[frames:], np.zeros(frames)]
            )
        else:
            # Pas assez de données, compléter avec des zéros
            outdata[: len(self.current_buffer)] = self.current_buffer.reshape(-1, 1)
            outdata[len(self.current_buffer) :] = 0
            self.current_buffer = np.zeros(0)

    def _output_loop(self):
        """Thread de lecture audio en continu"""
        try:
            # Créer et démarrer le stream audio
            self.output_stream = sd.OutputStream(
                samplerate=self.sample_rate,
                channels=1,
                callback=self._audio_callback,
                blocksize=1024,  # Petit blocksize pour plus de fluidité
            )
            self.output_stream.start()

            # Boucle principale
            while self.playing:
                try:
                    # Essayer d'obtenir de nouveaux échantillons
                    audio = self.audio_queue.get(timeout=0.1)

                    # Fondu enchaîné pour éviter les clics
                    if len(self.current_buffer) > 0:
                        fade_len = min(1024, len(audio), len(self.current_buffer))
                        fade_out = np.linspace(1.0, 0.0, fade_len)
                        fade_in = np.linspace(0.0, 1.0, fade_len)

                        # Appliquer le fondu
                        self.current_buffer[-fade_len:] *= fade_out
                        audio_fade = audio.copy()
                        audio_fade[:fade_len] *= fade_in

                        # Superposition
                        audio_fade[:fade_len] += self.current_buffer[-fade_len:]

                        # Mettre à jour le buffer
                        self.current_buffer = np.concatenate(
                            [self.current_buffer[:-fade_len], audio_fade]
                        )
                    else:
                        # Premier buffer ou buffer vide
                        self.current_buffer = audio

                except queue.Empty:
                    # Aucun nouvel audio disponible, attendre
                    time.sleep(0.05)

        except Exception as e:
            print(f"Erreur dans le thread audio: {e}")
        finally:
            # Fermer le stream si encore ouvert
            if self.output_stream:
                self.output_stream.stop()
                self.output_stream.close()
                self.output_stream = None

    def play_audio(self, audio, wait=False):
        """
        Ajoute de l'audio à la queue de lecture

        Args:
            audio (np.array): Audio à jouer
            wait (bool): Si True, attend que l'audio soit joué avant de retourner
        """
        try:
            # Convertir en numpy array si nécessaire
            if not isinstance(audio, np.ndarray):
                audio = np.array(audio)

            # Normaliser entre -1 et 1
            max_val = np.max(np.abs(audio))
            if max_val > 0:
                audio = audio / max_val * 0.9

            # Ajouter à la queue
            self.audio_queue.put(audio)

            if wait:
                # Attendre approximativement le temps de lecture
                duration = len(audio) / self.sample_rate
                time.sleep(duration)

            return True
        except Exception as e:
            print(f"Erreur lors de la lecture: {e}")
            return False

    def play_file(self, filename, wait=False):
        """Joue un fichier audio"""
        try:
            # Vérifier que le fichier existe
            if not os.path.exists(filename):
                print(f"Fichier introuvable: {filename}")
                return False

            # Essayer de lire le fichier
            try:
                audio, sr = sf.read(filename)
            except Exception as e:
                print(f"Erreur lors de la lecture du fichier {filename}: {e}")
                # Générer 4 secondes de silence comme fallback
                audio = np.zeros(self.sample_rate * 4)
                sr = self.sample_rate

            # Convertir en mono si stéréo
            if len(audio.shape) > 1:
                audio = np.mean(audio, axis=1)

            # Rééchantillonner si nécessaire
            if sr != self.sample_rate:
                try:
                    import librosa

                    audio = librosa.resample(
                        audio, orig_sr=sr, target_sr=self.sample_rate
                    )
                except Exception as e:
                    print(f"Erreur de resampling: {e}")
                    # En cas d'échec, simplement étirer/comprimer l'audio
                    ratio = self.sample_rate / sr
                    new_length = int(len(audio) * ratio)
                    audio = np.interp(
                        np.linspace(0, len(audio) - 1, new_length),
                        np.arange(len(audio)),
                        audio,
                    )

            return self.play_audio(audio, wait)
        except Exception as e:
            print(f"Erreur lors de la lecture du fichier: {e}")
            # Jouer un silence en cas d'erreur
            silence = np.zeros(self.sample_rate * 2)  # 2 secondes
            return self.play_audio(silence, wait)

    def save_mix(self, filename, audio):
        """Sauvegarde un mix dans un fichier"""
        try:
            # Normaliser
            max_val = np.max(np.abs(audio))
            if max_val > 0:
                audio = audio / max_val * 0.9

            # Sauvegarder
            sf.write(filename, audio, self.sample_rate)
            return True
        except Exception as e:
            print(f"Erreur lors de la sauvegarde: {e}")
            return False
