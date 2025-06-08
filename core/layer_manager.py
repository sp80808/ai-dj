import os
import librosa
import numpy as np
import soundfile as sf
from typing import Optional


class LayerManager:
    def __init__(
        self,
        output_dir,
        sample_rate: int = 48000,
    ):
        self.sample_rate = sample_rate
        self.output_dir = output_dir
        self.master_tempo: float = 126.0

    def applicate_lite_fade_in_fade_out(self, audio, layer_id, sr):
        fade_ms = 5
        fade_samples = int(sr * (fade_ms / 1000.0))
        if len(audio) > 2 * fade_samples:
            end_part = audio[-fade_samples:]
            start_part = audio[:fade_samples]
            fade_out_ramp = np.linspace(1.0, 0.0, fade_samples)
            fade_in_ramp = np.linspace(0.0, 1.0, fade_samples)
            audio[:fade_samples] = start_part * fade_in_ramp + end_part * fade_out_ramp
            audio[-fade_samples:] = end_part * fade_out_ramp
        else:
            print(f"ℹ️  Layer '{layer_id}' too short for {fade_ms}ms crossfade.")
        return audio

    def _prepare_sample_for_loop(
        self,
        original_audio_path: str,
        layer_id: str,
    ) -> Optional[str]:
        try:
            audio, sr_orig = librosa.load(original_audio_path, sr=None)
            if sr_orig != self.sample_rate:
                audio = librosa.resample(
                    audio, orig_sr=sr_orig, target_sr=self.sample_rate
                )
            sr = self.sample_rate
        except Exception as e:
            print(f"❌ Error loading sample {original_audio_path} with librosa: {e}")
            return None
        audio = self.applicate_lite_fade_in_fade_out(
            audio=audio, layer_id=layer_id, sr=sr
        )
        looped_sample_filename = f"{os.path.splitext(os.path.basename(original_audio_path))[0]}_loop_{layer_id}.wav"
        temp_path = os.path.join(self.output_dir, "temp_" + looped_sample_filename)
        looped_sample_path = os.path.join(self.output_dir, looped_sample_filename)
        sf.write(temp_path, audio, sr)

        try:
            if isinstance(audio, np.ndarray):
                sf.write(looped_sample_path, audio, sr)
                print(f"⏩ Looped sample: {looped_sample_path}")
            else:
                sf.write(looped_sample_path, audio, sr)
                print(
                    f"💾 Layer '{layer_id}': Looped sample saved: {looped_sample_path}"
                )
            if os.path.exists(temp_path):
                os.remove(temp_path)

            return looped_sample_path
        except Exception as e:
            print(f"❌ Error saving looped sample for {layer_id}: {e}")
            return None
