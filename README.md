# 🎧 DJ-IA VST: Real-Time AI Multi-Sampler Plugin

![DJ-IA VST Interface](./screenshot.png)

DJ-IA VST is a live music generation plugin that uses AI to create music loops in real-time directly in your DAW. The core innovation is the **LLM-driven prompt generation** that feeds **Stable Audio Open** to create contextually intelligent music that evolves with your session.

⚠️ **Proof of Concept** - Currently in active development

## 🎯 Core Concept

1. **LLM Brain**: Analyzes your session and generates intelligent prompts
2. **Stable Audio Open**: Generates high-quality audio from these prompts
3. **Multi-Track Sampler**: Up to 8 independent tracks with individual MIDI triggering
4. **Time-Stretching**: Automatically syncs generated audio to your DAW tempo
5. **Live Performance**: Everything happens in real-time for live use

## 🎹 Multi-Sampler Features

### **MIDI-Triggered Playback**

- **8 independent tracks** with individual MIDI note assignments (C3-B3 by default)
- **One-shot playback**: Samples play from start to end points, triggered by MIDI
- **Real-time sync**: Perfect timing with your DAW's transport
- **Individual outputs**: Route each track to separate DAW channels for mixing

### **Advanced Waveform Editor**

- **High-resolution waveform display** with professional visual quality
- **Zoom & scroll**: Ctrl+Wheel to zoom, Wheel to scroll horizontally
- **Precise loop points**: Drag start/end markers for exact sample sections
- **Live playback head**: Real-time position indicator with auto-follow
- **Smart locking**: Loop points lock automatically during playback

### **Intelligent Time-Stretching**

- **Host BPM Sync** (default): Auto-sync to DAW tempo
- **Manual BPM**: Set custom tempo per track
- **Off**: Play at original speed
- **Hybrid**: Combine host sync + manual offset

### **Per-Track Controls**

- **Volume & Pan**: Individual mixing controls
- **Mute & Solo**: Standard sampler functionality
- **Track naming**: Custom names for organization
- **Visual indicators**: Playing status, MIDI activity

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
4. **SoundTouch Library**:
   ```bash
   git clone https://github.com/suomipelit/soundtouch.git
   cd soundtouch
   mkdir build && cd build
   cmake .. -G "Visual Studio 17 2022"
   cmake --build . --config Release
   ```

### Build Steps

```bash
cd vst/
mkdir build && cd build
cmake .. -DJUCE_DIR="C:/path/to/JUCE" -DSOUNDTOUCH_DIR="C:/path/to/soundtouch" -G "Visual Studio 17 2022"
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
pip install fastapi uvicorn python-dotenv
```

### 2. Configure Environment

Rename `.env.example` to `.env` and fill in your configuration:

```bash
# .env file
DJ_IA_API_KEY=your_api_key_here          # Used in production only
LLM_MODEL_PATH=C:/path/to/your/model.gguf # Path to your LLM model
AUDIO_MODEL=stable-audio-open             # Music generation model
ENVIRONMENT=dev                           # dev/prod (API key only required in prod)
```

**Audio Model Options:**

- `musicgen-small` - Fast, lower quality
- `musicgen-medium` - Balanced speed/quality
- `musicgen-large` - Highest quality, slower
- `stable-audio-open` - **Recommended** for electronic music
- `stable-audio-pro` - Premium version (if available)

### 3. Start AI Server

```bash
python main.py --host 0.0.0.0 --port 8000 --clean
```

**Available options:**

- `--model-path`: Override LLM model path from .env
- `--audio-model`: Override audio model from .env
- `--output-dir`: Output directory (default: ./output)
- `--generation-duration`: Default sample length in seconds (default: 6.0)
- `--clean`: Clean output directory on startup
- `--host`: Server host (default: localhost)
- `--port`: Server port (default: 8000)

### 4. Load Plugin in DAW

- Add DJ-IA VST to any track
- Configure server URL (`http://localhost:8000`) and API key in plugin
- Create multiple tracks with different MIDI notes
- Generate AI loops for each track
- Play tracks with MIDI keyboard/controller

## 🎵 Multi-Sampler Workflow

### **1. Track Setup**

- Click **"+ Add Track"** to create new sampler tracks
- Assign unique MIDI notes to each track (C4, D4, E4, etc.)
- Name tracks for easy identification

### **2. AI Generation**

- Select a track and enter creative prompts
- Choose style, BPM, key, generation duration, and preferred stems
- Click **"Generate"** to create AI audio for that track
- **Load Modes**:
  - **Auto Load**: Samples load immediately after generation
  - **Load on MIDI**: Samples load when you press any MIDI key (default)
  - **Manual**: Click "Load Sample" button when ready

### **3. Live Performance**

- Use MIDI keyboard/controller to trigger tracks
- Each note plays its assigned track as a one-shot sample
- Tracks automatically sync to your DAW's tempo
- Mix individual tracks using volume/pan controls

### **4. Waveform Editing**

- Click **"Wave"** button to open waveform editor
- Zoom in on interesting sections with Ctrl+Wheel
- Drag start/end markers to define playback region
- Right-click to lock/unlock editing during playback

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

## 🎛️ DAW Integration

### **Multi-Output Setup**

- **Main Output**: Mixed audio from all playing tracks
- **Individual Outputs**: Track 1, Track 2, Track 3... for separate processing
- Route individual outputs to different DAW channels for advanced mixing

### **MIDI Control**

- Send MIDI notes to the plugin track
- Each note number triggers its assigned sampler track
- Works with any MIDI controller, keyboard, or sequenced notes
- Perfect for live performance and studio production

### **Session Management**

- **Save/Load Sessions**: Store complete multi-track setups (metadata only for performance)
- **Preset System**: Save favorite prompts for quick access
- **Auto-Load Modes**: Choose when samples load for optimal workflow

## ⚠️ Known Issues

- **CUDA required**: CPU generation too slow for live use
- **Audio crackling**: May occur with 2+ simultaneous tracks (increase DAW buffer size)
- **UI color updates**: Some button colors don't refresh immediately
- **Minor UI glitches**: Occasional display inconsistencies

## 🎯 Live Performance Focus

DJ-IA is designed for **live performance**:

- Generate loops that fit your current musical context
- LLM understands what elements are needed next
- Multi-track sampler enables complex live arrangements
- Time-stretching keeps everything locked to tempo
- MIDI triggering provides precise performance control
- Smart loading modes prevent audio interruptions
- No complex effects - just pure AI generation + intelligent sampling

This is about **intelligent music creation**, not effects processing.
