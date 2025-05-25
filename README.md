# 🎧 DJ-IA VST: Real-Time AI Music Generation Plugin

DJ-IA VST is a live music generation plugin that uses AI to create music loops in real-time directly in your DAW. The core innovation is the **LLM-driven prompt generation** that feeds **Stable Audio Open** to create contextually intelligent music that evolves with your session.

⚠️ **Proof of Concept** - Currently in active development

## 🎯 Core Concept

1. **LLM Brain**: Analyzes your session and generates intelligent prompts
2. **Stable Audio Open**: Generates high-quality audio from these prompts
3. **Time-Stretching**: Automatically syncs generated audio to your DAW tempo
4. **Live Performance**: Everything happens in real-time for live use

## 🛠️ Building the Plugin

### Prerequisites

1. **Visual Studio 2019/2022** with C++ build tools
2. **CMake 3.15+**
3. **JUCE Framework**:
   ```bash
   git clone https://github.com/juce-framework/JUCE.git
   cd JUCE
   git checkout 7.0.12
   ```

### Build Steps

```bash
cd vst/
mkdir build && cd build
cmake .. -DJUCE_DIR="C:/path/to/JUCE" -G "Visual Studio 17 2022"
cmake --build . --config Release
```

Copy the built `DJ-IA.vst3` to `C:\Program Files\Common Files\VST3\`

## 🚀 Setup & Usage

### 1. Install AI Backend

```bash
python -m venv dj_ia_env
dj_ia_env\Scripts\activate

# Core dependencies
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
pip install stable-audio-tools
pip install llama-cpp-python librosa soundfile pyrubberband
pip install fastapi uvicorn
```

### 2. Start AI Server

```bash
python main.py --model-path "C:\path\to\your\model.gguf" --profile "techno_minimal" --output-dir "./output" --clean --generation-duration 6.0 --mode api --host 0.0.0.0 --audio-model stable-audio-open
```

_(This command will be simplified in future versions)_

### 3. Load Plugin in DAW

- Add DJ-IA VST to any track
- Plugin auto-connects to AI server
- Click "Generate" to create contextual loops
- Everything syncs to your project tempo automatically

## 🎵 Why Stable Audio Open?

**Stable Audio Open** is the heart of DJ-IA because it:

- **Higher quality** than MusicGen for electronic music
- **Better stereo imaging** and spatial characteristics
- **More responsive** to detailed prompts from the LLM
- **44.1kHz native** output (vs 32kHz for MusicGen)
- **Faster generation** for live performance needs

The LLM creates sophisticated prompts like:

```
"Deep techno kick with sidechain compression, 126 BPM,
dark atmosphere, minimal hi-hats, rolling bassline"
```

## ⚠️ Known Issues

- **Window close crash**: Don't close plugin window, use bypass instead
- **First generation slow**: Models need to load initially
- **CUDA required**: CPU generation too slow for live use

## 🎯 Live Performance Focus

DJ-IA is designed for **live performance**:

- Generate loops that fit your current musical context
- LLM understands what elements are needed next
- Time-stretching keeps everything locked to tempo
- No complex effects - just pure AI generation + sync

This is about **intelligent music creation**, not effects processing.
