# OBSIDIAN - Neural Sound Engine v1.0

🎵 **Real-time AI music generation plugin for live performance**

OBSIDIAN transforms AI into a live music instrument using intelligent LLM prompts and Stable Audio Open to create contextually aware loops directly in your DAW.

---

## 🔮 Key Features

### 🤖 **Intelligent AI Generation**

- **LLM Brain**: Analyzes sessions and generates smart prompts
- **Stable Audio Open**: High-quality 44.1kHz electronic music generation
- **Real-time**: Everything happens live for performance use

### 🎹 **Multi-Track Sampler**

- **8 independent tracks** with MIDI triggering (C3-B3)
- **Advanced waveform editor** with zoom, precise loop points
- **Smart time-stretching** with DAW tempo sync
- **Individual outputs** for separate mixing

### 🎛️ **Live Performance Ready**

- **Global custom prompts** shared across projects
- **Multiple load modes**: Auto, MIDI-triggered, or manual
- **MIDI Learn system** for hardware controller mapping
- **Session management** with save/load
- **Background processing** - works with VST window closed

---

## 🚀 Quick Install

### Automated Setup

**Windows:**

```bash
install.bat
```

**Linux/Mac:**

```bash
chmod +x install.sh
./install.sh
```

The installer automatically:

- ✅ Checks prerequisites (Python, CMake, Git, CUDA)
- 📦 Creates Python environment
- 🤖 Downloads Gemma-3-4B model (2.49 GB)
- 🔨 Builds VST plugin
- ⚙️ Configures everything

### Start Server

```bash
# Windows
env\Scripts\activate.bat && python main.py

# Linux/Mac
source env/bin/activate && python main.py
```

### Configure (Optional)

Create `.env` file for custom settings:

```env
LLM_MODEL_PATH=/path/to/models/gemma-3-4b-it.gguf
AUDIO_MODEL=stable-audio-open
ENVIRONMENT=dev  # 'prod' for API key enforcement
HOST=127.0.0.1
PORT=8000
```

---

## 🎵 Usage Workflow

### 1. **Setup**

- Start AI server: `python main.py`
- Load OBSIDIAN in your DAW
- Set server URL: `http://localhost:8000`

### 2. **Create & Generate**

- Add multiple tracks with unique MIDI notes
- Enter creative prompts (style, BPM, key, duration)
- Choose load mode and generate AI audio

### 3. **Live Performance**

- Trigger tracks with MIDI keyboard/controller
- Each note plays assigned track as one-shot
- Auto-sync to DAW tempo
- Mix with individual controls

### 4. **Hardware Control**

- **MIDI Learn**: Map any hardware controller to plugin parameters
- **Smart mapping**: Avoids reserved sample notes (C3-G3)
- **Persistent mappings**: Controllers stay mapped across sessions
- **Real-time control**: Adjust parameters during live performance

### 5. **Advanced Editing**

- Zoom (Ctrl+Wheel), scroll (Wheel)
- Drag markers for custom loop regions

---

## 🎛️ What's New in v0.4.6

- **Global custom prompts** management
- **WAV file support** for better performance
- **Simplified BPM control** via pitch knob
- **Enhanced stability** and crash prevention
- **Background processing** improvements
- **Multi-user memory** with persistent sessions
- **MIDI Learn system** with controller mapping
- **Improved track management** and slot synchronization

---

## ⚙️ System Requirements

- **GPU**: NVIDIA with CUDA (required for real-time)
- **RAM**: 8GB+ for model loading
- **OS**: Windows 10+, Linux, macOS
- **DAW**: Any VST3-compatible DAW

---

## 🎯 Roadmap

**Next Updates:**

- **MIDI Learn on master channel** for global control
- **Code refactoring** and cleanup
- **Standalone version evaluation** (transport controls needed)
- Enhanced stability improvements

**Current Focus:** VST3 plugin (fully functional)

## 🎵 Why OBSIDIAN?

- LLM understands musical context and progression
- Generates contextually aware loops that evolve
- Perfect for live electronic music performance
- No pre-recorded samples needed

**Example LLM-generated prompt:**
_"Deep techno kick with sidechain compression, 126 BPM, dark atmosphere, minimal hi-hats, rolling bassline"_

---

## 📋 API Keys (Optional)

Only needed for:

- External server access
- Shared usage with collaborators
- Production deployment

**Local usage requires no API key.**

---

## ⚠️ Current Status

**Active Development** - Stable enough for improvements and testing

**Working:** VST3 plugin fully functional  
**In Progress:** Standalone version (transport controls missing)

**Known Issues:**

- Standalone version incomplete (missing transport controls)
- CUDA required (CPU too slow for live use)
- Large model files (initial download time)

---

## 🛠️ Contributing

Found a bug? Please report with:

- DAW name/version
- Steps to reproduce
- Screenshots if UI-related

---

## 📝 License & Attribution

MIT License - Feel free to modify, but please keep original attribution to InnerMost47

---

**OBSIDIAN** - Where artificial intelligence meets live music performance.

_Developed by innermost47_
