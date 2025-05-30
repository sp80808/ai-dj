# 🎧 DJ-IA VST: Real-Time AI Multi-Sampler Plugin

![DJ-IA VST Interface](./screenshot.png)

DJ-IA VST is a live music generation plugin that uses AI to create music loops in real-time directly in your DAW. The core innovation is the **LLM-driven prompt generation** that feeds **Stable Audio Open** to create contextually intelligent music that evolves with your session.

⚠️ **Proof of Concept** - Currently in active development

🚀 **[Download Alpha Release](https://github.com/innermost47/ai-dj/releases)** - Get the latest pre-release binaries

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

## 🚀 Quick Install & Setup

### **Automated Installation**

**Windows:**

```bash
install.bat
```

**Linux/Mac:**

```bash
chmod +x install.sh
./installa.sh
```

The installer automatically:

- ✅ Checks prerequisites (Python, CMake, Git, CUDA)
- 📦 Creates Python environment
- ⬇️ Downloads dependencies (PyTorch with CUDA if available)
- 🤖 Downloads Gemma-3-4B AI model (2.49 GB)
- ⚙️ Creates configuration file
- 🔨 Builds VST plugin
- 📋 Provides clear next steps

#### Configure Environment

Create `.env` file:

```bash
DJ_IA_API_KEYS=your_api_keys_here  # For external access (optional)
LLM_MODEL_PATH=/path/to/models/gemma-3-4b-it.gguf
AUDIO_MODEL=stable-audio-open
ENVIRONMENT=dev  # Use 'prod' for API key enforcement
HOST=127.0.0.1 # 0.0.0.0 for prod
PORT=8000
```

## 🚀 Starting the Server

After installation, start the AI server:

**Windows:**

```bash
env\Scripts\activate.bat && python main.py
```

**Linux/Mac:**

```bash
source env/bin/activate && python main.py
```

### **Server Options**

- `--model-path PATH` - Override LLM model path (default: from .env)
- `--audio-model MODEL` - musicgen-small|medium|large|stable-audio-open|stable-audio-pro (default: from .env)
- `--output-dir DIR` - Output directory (default: ./output)
- `--clean` - Clean output directory on startup
- `--host HOST` - Server host (default: from .env)
- `--port PORT` - Server port (default: from .env)

**Example:**

```bash
python main.py --audio-model stable-audio-open --clean
```

## 🔐 API Keys & External Access

**API Keys are optional** and only needed for:

- **External server access** - Allow remote connections to your DJ-IA server
- **Shared usage** - Give API keys to friends/collaborators for access
- **Production deployment** - Secure your server when hosting publicly

**Local usage** (default): No API key required
**External sharing**: Set `ENVIRONMENT=prod` in `.env` and provide API keys

## 🎵 Usage Workflow

### **1. Start Server & Load Plugin**

- Start the AI server with `python main.py`
- Add DJ-IA VST to a track in your DAW
- Set server URL to `http://localhost:8000` in plugin
- Add API key if using external server access

### **2. Create Multi-Track Setup**

- Click **"+ Add Track"** for new sampler tracks
- Assign unique MIDI notes (C4, D4, E4, etc.)
- Name tracks for organization

### **3. AI Generation**

- Select a track and enter creative prompts
- Choose style, BPM, key, generation duration
- Click **"Generate"** to create AI audio
- **Load Modes**:
  - **Auto Load**: Immediate loading after generation
  - **Load on MIDI**: Load when pressing any MIDI key (default)
  - **Manual**: Click "Load Sample" when ready

### **4. Live Performance**

- Trigger tracks with MIDI keyboard/controller
- Each note plays its assigned track as one-shot
- Tracks auto-sync to DAW tempo
- Mix with individual volume/pan controls

### **5. Waveform Editing**

- Click **"Wave"** for waveform editor
- Zoom with Ctrl+Wheel, scroll with Wheel
- Drag markers to define playback regions
- Right-click to lock/unlock during playback

## 🎛️ DAW Integration

### **Multi-Output Setup**

- **Main Output**: Mixed audio from all tracks
- **Individual Outputs**: Track 1, Track 2, Track 3... for separate processing
- Route individual outputs to different DAW channels

### **MIDI Control**

- Send MIDI notes to the plugin track
- Each note number triggers its sampler track
- Works with any MIDI controller or sequencer
- Perfect for live performance and production

### **Session Management**

- **Save/Load Sessions**: Complete multi-track setups
- **Preset System**: Save favorite prompts
- **Smart Loading**: Choose when samples load

## 🎵 Why Stable Audio Open?

**Stable Audio Open** is optimized for DJ-IA because:

- **Higher quality** than MusicGen for electronic music
- **Better stereo imaging** and spatial characteristics
- **More responsive** to detailed LLM-generated prompts
- **44.1kHz native** output (vs 32kHz for MusicGen)
- **Faster generation** for live performance

The LLM creates sophisticated prompts like:

```
"Deep techno kick with sidechain compression, 126 BPM,
dark atmosphere, minimal hi-hats, rolling bassline"
```

## ⚠️ System Requirements

- **NVIDIA GPU with CUDA** - Required for real-time generation
- **8GB+ RAM** - For model loading and audio processing
- **Windows 10+** or **Linux/macOS** - Cross-platform support
- **DAW with VST3 support** - Any modern DAW

## ⚠️ Known Issues

- **CUDA required**: CPU generation too slow for live use
- **Large model files**: Initial download takes time
- **Minor UI glitches**: Occasional display inconsistencies

## 🎯 Live Performance Focus

DJ-IA is designed for **intelligent live performance**:

- Generate contextually aware loops
- LLM understands musical progression needs
- Multi-track sampler for complex arrangements
- Time-stretching for perfect tempo sync
- MIDI triggering for precise control
- Smart loading to prevent interruptions

This is about **intelligent music creation**, not effects processing.
