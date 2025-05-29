import os
import tempfile
import subprocess
import traceback
import librosa
import numpy as np
from scipy import signal
import soundfile as sf
from typing import Optional
from config.config import BEATS_PER_BAR


class LayerManager:
    def __init__(
        self,
        output_dir,
        sample_rate: int = 48000,
    ):
        self.sample_rate = sample_rate
        self.output_dir = output_dir
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

        self.master_tempo: float = 126.0

    def find_kick_attack_start(self, audio, sr, onset_position, layer_id):
        search_window = int(sr * 0.15)
        search_start = max(0, onset_position - search_window)
        search_end = min(len(audio), onset_position + int(sr * 0.05))
        search_segment = audio[search_start:search_end]

        if len(search_segment) < 100:
            return max(0, onset_position - int(sr * 0.02))

        hop_length = 64
        rms = librosa.feature.rms(y=search_segment, hop_length=hop_length)[0]

        max_energy = np.max(rms)
        baseline_energy = np.mean(rms[: len(rms) // 4])
        threshold = baseline_energy + (max_energy - baseline_energy) * 0.1

        attack_candidates = np.where(rms > threshold)[0]

        if len(attack_candidates) > 0:
            attack_start_frame = attack_candidates[0]

            relative_sample = attack_start_frame * hop_length
            absolute_sample = search_start + relative_sample

            kick_duration_margin = int(sr * 0.3)
            final_start = max(0, absolute_sample - kick_duration_margin)

            print(
                f"🎯 Kick attack detected at {absolute_sample/sr:.3f}s, starting for full kick at {final_start/sr:.3f}s ('{layer_id}')"
            )
            return final_start

        conservative_start = max(0, onset_position - int(sr * 0.3))
        print(
            f"🤔 Kick attack not detected, conservative start at {conservative_start/sr:.3f}s ('{layer_id}')"
        )
        return conservative_start

    def trim_audio(self, sr, measures, layer_id, audio):
        seconds_per_beat = 60.0 / self.master_tempo
        samples_per_beat = int(seconds_per_beat * sr)
        samples_per_bar = samples_per_beat * BEATS_PER_BAR
        target_total_samples = samples_per_bar * measures
        target_total_samples = int(target_total_samples * 1.2)

        onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
        onsets_samples = librosa.onset.onset_detect(
            onset_envelope=onset_env, sr=sr, units="samples", backtrack=False
        )
        start_offset_samples = 0
        if len(onsets_samples) > 0:
            early_onsets = [o for o in onsets_samples if o < sr * 0.2]
            if early_onsets:
                detected_onset = early_onsets[0]
            else:
                detected_onset = onsets_samples[0]
            start_offset_samples = self.find_kick_attack_start(
                audio, sr, detected_onset, layer_id
            )
            if start_offset_samples < sr * 0.01:
                print(f"✅ Immediate kick detected ('{layer_id}'), no trim required")
                start_offset_samples = 0

        else:
            print(f"⚠️ No onset detected for '{layer_id}', starting without trim")
            start_offset_samples = 0

        if start_offset_samples > 0:
            print(
                f"✂️ Trim applied: {start_offset_samples/sr:.3f}s removed ('{layer_id}')"
            )
            audio = audio[start_offset_samples:]
        else:
            print(f"🎵 No trim needed for '{layer_id}'")

        current_length = len(audio)
        if current_length == 0:
            print(f"❌ Error: Layer '{layer_id}' empty after trim.")

        current_length = len(audio)
        if current_length == 0:
            print(f"❌ Error: Layer '{layer_id}' empty after trim.")
            return None

        if current_length > target_total_samples:
            fade_samples = int(sr * 0.1)
            audio[
                target_total_samples - fade_samples : target_total_samples
            ] *= np.linspace(1.0, 0.0, fade_samples)
            audio = audio[:target_total_samples]
        elif current_length < target_total_samples:
            num_repeats = int(np.ceil(target_total_samples / current_length))
            audio = np.tile(audio, num_repeats)[:target_total_samples]
        return audio

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
            print(f"Layer '{layer_id}' too short for {fade_ms}ms crossfade.")
        return audio

    def _prepare_sample_for_loop(
        self,
        original_audio_path: str,
        layer_id: str,
        measures: int,
        server_side_pre_treatment=False,
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
        if server_side_pre_treatment:
            audio = self.trim_audio(
                sr=sr, measures=measures, layer_id=layer_id, audio=audio
            )
        audio = self.applicate_lite_fade_in_fade_out(
            audio=audio, layer_id=layer_id, sr=sr
        )
        looped_sample_filename = f"{os.path.splitext(os.path.basename(original_audio_path))[0]}_loop_{layer_id}.wav"
        temp_path = os.path.join(self.output_dir, "temp_" + looped_sample_filename)
        looped_sample_path = os.path.join(self.output_dir, looped_sample_filename)
        sf.write(temp_path, audio, sr)

        try:
            if not server_side_pre_treatment:
                stretched_audio = audio
            else:
                stretched_audio = self.match_sample_to_tempo(
                    temp_path,
                    target_tempo=self.master_tempo,
                    sr=self.sample_rate,
                    preserve_measures=False,
                )

            if isinstance(stretched_audio, np.ndarray):
                sf.write(looped_sample_path, stretched_audio, sr)
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

    def set_master_tempo(self, new_tempo: float):
        if new_tempo > 50 and new_tempo < 300:
            print(
                f"Changing master tempo from {self.master_tempo} BPM to {new_tempo} BPM."
            )
            self.master_tempo = new_tempo
        else:
            print(f"❌ Invalid tempo: {new_tempo}. Must be between 50 and 300 BPM.")

    def match_sample_to_tempo(
        self, audio, target_tempo, sr, preserve_measures=True, beats_per_measure=4
    ):
        try:
            if isinstance(audio, str):
                print(f"📂 Loading audio file: {audio}")
                try:
                    audio, file_sr = sf.read(audio, always_2d=False)
                    if file_sr != sr:
                        audio = librosa.resample(audio, orig_sr=file_sr, target_sr=sr)
                    print(f"✅ Audio file loaded: {audio.shape}, sr={file_sr}")
                except Exception as e:
                    print(f"❌ Failed to load file: {e}")
                    return audio

            if not isinstance(audio, np.ndarray):
                print(
                    f"⚠️ Converting audio to numpy array (current type: {type(audio)})"
                )
                try:
                    audio = np.array(audio, dtype=np.float32)
                except Exception as e:
                    print(f"❌ Conversion failed: {e}")
                    return audio

            if audio.size == 0:
                print("❌ Audio is empty!")
                return audio

            print(f"ℹ️  Audio shape: {audio.shape}, dtype: {audio.dtype}")

            original_length = len(audio)
            original_duration = original_length / sr

            onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
            estimated_tempo = librosa.beat.tempo(onset_envelope=onset_env, sr=sr)[0]
            print(f"🎵 Estimated sample tempo: {estimated_tempo:.1f} BPM")
            if estimated_tempo < 40 or estimated_tempo > 220:
                print(
                    f"⚠️ Implausible estimated tempo ({estimated_tempo:.1f} BPM), using default value"
                )
                estimated_tempo = 120

            tempo_ratio = abs(estimated_tempo - target_tempo) / target_tempo
            if tempo_ratio < 0.02:
                print(
                    f"ℹ️ Similar tempos ({estimated_tempo:.1f} vs {target_tempo:.1f} BPM), no stretching"
                )
                return audio
            stretch_ratio = estimated_tempo / target_tempo
            print(f"🔧 Using Rubber Band for time stretch...")

            try:
                stretched_audio = self._time_stretch_rubberband(
                    audio, stretch_ratio, sr
                )
                print(f"✅ Rubber Band time stretch successful")
            except Exception as rb_error:
                print(f"⚠️ Rubber Band Error: {rb_error}")
                print("🔄 Fallback to librosa time stretch...")
                stretched_audio = librosa.effects.time_stretch(
                    audio, rate=stretch_ratio
                )
            stretched_length = len(stretched_audio)
            if preserve_measures:
                beats_in_original = (original_duration / 60.0) * estimated_tempo
                measures_in_original = beats_in_original / beats_per_measure
                whole_measures = max(1, round(measures_in_original))
                print(
                    f"📏 Estimated number of measurements: {measures_in_original:.2f} → {whole_measures}"
                )
                target_beats = whole_measures * beats_per_measure
                target_duration = (target_beats / target_tempo) * 60.0
                target_duration *= 1.2
                target_length = int(target_duration * sr)
                if abs(target_length - stretched_length) > sr * 0.1:
                    print(
                        f"✂️ Adjusting to an exact number of measures: {target_duration:.2f}s ({whole_measures} measures)"
                    )
                    try:
                        stretched_audio = signal.resample(
                            stretched_audio, target_length
                        )
                        print("🔬 High quality interpolation with scipy")
                    except ImportError:
                        x_original = np.linspace(0, 1, stretched_length)
                        x_target = np.linspace(0, 1, target_length)
                        stretched_audio = np.interp(
                            x_target, x_original, stretched_audio
                        )
                        print("📐 Standard interpolation with numpy")
            print(
                f"⏩ Time stretching applied: {estimated_tempo:.1f} → {target_tempo:.1f} BPM (ratio: {stretch_ratio:.2f})"
            )
            final_duration = len(stretched_audio) / sr
            print(f"⏱️ Duration: {original_duration:.2f}s → {final_duration:.2f}s")
            return stretched_audio

        except Exception as e:
            print(f"⚠️ Error while stretching time: {e}")
            traceback.print_exc()
            print("⚠️ Returning the original audio without modification")
            return audio

    def _time_stretch_rubberband(self, audio, stretch_ratio, sr):
        temp_files = []

        try:
            temp_in = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
            temp_out = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
            temp_files = [temp_in.name, temp_out.name]
            sf.write(temp_in.name, audio, sr)
            temp_in.close()
            cmd = [
                "rubberband",
                "-t",
                str(stretch_ratio),
                temp_in.name,
                temp_out.name,
            ]

            print(f"🎛️ Rubber Band command: {' '.join(cmd)}")
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
            )

            if result.returncode != 0:
                raise Exception(
                    f"Rubber Band failed with code {result.returncode}: {result.stderr}"
                )

            temp_out.close()
            stretched_audio, _ = sf.read(temp_out.name, always_2d=False)
            print(f"🎯 Rubber Band: {len(audio)} → {len(stretched_audio)} samples")
            return stretched_audio

        except subprocess.TimeoutExpired:
            raise Exception("Rubber Band timeout (>30s)")
        except FileNotFoundError:
            raise Exception(
                "Rubber Band not found. Install it: apt install rubberband-cli"
            )
        except Exception as e:
            raise Exception(f"Rubber Band Error: {str(e)}")
        finally:
            for temp_file in temp_files:
                try:
                    if os.path.exists(temp_file):
                        os.unlink(temp_file)
                except Exception as cleanup_error:
                    print(f"⚠️ Error cleaning file {temp_file}: {cleanup_error}")
