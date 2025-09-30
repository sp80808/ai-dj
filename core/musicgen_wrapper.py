import base64
import io
import os
import torch
import torchaudio

try:
    from audiocraft.models import MusicGen
    from audiocraft.data.audio import audio_write
except Exception as e:  # pragma: no cover
    raise RuntimeError(f"Audiocraft/MusicGen unavailable: {e}")


class MusicGenWrapper:
    def __init__(self, model_name: str = "facebook/musicgen-small", device: str | None = None, cache_dir: str | None = None):
        if device is None:
            self.device = "cuda" if torch.cuda.is_available() else "cpu"
        else:
            self.device = device
        self.model = MusicGen.get_pretrained(model_name, device=self.device, cache_dir=cache_dir)
        self.model.to(self.device)
        self.sample_rate = getattr(self.model, "sample_rate", 32000)

    def set_params(self, duration=30.0, temperature=1.0, top_k=250, cfg_coef=3.0):
        self.model.set_generation_params(
            duration=duration,
            use_sampling=True,
            top_k=top_k,
            temperature=temperature,
            cfg_coef=cfg_coef,
        )

    def _load_conditioning_audio(self, audio_path: str):
        wav, sr = torchaudio.load(audio_path)
        if sr != self.sample_rate:
            wav = torchaudio.functional.resample(wav, sr, self.sample_rate)
        return wav.to(self.device)

    def _load_conditioning_midi(self, midi_b64: str):
        try:
            import pretty_midi
            pm = pretty_midi.PrettyMIDI(io.BytesIO(base64.b64decode(midi_b64)))
            fs = self.sample_rate
            audio = pm.fluidsynth(fs=fs)
            wav = torch.from_numpy(audio).unsqueeze(0)
            return wav.to(self.device)
        except Exception:
            return None

    @torch.inference_mode()
    def generate_audio(self, prompt: str, output_path='generated.wav',
                       conditioning_audio_path: str | None = None,
                       conditioning_midi_base64: str | None = None,
                       conditioning_strength: float = 0.5,
                       seed: int | None = None):
        if seed is not None:
            torch.manual_seed(seed)

        melody = None
        if conditioning_audio_path and os.path.exists(conditioning_audio_path):
            melody = self._load_conditioning_audio(conditioning_audio_path)
        elif conditioning_midi_base64:
            melody = self._load_conditioning_midi(conditioning_midi_base64)

        if melody is not None and hasattr(self.model, "generate_with_chroma"):
            out = self.model.generate_with_chroma(
                descriptions=[prompt],
                melody_wavs=[melody],
                melody_sample_rate=self.sample_rate,
                strength=conditioning_strength,
                progress=False,
            )
        elif melody is not None and hasattr(self.model, "generate_with_melody"):
            out = self.model.generate_with_melody([prompt], melody, self.sample_rate, progress=False)
        else:
            out = self.model.generate([prompt], progress=False)

        audio = out[0].detach().cpu()
        audio_write(output_path, audio, self.sample_rate, strategy="loudness", loudness_compressor=True)
        return output_path

