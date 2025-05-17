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

        print("\n🎵 Lecture audio démarrée")

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

    def play_audio(
        self,
        audio,
        wait=False,
        master_volume=1.2,
        master_compression=True,
        master_limiting=True,
    ):
        """
        Ajoute de l'audio à la queue de lecture avec traitement

        Args:
            audio (np.array): Audio à jouer
            wait (bool): Si True, attend que l'audio soit joué avant de retourner
            master_volume (float): Volume master (0.0-2.0)
            master_compression (bool): Activer la compression
            master_limiting (bool): Activer le limiteur
        """
        try:
            # Convertir en numpy array si nécessaire
            if not isinstance(audio, np.ndarray):
                audio = np.array(audio)

            # Applique le traitement master
            processed = self.apply_master_effects(
                audio,
                master_volume=master_volume,
                master_compression=master_compression,
                master_limiting=master_limiting,
            )

            # Ajouter à la queue
            self.audio_queue.put(processed)

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

    def apply_master_effects(
        self,
        audio_data,
        master_volume=1.0,
        master_compression=True,
        master_limiting=True,
    ):
        """
        Applique des effets de mastering à l'audio avant lecture

        Args:
            audio_data (np.array): Audio à traiter
            master_volume (float): Volume master (0.0-2.0)
            master_compression (bool): Activer la compression
            master_limiting (bool): Activer le limiteur
        Returns:
            np.array: Audio traité
        """
        # Copie de l'audio pour éviter de modifier l'original
        processed = audio_data.copy()

        # Compression multibande pour plus de punch
        if master_compression:
            processed = self._apply_multiband_compression(processed)
            print("🎚️ Compression multibande appliquée au master")

        # Maximiseur (limiteur avancé)
        if master_limiting:
            processed = self._apply_maximizer(processed, threshold=-1.0, ceiling=0.99)
            print("🔊 Maximiseur appliqué au master")

        # Appliquer le volume master (après compression)
        processed = processed * master_volume

        # Protection finale contre la saturation
        max_val = np.max(np.abs(processed))
        if max_val > 0.99:
            processed = processed * (0.99 / max_val)
            print(
                f"⚠️ Limiteur de protection activé (gain réduit de {20*np.log10(0.99/max_val):.1f} dB)"
            )

        return processed

    def _apply_multiband_compression(self, audio, bands=3):
        """
        Compression multibande pour un son plus dense

        Args:
            audio (np.array): Audio à traiter
            bands (int): Nombre de bandes de fréquence
        """
        import scipy.signal as signal

        # Paramètres pour chaque bande
        band_settings = [
            # Graves: compression douce mais profonde
            {
                "cutoff": 150,
                "threshold": 0.2,
                "ratio": 3.0,
                "attack": 0.01,
                "release": 0.2,
                "makeup": 1.3,
            },
            # Médiums: peu de compression pour préserver la clarté
            {
                "cutoff": 2000,
                "threshold": 0.3,
                "ratio": 2.0,
                "attack": 0.005,
                "release": 0.05,
                "makeup": 1.1,
            },
            # Aigus: compression forte mais rapide
            {
                "cutoff": 20000,
                "threshold": 0.15,
                "ratio": 4.0,
                "attack": 0.001,
                "release": 0.03,
                "makeup": 1.4,
            },
        ]

        sr = self.sample_rate
        output = np.zeros_like(audio)

        # Traiter chaque bande
        for i in range(bands):
            # Créer un filtre passe-bande pour la bande actuelle
            if i == 0:
                # Graves (passe-bas)
                b, a = signal.butter(
                    2, band_settings[i]["cutoff"] / (sr / 2), btype="lowpass"
                )
            elif i == bands - 1:
                # Aigus (passe-haut)
                b, a = signal.butter(
                    2, band_settings[i - 1]["cutoff"] / (sr / 2), btype="highpass"
                )
            else:
                # Médiums (passe-bande)
                b1, a1 = signal.butter(
                    2, band_settings[i - 1]["cutoff"] / (sr / 2), btype="highpass"
                )
                b2, a2 = signal.butter(
                    2, band_settings[i]["cutoff"] / (sr / 2), btype="lowpass"
                )
                # Appliquer deux filtres en série
                band = signal.filtfilt(b1, a1, audio)
                band = signal.filtfilt(b2, a2, band)
                # Ajouter à la sortie
                output += self._apply_compression(
                    band,
                    threshold=band_settings[i]["threshold"],
                    ratio=band_settings[i]["ratio"],
                    attack=band_settings[i]["attack"],
                    release=band_settings[i]["release"],
                    makeup=band_settings[i]["makeup"],
                )
                continue

            # Filtrer pour isoler la bande
            band = signal.filtfilt(b, a, audio)

            # Appliquer la compression spécifique à cette bande
            processed_band = self._apply_compression(
                band,
                threshold=band_settings[i]["threshold"],
                ratio=band_settings[i]["ratio"],
                attack=band_settings[i]["attack"],
                release=band_settings[i]["release"],
                makeup=band_settings[i]["makeup"],
            )

            # Ajouter à la sortie
            output += processed_band

        return output

    def _apply_compression(
        self, audio, threshold=0.3, ratio=2.0, attack=0.01, release=0.1, makeup=1.0
    ):
        """
        Compresseur de base

        Args:
            audio (np.array): Audio à compresser
            threshold (float): Seuil de compression (0.0-1.0)
            ratio (float): Ratio de compression
            attack (float): Temps d'attaque en secondes
            release (float): Temps de relâchement en secondes
            makeup (float): Gain de compensation
        """
        # Calculer l'enveloppe du signal (valeur absolue)
        abs_signal = np.abs(audio)

        # Enveloppe avec attack/release
        envelope = np.zeros_like(abs_signal)

        # Convertir attack/release en échantillons
        attack_samples = int(attack * self.sample_rate)
        release_samples = int(release * self.sample_rate)

        # Coefficients pour l'attaque et le relâchement
        attack_coef = np.exp(-1.0 / attack_samples)
        release_coef = np.exp(-1.0 / release_samples)

        # Calculer l'enveloppe avec attaque/relâchement
        for i in range(1, len(abs_signal)):
            # Si le signal monte, utiliser l'attaque
            if abs_signal[i] > envelope[i - 1]:
                envelope[i] = (
                    attack_coef * envelope[i - 1] + (1.0 - attack_coef) * abs_signal[i]
                )
            # Si le signal descend, utiliser le relâchement
            else:
                envelope[i] = (
                    release_coef * envelope[i - 1]
                    + (1.0 - release_coef) * abs_signal[i]
                )

        # Calculer le gain de réduction
        gain = np.ones_like(envelope)
        mask = envelope > threshold

        if np.any(mask):
            above_threshold = envelope[mask] - threshold
            reduction = above_threshold * (1.0 - 1.0 / ratio)
            gain_reduction = 1.0 - reduction / envelope[mask]
            gain[mask] = gain_reduction

        # Appliquer le gain et le makeup
        compressed = audio * gain * makeup

        return compressed

    def _apply_maximizer(self, audio, threshold=-1.0, ceiling=0.99, release=0.01):
        """
        Maximiseur (limiteur avancé avec relâchement doux)

        Args:
            audio (np.array): Audio à traiter
            threshold (float): Seuil en dB (-inf à 0)
            ceiling (float): Niveau maximum de sortie (0.0-1.0)
            release (float): Temps de relâchement en secondes
        """
        # Convertir threshold de dB à linéaire
        threshold_lin = 10 ** (threshold / 20.0)

        # Calculer l'enveloppe du signal
        abs_signal = np.abs(audio)

        # Échantillons de relâchement
        release_samples = int(release * self.sample_rate)
        release_coef = np.exp(-1.0 / release_samples)

        # Calculer l'enveloppe avec look-ahead (pour éviter distorsion)
        look_ahead = int(0.001 * self.sample_rate)  # 1ms de look-ahead
        padded_signal = np.pad(abs_signal, (0, look_ahead), mode="edge")
        max_env = np.zeros_like(audio)

        for i in range(len(audio)):
            # Prendre le max sur la fenêtre look-ahead
            max_env[i] = np.max(padded_signal[i : i + look_ahead])

        # Calculer le gain de réduction avec relâchement
        gain = np.ones_like(max_env)
        mask = max_env > threshold_lin

        if np.any(mask):
            gain[mask] = threshold_lin / max_env[mask]

        # Appliquer un relâchement doux au gain
        smoothed_gain = np.zeros_like(gain)
        smoothed_gain[0] = gain[0]

        for i in range(1, len(gain)):
            # Uniquement pour les réductions de gain
            if gain[i] < smoothed_gain[i - 1]:
                smoothed_gain[i] = gain[i]
            else:
                smoothed_gain[i] = (
                    release_coef * smoothed_gain[i - 1] + (1.0 - release_coef) * gain[i]
                )

        # Appliquer le gain et ajuster au ceiling
        limited = audio * smoothed_gain

        # Mise à l'échelle finale pour respecter le ceiling
        max_output = np.max(np.abs(limited))
        if max_output > 0:
            scale = ceiling / max_output
            limited = limited * scale

        return limited
