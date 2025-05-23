import torch
import torchaudio
from einops import rearrange
from stable_audio_tools import get_pretrained_model
from stable_audio_tools.inference.generation import generate_diffusion_cond
import random
from core.layer_manager import LayerManager

device = "cuda" if torch.cuda.is_available() else "cpu"

# Download model
model, model_config = get_pretrained_model("stabilityai/stable-audio-open-1.0")
sample_rate = model_config["sample_rate"]
sample_size = model_config["sample_size"]
model = model.to(device)

# Set up text and timing conditioning
conditioning = [
    {"prompt": "128 BPM tech house drum loop", "seconds_start": 0, "seconds_total": 8}
]

# Generate a random seed within safe bounds
seed_value = random.randint(0, 2**31 - 1)
print(f"Using seed: {seed_value}")

# Generate stereo audio with random seed
output = generate_diffusion_cond(
    model,
    steps=100,
    cfg_scale=7,
    conditioning=conditioning,
    sample_size=sample_size,
    sigma_min=0.3,
    sigma_max=500,
    sampler_type="dpmpp-3m-sde",
    device=device,
    seed=seed_value,
)

# Rearrange audio batch to a single sequence
output = rearrange(output, "b d n -> d (b n)")

# Peak normalize, clip, convert to int16, and save to file
output = (
    output.to(torch.float32)
    .div(torch.max(torch.abs(output)))
    .clamp(-1, 1)
    .mul(32767)
    .to(torch.int16)
    .cpu()
)

temp_path = "output.wav"
torchaudio.save("output.wav", output, sample_rate)

layer_manager = LayerManager(sample_rate=sample_rate, output_dir="output")
processed_path = layer_manager._prepare_sample_for_loop(
    original_audio_path=temp_path,
    layer_id=f"layer_1",
    measures=1,
    model_name="stable-audio-open",
)
print(processed_path)
