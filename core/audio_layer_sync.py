import time
import os
from typing import Optional
import numpy as np
import pygame
from core.midi_clock_manager import MidiClockManager


class AudioLayerSync:
    """AudioLayer synchronisé avec MIDI Clock"""

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel: pygame.mixer.Channel,
        midi_manager: MidiClockManager,
        volume: float = 0.9,
        pan: float = 0.0,
        measures: int = 1,
    ):
        self.layer_id = layer_id
        self.file_path = file_path
        self.sound_object: Optional[pygame.mixer.Sound] = None
        self.channel = channel
        self.midi_manager = midi_manager
        self.master_volume = 0.8
        self.volume = volume * self.master_volume
        self.pan = pan
        self.measures = measures

        # État de lecture
        self.is_armed = False  # Prêt à jouer au prochain beat 1
        self.is_playing = False
        self.last_played_measure = 0

        try:
            self.sound_object = pygame.mixer.Sound(self.file_path)
            self.length_seconds = self.sound_object.get_length()
            self.channel.set_volume(self.volume)
            self._apply_pan()

            print(f"🎵 Layer '{self.layer_id}' chargé ({self.length_seconds:.2f}s)")

        except pygame.error as e:
            print(f"❌ Erreur chargement {self.layer_id}: {e}")
            self.sound_object = None

    def _apply_pan(self):
        """Applique le panoramique"""
        if self.channel and self.sound_object:
            left_vol = self.volume * (1.0 - max(0, self.pan))
            right_vol = self.volume * (1.0 + min(0, self.pan))
            self.channel.set_volume(left_vol, right_vol)

    def play(self):
        """Arme le layer - il se déclenchera au prochain beat 1"""
        if not self.sound_object:
            print(f"❌ Impossible d'armer {self.layer_id} - fichier non chargé")
            return

        self.is_armed = True
        self.midi_manager.add_listener(self)
        print(f"🎼 Layer '{self.layer_id}' armé - attente du prochain beat 1...")

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """Désarme et arrête le layer"""
        self.is_armed = False
        self.midi_manager.remove_listener(self)

        if fadeout_ms > 0:
            self.channel.fadeout(fadeout_ms)
        else:
            self.channel.stop()

        self.is_playing = False
        print(f"⏹️  Layer '{self.layer_id}' arrêté")

        if cleanup and self.file_path and os.path.exists(self.file_path):
            # Nettoyer les fichiers temporaires
            is_temp_file = any(
                marker in self.file_path
                for marker in ["_loop_", "_fx_", "temp_", "_orig_"]
            )

            if is_temp_file:
                try:
                    if fadeout_ms > 0:
                        time.sleep(fadeout_ms / 1000.0)
                    os.remove(self.file_path)
                    print(f"🗑️  Fichier temporaire supprimé: {self.file_path}")
                except (PermissionError, OSError) as e:
                    print(f"Impossible de supprimer {self.file_path}: {e}")

    def on_midi_event(self, event_type: str, measure: int = None):
        """Callback des events MIDI Clock"""

        if event_type == "measure_start" and self.is_armed:
            # Jouer seulement si on n'a pas déjà joué sur cette mesure
            # ET si on est dans le bon cycle
            if measure != self.last_played_measure:
                # Vérifier si c'est le bon moment selon self.measures
                if (
                    measure - 1
                ) % self.measures == 0:  # Beat 1, 5, 9, 13... pour measures=4
                    try:
                        self.channel.play(self.sound_object, loops=0)
                        self.is_playing = True
                        self.last_played_measure = measure
                    except pygame.error as e:
                        print(f"❌ Erreur lecture {self.layer_id}: {e}")

        elif event_type == "stop":
            # Arrêter si Bitwig s'arrête
            self.channel.stop()
            self.is_playing = False
            self.last_played_measure = 0

    def set_volume(self, volume: float):
        """Ajuste le volume"""
        self.volume = np.clip(volume, 0.0, 1.0) * self.master_volume
        self._apply_pan()

    def set_pan(self, pan: float):
        """Ajuste le panoramique"""
        self.pan = np.clip(pan, -1.0, 1.0)
        self._apply_pan()