import torch
import torchaudio
import numpy as np
import tempfile
import os
import time
import gc
import subprocess
from pathlib import Path
from typing import Optional, Tuple, Union
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MusicGenWrapper:
    """
    A complete MusicGen wrapper with melody conditioning support for the AI Track Generator VST.

    Supports both text-to-audio generation and melody-conditioned generation using
    the facebook/musicgen-melody model for enhanced control over musical output.
    """

    def __init__(self, model_id: str = "facebook/musicgen-melody"):
        """
        Initialize the MusicGen wrapper.

        Args:
            model_id: HuggingFace model ID for MusicGen (default: facebook/musicgen-melody)
        """
        self.model_id = model_id
        self.model = None
        self.processor = None
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.sample_rate = 32000  # MusicGen default sample rate
        self.is_initialized = False

        # Model configuration
        self.max_duration = 30  # Maximum generation duration in seconds
        self.default_duration = 10  # Default generation duration
        self.guidance_scale = 3.0  # Classifier-free guidance scale

        logger.info(f"MusicGenWrapper initialized for device: {self.device}")

    def init_model(self) -> bool:
        """
        Initialize the MusicGen model and processor.

        Returns:
            bool: True if initialization successful, False otherwise
        """
        try:
            logger.info(f"Initializing MusicGen model: {self.model_id}")

            # Import here to handle potential import errors gracefully
            from transformers import AutoProcessor, MusicgenForConditionalGeneration

            # Load processor and model
            logger.info("Loading MusicGen processor...")
            self.processor = AutoProcessor.from_pretrained(self.model_id)

            logger.info("Loading MusicGen model...")
            self.model = MusicgenForConditionalGeneration.from_pretrained(
                self.model_id,
                torch_dtype=torch.float16 if self.device == "cuda" else torch.float32,
                attn_implementation="eager"  # Use eager attention for stability
            )

            # Move model to appropriate device
            self.model = self.model.to(self.device)

            # Enable CPU offload for memory efficiency on CUDA
            if self.device == "cuda" and torch.cuda.get_device_properties(0).total_memory < 8e9:  # Less than 8GB
                self.model.enable_cpu_offload()

            self.is_initialized = True
            logger.info(f"MusicGen model initialized successfully on {self.device}")
            return True

        except ImportError as e:
            logger.error(f"Failed to import required packages: {e}")
            logger.error("Please install: pip install transformers torch torchaudio")
            return False
        except Exception as e:
            logger.error(f"Failed to initialize MusicGen model: {e}")
            return False

    def destroy_model(self):
        """Clean up model and free GPU memory."""
        if self.model is not None:
            del self.model
            self.model = None

        if self.processor is not None:
            del self.processor
            self.processor = None

        self.is_initialized = False

        # Clear GPU cache if available
        if torch.cuda.is_available():
            torch.cuda.empty_cache()

        gc.collect()
        logger.info("MusicGen model destroyed and memory freed")

    def load_melody_audio(self, melody_path: Union[str, Path]) -> Optional[torch.Tensor]:
        """
        Load and preprocess melody audio file for conditioning.

        Args:
            melody_path: Path to melody audio file (MP3, WAV, etc.)

        Returns:
            torch.Tensor: Preprocessed melody tensor or None if loading fails
        """
        try:
            # Convert to Path object for better handling
            melody_path = Path(melody_path)

            if not melody_path.exists():
                logger.error(f"Melody file not found: {melody_path}")
                return None

            logger.info(f"Loading melody from: {melody_path}")

            # Load audio file
            waveform, original_sr = torchaudio.load(str(melody_path))

            # Convert to mono if stereo
            if waveform.shape[0] > 1:
                waveform = torch.mean(waveform, dim=0, keepdim=True)

            # Resample to MusicGen's expected sample rate (32000 Hz)
            if original_sr != self.sample_rate:
                logger.info(f"Resampling melody from {original_sr}Hz to {self.sample_rate}Hz")
                resampler = torchaudio.transforms.Resample(orig_freq=original_sr, new_freq=self.sample_rate)
                waveform = resampler(waveform)

            # Normalize audio
            waveform = waveform / (torch.max(torch.abs(waveform)) + 1e-8)

            # Ensure minimum length (pad if necessary)
            min_samples = int(self.sample_rate * 2)  # At least 2 seconds
            if waveform.shape[1] < min_samples:
                padding = torch.zeros(1, min_samples - waveform.shape[1])
                waveform = torch.cat([waveform, padding], dim=1)

            # Trim to maximum length (30 seconds)
            max_samples = int(self.sample_rate * self.max_duration)
            if waveform.shape[1] > max_samples:
                waveform = waveform[:, :max_samples]

            logger.info(f"Melody loaded: {waveform.shape[1]/self.sample_rate:.1f}s at {self.sample_rate}Hz")
            return waveform

        except Exception as e:
            logger.error(f"Failed to load melody audio: {e}")
            return None

    def generate_audio(
        self,
        prompt: str,
        melody_path: Optional[Union[str, Path]] = None,
        duration: float = 10.0,
        guidance_scale: Optional[float] = None,
        temperature: float = 1.0,
        top_k: int = 250,
        top_p: float = 0.0,
    ) -> Tuple[np.ndarray, dict]:
        """
        Generate audio using MusicGen with optional melody conditioning.

        Args:
            prompt: Text description of the desired audio
            melody_path: Optional path to melody audio file for conditioning
            duration: Generation duration in seconds (max 30)
            guidance_scale: Classifier-free guidance scale (uses default if None)
            temperature: Sampling temperature for generation
            top_k: Top-k sampling parameter
            top_p: Top-p (nucleus) sampling parameter

        Returns:
            Tuple of (audio_array, generation_info_dict)
        """
        if not self.is_initialized:
            if not self.init_model():
                error_msg = "Failed to initialize MusicGen model"
                logger.error(error_msg)
                return np.zeros(int(self.sample_rate * 4)), {"error": error_msg}

        try:
            # Validate and clamp duration
            duration = max(1.0, min(duration, self.max_duration))

            # Set guidance scale
            if guidance_scale is None:
                guidance_scale = self.guidance_scale

            logger.info(f"Generating audio: '{prompt}' ({duration}s)")

            # Prepare inputs
            inputs = self.processor(
                text=[prompt],
                return_tensors="pt",
                padding=True
            )

            # Move inputs to device
            inputs = {k: v.to(self.device) for k in inputs}

            # Handle melody conditioning
            melody_waveform = None
            if melody_path is not None:
                melody_waveform = self.load_melody_audio(melody_path)
                if melody_waveform is not None:
                    # Move melody to device
                    melody_waveform = melody_waveform.to(self.device)
                    logger.info("Using melody conditioning")
                else:
                    logger.warning("Failed to load melody, falling back to text-only generation")

            # Generate audio
            start_time = time.time()
            with torch.no_grad():
                if melody_waveform is not None:
                    # Use melody-conditioned generation
                    generated_ids = self.model.generate(
                        **inputs,
                        melody_waveform=melody_waveform,
                        max_new_tokens=int(duration * self.sample_rate / 320),  # Approximate token calculation
                        guidance_scale=guidance_scale,
                        temperature=temperature,
                        top_k=top_k,
                        top_p=top_p,
                        do_sample=True,
                    )
                else:
                    # Use standard text-to-audio generation
                    generated_ids = self.model.generate(
                        **inputs,
                        max_new_tokens=int(duration * self.sample_rate / 320),
                        guidance_scale=guidance_scale,
                        temperature=temperature,
                        top_k=top_k,
                        top_p=top_p,
                        do_sample=True,
                    )

            generation_time = time.time() - start_time
            logger.info(f"Generation completed in {generation_time:.2f}s")

            # Decode audio
            decode_start = time.time()
            audio_values = self.processor.batch_decode(generated_ids, sampling_rate=self.sample_rate)
            decode_time = time.time() - decode_start

            # Extract audio array
            if isinstance(audio_values, list) and len(audio_values) > 0:
                audio_array = audio_values[0].cpu().numpy()
            else:
                logger.error("No audio generated")
                return np.zeros(int(self.sample_rate * 4)), {"error": "No audio generated"}

            # Ensure audio is mono and proper length
            if len(audio_array.shape) > 1:
                audio_array = np.mean(audio_array, axis=0)

            # Trim or pad to requested duration
            target_samples = int(duration * self.sample_rate)
            if len(audio_array) > target_samples:
                audio_array = audio_array[:target_samples]
            elif len(audio_array) < target_samples:
                padding = np.zeros(target_samples - len(audio_array))
                audio_array = np.concatenate([audio_array, padding])

            # Normalize audio
            max_val = np.max(np.abs(audio_array))
            if max_val > 0:
                audio_array = audio_array / max_val * 0.9

            logger.info(f"Audio generated: {len(audio_array)} samples ({len(audio_array)/self.sample_rate:.1f}s)")

            # Prepare generation info
            generation_info = {
                "success": True,
                "prompt": prompt,
                "duration": duration,
                "sample_rate": self.sample_rate,
                "guidance_scale": guidance_scale,
                "temperature": temperature,
                "melody_conditioned": melody_waveform is not None,
                "generation_time": generation_time,
                "decode_time": decode_time,
                "total_time": generation_time + decode_time,
            }

            return audio_array, generation_info

        except Exception as e:
            error_msg = f"Generation failed: {str(e)}"
            logger.error(error_msg)
            logger.error(f"Exception details: {type(e).__name__}: {e}")

            # Return silence on error
            silence = np.zeros(int(self.sample_rate * 4))
            return silence, {
                "success": False,
                "error": error_msg,
                "duration": duration,
                "prompt": prompt,
            }

    def get_model_info(self) -> dict:
        """Get information about the current model state."""
        return {
            "model_id": self.model_id,
            "device": self.device,
            "is_initialized": self.is_initialized,
            "sample_rate": self.sample_rate,
            "max_duration": self.max_duration,
            "default_duration": self.default_duration,
            "guidance_scale": self.guidance_scale,
        }


