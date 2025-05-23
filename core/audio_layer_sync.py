import pyaudio
import soundfile as sf
import os
import numpy as np


class AudioLayerSync:
    """AudioLayer avec PyAudio - Alternative stable à sounddevice"""
    
    # Player PyAudio partagé
    _pyaudio_player = None
    _active_layers = {}

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel_id: int,
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
        self.sample_position = 0

        # Initialiser le player PyAudio global si pas encore fait
        if AudioLayerSync._pyaudio_player is None:
            AudioLayerSync._init_pyaudio_player()

        # Charger l'audio
        try:
            self.audio_data, self.sample_rate = sf.read(self.file_path, always_2d=True)
            self.length_seconds = len(self.audio_data) / self.sample_rate
            
            # Convertir en stéréo si nécessaire
            if self.audio_data.shape[1] == 1:
                self.audio_data = np.tile(self.audio_data, (1, 2))
            
            # Resampler si nécessaire pour correspondre au stream
            target_rate = AudioLayerSync._pyaudio_player.sample_rate
            if self.sample_rate != target_rate:
                import librosa
                self.audio_data = librosa.resample(
                    self.audio_data.T, 
                    orig_sr=self.sample_rate, 
                    target_sr=target_rate
                ).T
                self.sample_rate = target_rate
            
            print(f"🎵 Layer '{self.layer_id}' chargé ({self.length_seconds:.2f}s)")

        except Exception as e:
            print(f"❌ Erreur chargement {self.layer_id}: {e}")
            self.audio_data = None
            
        # Compatibilité avec LayerManager
        self.sound_object = True if self.audio_data is not None else None

    @classmethod
    def _init_pyaudio_player(cls):
        """Initialise le player PyAudio global"""
        try:
            cls._pyaudio_player = PyAudioMixer()
            print("🎛️  PyAudio mixer initialisé")
        except Exception as e:
            print(f"❌ Erreur init PyAudio: {e}")
            raise

    def play(self):
        """Arme le layer"""
        if self.audio_data is None:
            print(f"❌ Impossible d'armer {self.layer_id} - fichier non chargé")
            return

        self.is_armed = True
        self.midi_manager.add_listener(self)
        
        # Ajouter ce layer au mixer global
        AudioLayerSync._active_layers[self.layer_id] = self
        
        print(f"🎼 Layer '{self.layer_id}' armé - attente du prochain beat 1...")

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """Arrête le layer"""
        self.is_armed = False
        self.midi_manager.remove_listener(self)
        self.is_playing = False
        
        # Retirer du mixer global
        if self.layer_id in AudioLayerSync._active_layers:
            del AudioLayerSync._active_layers[self.layer_id]
        
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
        """Callback MIDI - Déclenchement PyAudio"""
        # Reset position pour rejouer depuis le début
        self.sample_position = 0
        if event_type == "measure_start" and self.is_armed:
            self.is_playing = True
        elif event_type == "stop":
            self.is_playing = False

    def get_audio_chunk(self, nframes):
        """Retourne un chunk audio pour le mixer"""
        if not self.is_playing or self.audio_data is None:
            return None

        # Calculer combien d'échantillons on peut retourner
        remaining_samples = len(self.audio_data) - self.sample_position
        samples_to_copy = min(nframes, remaining_samples)
        
        if samples_to_copy <= 0:
            # Fin du sample, recommencer (boucle)
            self.sample_position = 0
            samples_to_copy = min(nframes, len(self.audio_data))
        
        # Extraire le chunk
        chunk = self.audio_data[
            self.sample_position:self.sample_position + samples_to_copy
        ].copy()
        
        # Appliquer volume et pan
        chunk = self._apply_volume_and_pan(chunk)
        
        # Avancer la position
        self.sample_position += samples_to_copy
        
        # Si le chunk est plus petit que demandé, padder avec des zéros
        if samples_to_copy < nframes:
            padding = np.zeros((nframes - samples_to_copy, 2), dtype=np.float32)
            chunk = np.vstack([chunk, padding])
        
        return chunk.astype(np.float32)

    def _apply_volume_and_pan(self, audio):
        """Applique volume et panoramique"""
        if audio is None:
            return None
            
        processed = audio.copy()
        
        # Appliquer le pan
        if self.pan != 0:
            if self.pan > 0:  # Pan vers la droite
                processed[:, 0] *= (1.0 - self.pan)
            else:  # Pan vers la gauche
                processed[:, 1] *= (1.0 + self.pan)
        
        # Appliquer le volume
        processed *= self.volume
        
        return processed

    def set_volume(self, volume: float):
        """Ajuste le volume"""
        self.volume = np.clip(volume, 0.0, 1.0) * self.master_volume

    def set_pan(self, pan: float):
        """Ajuste le panoramique"""
        self.pan = np.clip(pan, -1.0, 1.0)

    def is_loaded(self):
        """Vérifie si l'audio est chargé"""
        return self.audio_data is not None


class PyAudioMixer:
    """Mixer PyAudio qui gère tous les layers"""
    
    def __init__(self, sample_rate=48000, chunk_size=8192):
        self.sample_rate = sample_rate
        self.chunk_size = chunk_size
        self.is_running = False
        
        # Initialiser PyAudio
        self.pa = pyaudio.PyAudio()
        
        # Créer le stream de sortie
        try:
            self.stream = self.pa.open(
                format=pyaudio.paFloat32,
                channels=2,
                rate=sample_rate,
                output=True,
                frames_per_buffer=chunk_size,
                stream_callback=self._audio_callback
            )
            
            self.stream.start_stream()
            self.is_running = True
            
            print(f"🎵 PyAudio mixer démarré ({sample_rate}Hz, chunk={chunk_size})")
            
        except Exception as e:
            print(f"❌ Erreur PyAudio stream: {e}")
            self.pa.terminate()
            raise
        if os.name == 'nt':  # Windows
            try:
                import ctypes
                # Priorité temps réel pour le thread audio
                ctypes.windll.kernel32.SetThreadPriority(
                    ctypes.windll.kernel32.GetCurrentThread(), 
                    15  # THREAD_PRIORITY_TIME_CRITICAL
                )
                print("🚀 Priorité thread audio élevée")
            except:
                pass

    def _audio_callback(self, in_data, frame_count, time_info, status):
        """Callback audio PyAudio - Mixe tous les layers"""
        
        # Buffer de sortie
        output_buffer = np.zeros((frame_count, 2), dtype=np.float32)
        
        # Mixer tous les layers actifs
        for layer in AudioLayerSync._active_layers.values():
            chunk = layer.get_audio_chunk(frame_count)
            if chunk is not None:
                # Additionner au mix
                output_buffer += chunk
        
        # Clipping de sécurité
        np.clip(output_buffer, -1.0, 1.0, out=output_buffer)
        
        # Convertir en bytes pour PyAudio
        audio_data = output_buffer.astype(np.float32).tobytes()
        
        return (audio_data, pyaudio.paContinue)

    def close(self):
        """Ferme proprement PyAudio"""
        if self.is_running:
            self.stream.stop_stream()
            self.stream.close()
            self.pa.terminate()
            self.is_running = False
            print("🔇 PyAudio mixer fermé")