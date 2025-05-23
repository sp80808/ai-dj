from typing import Optional
import threading
import time
import os
import numpy as np
import pygame
from config.config import BEATS_PER_BAR


class AudioLayer:
    """Représente un layer audio individuel dans le mix avec synchronisation de grille."""

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel: pygame.mixer.Channel,
        volume: float = 0.9,
        pan: float = 0.0,
        measures: int = 4,
    ):
        self.layer_id = layer_id
        self.file_path = file_path  # Chemin vers le sample bouclé et traité
        self.sound_object: Optional[pygame.mixer.Sound] = None
        self.channel = channel
        self.master_volume = 0.8
        self.volume = volume * self.master_volume
        self.pan = pan  # -1.0 (gauche) à 1.0 (droite), 0.0 (centre)
        self.is_playing = False
        self.length_seconds = 0.0
        self.measures = measures
        # Variables pour la synchronisation de grille
        self.grid_start_time = None
        self.tempo = 126.0
        self.should_stop = False
        self.playback_thread = None

        try:
            self.sound_object = pygame.mixer.Sound(self.file_path)
            self.length_seconds = self.sound_object.get_length()
            self.channel.set_volume(self.volume)
            # La gestion du pan avec set_volume((L, R)) est plus flexible que set_pan
            left_vol = self.volume * (1.0 - max(0, self.pan))
            right_vol = self.volume * (1.0 + min(0, self.pan))
            self.channel.set_volume(left_vol, right_vol)

        except pygame.error as e:
            print(
                f"Erreur Pygame lors de la création du Layer {self.layer_id} avec {self.file_path}: {e}"
            )
            self.sound_object = None  # Indiquer que le son n'a pas pu être chargé

    def play(self, loops=-1, grid_start_time=None, tempo=126.0):
        """Joue le sample sur son canal, synchronisé avec la grille si spécifié."""
        if self.sound_object and not self.is_playing:
            if grid_start_time is not None:
                # Mode synchronisé avec la grille
                self.grid_start_time = grid_start_time
                self.tempo = tempo
                self.should_stop = False
                self.is_playing = True

                # Démarrer le thread de lecture synchronisée
                self.playback_thread = threading.Thread(
                    target=self._synced_playback_loop, daemon=True
                )
                self.playback_thread.start()
                print(f"\n▶️  Layer '{self.layer_id}' démarré en mode synchronisé.")
            else:
                # Mode classique (boucle libre)
                try:
                    self.channel.play(self.sound_object, loops=loops)
                    self.is_playing = True
                    print(
                        f"\n▶️  Layer '{self.layer_id}' démarré sur le canal {self.channel}."
                    )
                except pygame.error as e:
                    print(
                        f"Erreur Pygame lors de la lecture du layer {self.layer_id}: {e}"
                    )
        elif not self.sound_object:
            print(
                f"Impossible de jouer le layer '{self.layer_id}', sound_object non chargé."
            )
        elif self.is_playing:
            print(f"Layer '{self.layer_id}' est déjà en cours de lecture.")

    def _synced_playback_loop(self):
        """Boucle de lecture qui respecte la grille rythmique."""
        # Calculer les durées
        seconds_per_beat = 60.0 / self.tempo
        measure_duration = (
            seconds_per_beat * BEATS_PER_BAR
        )  # Durée d'UNE mesure (4 bars)
        sample_duration = self.sound_object.get_length()

        print(
            f"🎼 Layer '{self.layer_id}': Mesure={measure_duration:.3f}s, Sample={sample_duration:.3f}s"
        )

        # Attendre le prochain début de mesure depuis le grid_start_time
        current_time = time.time()
        elapsed_since_grid_start = current_time - self.grid_start_time

        # Calculer combien de mesures complètes se sont écoulées
        measures_elapsed = elapsed_since_grid_start / measure_duration
        next_measure_start = (
            self.grid_start_time + (int(measures_elapsed) + 1) * measure_duration
        )

        # Si on est très proche du début d'une mesure (moins de 50ms), partir maintenant
        if (next_measure_start - current_time) < 0.05:
            next_measure_start += measure_duration

        wait_time = next_measure_start - current_time
        if wait_time > 0:
            print(f"⏱️  Attente de {wait_time:.3f}s pour synchronisation...")
            time.sleep(wait_time)

        measure_count = 0

        while not self.should_stop:
            if self.should_stop:
                break

            measure_start_time = time.time()
            measure_count += 1

            # Jouer le sample au début de chaque mesure
            try:
                self.channel.play(self.sound_object, loops=0)
                print(f"🎵 Sample '{self.layer_id}' démarré sur mesure {measure_count}")

                # Attendre soit la fin du sample, soit la fin de la mesure
                sleep_increment = 0.01  # 10ms
                total_slept = 0

                while total_slept < sample_duration and not self.should_stop:
                    current_time = time.time()
                    elapsed_in_measure = current_time - measure_start_time

                    # Si on atteint la fin de la mesure, arrêter le sample
                    if elapsed_in_measure >= measure_duration:
                        self.channel.stop()
                        print(
                            f"🔪 Sample '{self.layer_id}' coupé (fin de mesure {measure_count})"
                        )
                        break

                    time.sleep(sleep_increment)
                    total_slept += sleep_increment

            except pygame.error as e:
                print(f"Erreur lecture layer {self.layer_id}: {e}")
                break

            # Attendre le début de la prochaine mesure
            current_time = time.time()
            elapsed_in_measure = current_time - measure_start_time
            remaining_time = measure_duration - elapsed_in_measure

            if remaining_time > 0:
                time.sleep(remaining_time)

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """
        Arrête le sample sur son canal, avec un fadeout optionnel.

        Args:
            fadeout_ms: Durée du fadeout en millisecondes
            cleanup: Si True, nettoie le fichier audio du disque après l'arrêt
        """
        if self.is_playing:
            # Arrêter le thread synchronisé si il existe
            self.should_stop = True

            # Arrêter le playback pygame
            if fadeout_ms > 0 and self.sound_object:
                self.channel.fadeout(fadeout_ms)
            else:
                self.channel.stop()

            # Attendre que le thread se termine si il existe
            if self.playback_thread and self.playback_thread.is_alive():
                self.playback_thread.join(timeout=1.0)

            self.is_playing = False
            print(f"⏹️  Layer '{self.layer_id}' arrêté.")

            # Nettoyer les ressources
            if cleanup:
                # Attendre que le fadeout soit terminé avant de nettoyer
                if fadeout_ms > 0:
                    time.sleep(fadeout_ms / 1000.0)

                # Libérer le son de la mémoire
                if self.sound_object:
                    # Supprimer la référence à l'objet son pour que le GC puisse le collecter
                    self.sound_object = None

                # Si c'est un fichier temporaire, le supprimer du disque
                if self.file_path and os.path.exists(self.file_path):
                    # Vérifier si c'est un fichier temporaire
                    is_temp_file = any(
                        marker in self.file_path
                        for marker in ["_loop_", "_fx_", "temp_", "_orig_"]
                    )

                    if is_temp_file:
                        try:
                            os.remove(self.file_path)
                            print(
                                f"🗑️  Fichier audio temporaire supprimé: {self.file_path}"
                            )
                        except (PermissionError, OSError) as e:
                            # Ignorer l'erreur si le fichier est encore utilisé
                            print(
                                f"Impossible de supprimer le fichier {self.file_path}: {e}"
                            )

    def set_volume(self, volume: float):
        """Ajuste le volume du layer."""
        self.volume = np.clip(volume, 0.0, 1.0)
        if self.channel and self.sound_object:
            # Recalculer L/R avec le nouveau volume et le pan existant
            left_vol = self.volume * (1.0 - max(0, self.pan))
            right_vol = self.volume * (1.0 + min(0, self.pan))
            self.channel.set_volume(left_vol, right_vol)

    def set_pan(self, pan: float):
        """Ajuste le panoramique du layer."""
        self.pan = np.clip(pan, -1.0, 1.0)
        if self.channel and self.sound_object:
            left_vol = self.volume * (1.0 - max(0, self.pan))  # Si pan > 0, L diminue
            right_vol = self.volume * (1.0 + min(0, self.pan))  # Si pan < 0, R diminue
            self.channel.set_volume(left_vol, right_vol)