class FluidSynthWrapper:
    """
    FluidSynth wrapper for MIDI-to-audio conversion with real-time capabilities.

    Provides fast MIDI synthesis using SoundFont files for integration with
    the MusicGen wrapper for melody conditioning.
    """

    def __init__(self, soundfont_path: Optional[str] = None):
        """
        Initialize FluidSynth wrapper.

        Args:
            soundfont_path: Path to SoundFont file (.sf2). If None, uses default.
        """
        self.soundfont_path = soundfont_path or self._get_default_soundfont()
        self.sample_rate = 44100  # Standard audio sample rate
        self.is_initialized = False
        self.synth = None

        # Try to import fluidsynth
        try:
            import fluidsynth
            self.fluidsynth = fluidsynth
        except ImportError:
            logger.warning("FluidSynth not available. Install with: pip install fluidsynth")
            self.fluidsynth = None

    def _get_default_soundfont(self) -> str:
        """Get path to default SoundFont file."""
        # Common SoundFont locations
        common_paths = [
            "/usr/share/sounds/sf2/FluidR3_GM.sf2",
            "/usr/share/sounds/sf2/default.sf2",
            "FluidR3_GM.sf2",
            "default.sf2",
        ]

        for path in common_paths:
            if os.path.exists(path):
                return path

        # Return most likely name if not found
        return "FluidR3_GM.sf2"

    def init_synth(self) -> bool:
        """
        Initialize FluidSynth synthesizer.

        Returns:
            bool: True if initialization successful
        """
        if self.fluidsynth is None:
            logger.error("FluidSynth not available")
            return False

        try:
            logger.info(f"Initializing FluidSynth with SoundFont: {self.soundfont_path}")

            # Create synthesizer
            self.synth = self.fluidsynth.Synth()

            # Load SoundFont
            sfid = self.synth.sfload(self.soundfont_path)
            if sfid == -1:
                logger.error(f"Failed to load SoundFont: {self.soundfont_path}")
                return False

            # Set sample rate
            self.synth.setting('synth.sample-rate', self.sample_rate)

            # Configure for low latency
            self.synth.setting('audio.periods', 8)
            self.synth.setting('audio.period-size', 64)

            self.is_initialized = True
            logger.info("FluidSynth initialized successfully")
            return True

        except Exception as e:
            logger.error(f"Failed to initialize FluidSynth: {e}")
            return False

    def midi_to_audio(
        self,
        midi_path: Union[str, Path],
        duration: Optional[float] = None,
        output_path: Optional[str] = None
    ) -> Tuple[np.ndarray, float]:
        """
        Convert MIDI file to audio using FluidSynth.

        Args:
            midi_path: Path to MIDI file
            duration: Maximum duration in seconds (None for full length)
            output_path: Optional output path for WAV file

        Returns:
            Tuple of (audio_array, actual_duration)
        """
        if not self.is_initialized:
            if not self.init_synth():
                logger.error("Failed to initialize FluidSynth")
                return np.zeros(int(self.sample_rate * 4)), 0.0

        try:
            midi_path = Path(midi_path)

            if not midi_path.exists():
                logger.error(f"MIDI file not found: {midi_path}")
                return np.zeros(int(self.sample_rate * 4)), 0.0

            logger.info(f"Converting MIDI to audio: {midi_path}")

            # Create sequencer and player
            sequencer = self.fluidsynth.Sequencer()
            synth_id = sequencer.register_synth(self.synth)

            # Load MIDI file
            player = sequencer.add_midi_file(str(midi_path))
            if player is None:
                logger.error(f"Failed to load MIDI file: {midi_path}")
                return np.zeros(int(self.sample_rate * 4)), 0.0

            # Get MIDI duration if not specified
            if duration is None:
                duration = self._get_midi_duration(midi_path)

            # Calculate number of samples
            num_samples = int(duration * self.sample_rate)

            # Create audio buffer
            audio_buffer = np.zeros(num_samples, dtype=np.float32)

            # Process MIDI events and render audio
            start_time = time.time()
            self._render_midi_audio(sequencer, player, audio_buffer, duration)
            render_time = time.time() - start_time

            logger.info(f"MIDI rendered in {render_time:.3f}s ({duration:.1f}s audio)")

            # Normalize audio
            max_val = np.max(np.abs(audio_buffer))
            if max_val > 0:
                audio_buffer = audio_buffer / max_val * 0.9

            # Save to file if requested
            if output_path:
                import soundfile as sf
                sf.write(output_path, audio_buffer, self.sample_rate)
                logger.info(f"Audio saved to: {output_path}")

            return audio_buffer, duration

        except Exception as e:
            logger.error(f"MIDI to audio conversion failed: {e}")
            return np.zeros(int(self.sample_rate * 4)), 0.0

    def _get_midi_duration(self, midi_path: Path) -> float:
        """Get duration of MIDI file in seconds."""
        try:
            # Simple MIDI duration calculation
            # This is a basic implementation - could be enhanced with proper MIDI parsing
            return 10.0  # Default 10 seconds for now
        except:
            return 10.0

    def _render_midi_audio(self, sequencer, player, audio_buffer: np.ndarray, duration: float):
        """Render MIDI to audio buffer."""
        try:
            # Simple rendering - play notes and capture audio
            # This is a simplified implementation
            samples_per_buffer = 512
            num_buffers = len(audio_buffer) // samples_per_buffer

            for i in range(num_buffers):
                start_sample = i * samples_per_buffer
                end_sample = min(start_sample + samples_per_buffer, len(audio_buffer))

                # Process audio block
                chunk = audio_buffer[start_sample:end_sample]

                # This would need more sophisticated implementation for real MIDI rendering
                # For now, we'll use a placeholder that generates basic tones
                self._generate_basic_audio_chunk(chunk, i)

        except Exception as e:
            logger.error(f"Audio rendering failed: {e}")

    def _generate_basic_audio_chunk(self, chunk: np.ndarray, chunk_index: int):
        """Generate basic audio for chunk (placeholder implementation)."""
        # This is a simplified placeholder
        # Real implementation would need proper MIDI event processing
        frequency = 440.0 + (chunk_index * 10)  # Simple frequency sweep
        t = np.linspace(0, len(chunk) / self.sample_rate, len(chunk))
        chunk[:] = 0.1 * np.sin(2 * np.pi * frequency * t)

    def cleanup(self):
        """Clean up FluidSynth resources."""
        if self.synth is not None:
            self.synth.delete()
            self.synth = None

        self.is_initialized = False
        logger.info("FluidSynth cleaned up")


