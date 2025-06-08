# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

OBSIDIAN-Neural transforms AI into a live music instrument using intelligent LLM prompts and Stable Audio Open to create contextually aware loops directly in your DAW.

## 🏆 Press Coverage

**[Featured on Bedroom Producers Blog](https://bedroomproducersblog.com/2025/06/06/obsidian-neural-sound-engine/)**  
_"OBSIDIAN Neural Sound Engine, a FREE AI-powered jam partner"_

> "Too many AI projects focus on the things AI can save you from doing rather than how AI can help you get better at what you do."  
> — James Nugent, BPB

**[Listed on AudioZ (community mirror)](https://audioz.download/software/win/278483-download_innermost47-obsidian-v047-real-time-ai-music-generator-for-live-performance-vst3-standalone-win-mac-free.html)**  
_(Unofficial mirror of an early test build — for the latest stable version, always use the [official GitHub release](https://github.com/innermost47/ai-dj/releases))_

**[Featured on DTM Plugin Sale (Japan)](https://projectofnapskint.com/obsidian-2/)**  
_"AI を楽器に変える時代が来た！"_  
(The era of turning AI into musical instruments has arrived!)

**[Featured on Shuziyinpin (China)](https://www.shuziyinpin.vip/31549.html)**  
_International coverage in Asian music production communities_

---

> ✨ OBSIDIAN is 100% free and open source. All contributions, builds, and community use are welcome — but for the best experience, get it from the [official GitHub](https://github.com/innermost47/ai-dj).

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

## Prerequisites

### System Requirements

**Hardware:**

- NVIDIA GPU with CUDA support (RTX 3060+ recommended)
- **8GB+ VRAM** for Stable Audio Open standard model
- **Note:** Stable Audio Open Small model may require less VRAM (testing needed)
- 16GB+ system RAM recommended

**Software Dependencies:**

- **NVIDIA CUDA Toolkit** (latest version)
- **Python 3.10** (other versions may not work properly)
- **CMake** (3.16 or higher)
- **Git**
- **Build tools:**
  - Windows: Visual Studio Build Tools or Visual Studio Community
  - Linux: GCC/G++ compiler, make
- **Download utilities:**
  - Windows: `curl.exe` (usually included with Windows 10+)
  - Linux: `wget` (install with package manager if not present)

### Stable Audio Open Access & Licensing

⚠️ **Important:** Before using OBSIDIAN-Neural, you must:

1. **Request access** to the Stable Audio Open model on Hugging Face:
   👉 **[Request Access Here](https://huggingface.co/stabilityai/stable-audio-open-1.0)**

2. **Understand the licensing terms:**
   - **Personal/Research use**: Free under Stability AI Community License
   - **Commercial use**: Requires Enterprise License if your organization generates **>$1M USD annual revenue**
   - **For revenue under $1M/year**: Commercial use allowed under Community License
   - Generated audio is yours to use according to license terms

**License Details:** Stable Audio Open uses the Stability AI Community License, not a traditional open-source license. Commercial use is permitted for individuals and organizations with annual revenue under $1M USD. For enterprise use (>$1M revenue), contact Stability AI for licensing.

## Setup

After getting access approved on Hugging Face:

1. **Open a command line/terminal**
2. **Install Hugging Face CLI:** `pip install huggingface_hub`
3. **Login with your token:** `huggingface-cli login`
4. **Enter your HF token** when prompted
5. **Then proceed with installation below**

⚠️ You must complete steps 1-4 **before** running the plugin!

---

### Installation

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

## 🐛 Bug Reports & Issues

Found a bug or unexpected behavior? Help improve OBSIDIAN-Neural by reporting issues!

### 📝 How to Report

1. **[Create an issue](https://github.com/innermost47/ai-dj/issues/new)** on GitHub
2. **Include essential info:**
   - DAW name and version
   - Operating system
   - Steps to reproduce the bug
   - Expected vs actual behavior
   - Screenshots/videos if UI-related

### 🚀 What Gets Priority

- Crashes or data loss
- Sequencer timing issues
- AI generation failures
- MIDI mapping problems

**Your reports help make OBSIDIAN-Neural more stable for everyone!**

---

## 📝 License

MIT License - Feel free to modify, but please keep original attribution to InnerMost47

---

**OBSIDIAN-Neural** - Where artificial intelligence meets live music performance.

_Developed by innermost47_
