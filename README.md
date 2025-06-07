# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

OBSIDIAN-Neural transforms AI into a live music instrument using intelligent LLM prompts and Stable Audio Open to create contextually aware loops directly in your DAW.

## 🏆 Press Coverage

📰 **[Featured on Bedroom Producers Blog](https://bedroomproducersblog.com/2025/06/06/obsidian-neural-sound-engine/)** - _"OBSIDIAN Neural Sound Engine, a FREE AI-powered jam partner"_

> "Too many AI projects focus on the things AI can save you from doing rather than how AI can help you get better at what you do." - James Nugent, BPB

---

![OBSIDIAN-Neural Interface](./screenshot.png)

## 🎵 Live Demo

[![OBSIDIAN Live Performance](https://img.youtube.com/vi/l4KMC5adxVA/maxresdefault.jpg)](https://www.youtube.com/watch?v=l4KMC5adxVA)

_Click to watch: World's First AI Jam Partner in action!_

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

### 🥁 **Built-in Step Sequencer**

- **16-step programmable sequencer** per track with multi-measure support
- **DAW sync**: Perfectly locked to host tempo and transport
- **Per-step velocity control** and multi-measure patterns (1-4 measures)
- **Start/Stop on measure boundaries** for seamless live performance
- **Real-time pattern editing** while sequencer is playing

### 🎛️ **Live Performance Ready**

- **Global custom prompts** shared across projects
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

The installer automatically handles everything: prerequisites, Python environment, model download (2.49 GB), and VST3 build.

### Start Server

```bash
# Windows
env\Scripts\activate.bat && python main.py

# Linux/Mac
source env/bin/activate && python main.py
```

---

## 🎵 Usage Workflow

1. **Setup**: Start AI server, load OBSIDIAN-Neural in DAW
2. **Generate**: Add tracks, enter creative prompts, generate AI audio
3. **Perform**: Trigger tracks with MIDI, auto-sync to DAW tempo
4. **Control**: Map hardware controllers with MIDI Learn system

---

## ⚙️ System Requirements

- **GPU**: NVIDIA with CUDA (required for real-time)
- **RAM**: 8GB+ for model loading
- **OS**: Windows 10+, Linux, macOS
- **DAW**: Any VST3-compatible DAW

---

## 🎯 Why OBSIDIAN-Neural?

LLM understands musical context and generates contextually aware loops that evolve with your performance. Perfect for live electronic music with no pre-recorded samples needed.

**Example LLM-generated prompt:**
_"Deep techno kick with sidechain compression, 126 BPM, dark atmosphere, minimal hi-hats, rolling bassline"_

---

## ⚠️ Current Status

**VST3 Plugin**: Fully functional and performance-ready  
**Standalone Version**: Not planned (VST3 focus)

**Known Issues:**

- CUDA required (CPU too slow for live use)
- Large model files (initial download time)

---

## 🛠️ Contributing

Found a bug? Please report with DAW name/version, steps to reproduce, and screenshots if UI-related.

---

## 📝 License

MIT License - Feel free to modify, but please keep original attribution to InnerMost47

---

**OBSIDIAN-Neural** - Where artificial intelligence meets live music performance.

_Developed by innermost47_