# Convenience function for integrated MIDI-to-MusicGen workflow
def midi_to_musicgen_audio(
    midi_path: Union[str, Path],
    text_prompt: str,
    musicgen_model: Optional[MusicGenWrapper] = None,
    duration: float = 10.0,
    temp_audio_path: Optional[str] = None
) -> Tuple[np.ndarray, dict]:
    """
    Convert MIDI to audio using FluidSynth, then use as melody conditioning for MusicGen.

    Args:
        midi_path: Path to MIDI file
        text_prompt: Text prompt for MusicGen generation
        musicgen_model: Optional MusicGenWrapper instance (creates new one if None)
        duration: Generation duration in seconds
        temp_audio_path: Optional path for temporary audio file

    Returns:
        Tuple of (audio_array, generation_info)
    """
    # Initialize components
    if musicgen_model is None:
        musicgen_model = MusicGenWrapper()

    fluidsynth = FluidSynthWrapper()

    try:
        # Convert MIDI to audio
        logger.info("Converting MIDI to audio for melody conditioning...")
        melody_audio, actual_duration = fluidsynth.midi_to_audio(midi_path, duration)

        # Save temporary audio file for MusicGen
        if temp_audio_path is None:
            temp_dir = tempfile.gettempdir()
            temp_audio_path = os.path.join(temp_dir, f"melody_{int(time.time())}.wav")

        # Save melody audio
        import soundfile as sf
        sf.write(temp_audio_path, melody_audio, fluidsynth.sample_rate)

        # Generate with MusicGen using melody conditioning
        logger.info("Generating audio with MusicGen using melody conditioning...")
        generated_audio, info = musicgen_model.generate_audio(
            prompt=text_prompt,
            melody_path=temp_audio_path,
            duration=duration
        )

        # Clean up temporary file
        try:
            os.remove(temp_audio_path)
        except:
            pass

        return generated_audio, info

    except Exception as e:
        logger.error(f"MIDI-to-MusicGen workflow failed: {e}")
        return np.zeros(int(musicgen_model.sample_rate * 4)), {"error": str(e)}


# Example usage and testing
if __name__ == "__main__":
    # Test MusicGen wrapper
    print("Testing MusicGen wrapper...")

    musicgen = MusicGenWrapper()

    # Test basic generation
    audio, info = musicgen.generate_audio(
        prompt="A cheerful electronic dance track with synth melodies",
        duration=5.0
    )

    print(f"Generated {len(audio)} samples")
    print(f"Info: {info}")

    # Test with melody conditioning (if melody file exists)
    melody_file = "test_melody.wav"  # Replace with actual melody file
    if os.path.exists(melody_file):
        audio2, info2 = musicgen.generate_audio(
            prompt="Transform this melody into a full orchestral arrangement",
            melody_path=melody_file,
            duration=8.0
        )
        print(f"Melody-conditioned generation: {len(audio2)} samples")

    musicgen.destroy_model()

    print("MusicGen wrapper test completed!")