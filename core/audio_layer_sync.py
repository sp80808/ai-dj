import sounddevice as sd
import soundfile as sf
import threading
import os
import numpy as np


class AudioLayerSync:
    """AudioLayer avec sounddevice - Plus stable que pygame"""

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel_id: int,  # Juste un ID, pas d'objet pygame
        midi_manager,
        volume: float = 0.9,
        pan: float = 0.0,
        measures: int = 1,
    ):
        self.layer_id = layer_id
        self.file_path = file_path
        self.channel_id = channel_id
        self.midi_manager = midi_manager
        self.master_volume = 0.8
        self.volume = volume * self.master_volume
        self.pan = pan
        self.measures = measures

        # Audio data
        self.audio_data = None
        self.sample_rate = None
        self.length_seconds = 0

        # État de lecture
        self.is_armed = False
        self.is_playing = False
        self.playback_thread = None
        self.stop_event = threading.Event()
        # Charger l'audio
        try:
            self.audio_data, self.sample_rate = sf.read(self.file_path, always_2d=True)
            self.length_seconds = len(self.audio_data) / self.sample_rate
            
            # Convertir en stéréo si nécessaire
            if self.audio_data.shape[1] == 1:
                self.audio_data = np.tile(self.audio_data, (1, 2))
            
            print(f"🎵 Layer '{self.layer_id}' chargé ({self.length_seconds:.2f}s)")

        except Exception as e:
            print(f"❌ Erreur chargement {self.layer_id}: {e}")
            self.audio_data = None
        self.sound_object = self.audio_data 
        
    def _apply_volume_and_pan(self, audio):
        """Applique volume et panoramique"""
        if audio is None:
            return None
            
        # Copier pour ne pas modifier l'original
        processed = audio.copy()
        
        # Appliquer le pan
        if self.pan != 0:
            if self.pan > 0:  # Pan vers la droite
                processed[:, 0] *= (1.0 - self.pan)  # Réduire le canal gauche
            else:  # Pan vers la gauche
                processed[:, 1] *= (1.0 + self.pan)  # Réduire le canal droit
        
        # Appliquer le volume
        processed *= self.volume
        
        return processed

    def _play_sample(self):
        """Joue le sample une fois"""
        if self.audio_data is None:
            return
            
        try:
            # Appliquer volume et pan
            audio_to_play = self._apply_volume_and_pan(self.audio_data)
            
            # Jouer avec sounddevice
            sd.play(audio_to_play, samplerate=self.sample_rate)
            
            # Marquer comme en cours de lecture
            self.is_playing = True
            
            # Attendre la fin ou le stop
            sd.wait()  # Attend que la lecture se termine
            
        except Exception as e:
            print(f"❌ Erreur lecture {self.layer_id}: {e}")
        finally:
            self.is_playing = False

    def play(self):
        """Arme le layer"""
        if self.audio_data is None:
            print(f"❌ Impossible d'armer {self.layer_id} - fichier non chargé")
            return

        self.is_armed = True
        self.midi_manager.add_listener(self)
        print(f"🎼 Layer '{self.layer_id}' armé - attente du prochain beat 1...")

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """Arrête le layer"""
        self.is_armed = False
        self.midi_manager.remove_listener(self)

        # Arrêter sounddevice
        if self.is_playing:
            sd.stop()  # Arrêt immédiat, sounddevice gère ça proprement
        
        self.is_playing = False
        print(f"⏹️  Layer '{self.layer_id}' arrêté")

        # Cleanup fichiers temporaires
        if cleanup and self.file_path and os.path.exists(self.file_path):
            is_temp_file = any(
                marker in self.file_path
                for marker in ["_loop_", "_fx_", "temp_", "_orig_"]
            )

            if is_temp_file:
                try:
                    os.remove(self.file_path)
                    print(f"🗑️  Fichier temporaire supprimé: {self.file_path}")
                except (PermissionError, OSError) as e:
                    print(f"Impossible de supprimer {self.file_path}: {e}")

    def on_midi_event(self, event_type: str, measure: int = None):
        """Callback MIDI - Version sounddevice"""
        
        if event_type == "measure_start" and self.is_armed:
            
            # Arrêter la lecture précédente si elle existe
            if self.is_playing:
                sd.stop()
            
            # Jouer le sample dans un thread séparé pour ne pas bloquer MIDI
            if self.playback_thread and self.playback_thread.is_alive():
                self.stop_event.set()
                self.playback_thread.join(timeout=0.1)
            
            self.stop_event.clear()
            self.playback_thread = threading.Thread(target=self._play_sample)
            self.playback_thread.daemon = True
            self.playback_thread.start()

        elif event_type == "stop":
            if self.is_playing:
                sd.stop()
            self.is_playing = False

    def set_volume(self, volume: float):
        """Ajuste le volume"""
        self.volume = np.clip(volume, 0.0, 1.0) * self.master_volume

    def set_pan(self, pan: float):
        """Ajuste le panoramique"""
        self.pan = np.clip(pan, -1.0, 1.0)

    def is_loaded(self):
        """Vérifie si l'audio est chargé"""
        return self.audio_data is not None