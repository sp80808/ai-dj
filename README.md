# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

OBSIDIAN-Neural transforms AI into a live music instrument using intelligent LLM prompts and Stable Audio Open to create contextually aware loops directly in your DAW.

## ⚠️ Pre-Release Status

**OBSIDIAN-Neural is currently in pre-release.** While functional, you may encounter bugs and instabilities between versions. Your feedback and bug reports help us improve! 🙏

## 🚀 Quick Start

### New: GUI Tools Available!

- **🔧 [Installer](https://github.com/innermost47/ai-dj/releases)** - One-click setup _(Windows .exe)_
- **🎛️ Server GUI** - Control panel for server management

**Cross-Platform Alternative:**

- **Mac/Linux users:** Run the tools directly with Python:
  ```bash
  python installer.py      # For installation
  python server_interface.py  # For server control
  pyinstaller --onefile --name OBSIDIAN-Neural-Installer --icon=logo.png --noconsole .\installer.py # Build the installer
  pyinstaller --onefile --name OBSIDIAN-Neural-Server --icon=logo.png --noconsole .\server_interface.py # Build the server interface
  ```

## 📢 Community Update

**Thank you!** We've gained **53 stars since June 6th** - the support has been incredible! 🌟

_Your bug reports and feedback help us improve with each release._

> ✨ OBSIDIAN is 100% free and open source. All contributions, builds, and community use are welcome — but for the best experience, get it from the [official GitHub](https://github.com/innermost47/ai-dj).

> 📰 **Press coverage moved to [PRESS.md](PRESS.md)**

---

![OBSIDIAN-Neural Interface](./screenshot.png)

## 🎵 Live Demo

[![OBSIDIAN Live Performance](https://img.youtube.com/vi/l4KMC5adxVA/maxresdefault.jpg)](https://www.youtube.com/watch?v=l4KMC5adxVA)

_Click to watch: World's First AI Jam Partner in action!_

---

## 🔮 Key Features

### 🤖 **Intelligent AI Generation**

- **LLM Brain**: Analyzes sessions and generates smart prompts
- **Stable Audio Open**: High-quality electronic music generation with DAW sample rate sync
- **Real-time**: Everything happens live for performance use
- **SoundTouch Integration**: Automatically syncs generated loops to current DAW tempo

### 🎹 **Multi-Track Sampler**

- **8 independent tracks** with MIDI triggering (C3-B3)
- **Advanced waveform editor** with zoom, precise loop points, and drag & drop to DAW
- **Loop markers**: Set preferred loop sections for sequencer playback
- **Smart time-stretching** with DAW tempo sync
- **Individual outputs** for separate mixing

### 🥁 **Built-in Step Sequencer**

- **16-step programmable sequencer** per track with multi-measure support
- **DAW sync**: Perfectly locked to host tempo and transport
- **Start/Stop on measure boundaries** for seamless live performance
- **Real-time pattern editing** while sequencer is playing
- **Loop marker integration**: Sequence specific sections of your generated audio

### 🎛️ **Live Performance Ready**

- **Global custom prompts** shared across projects
- **MIDI Learn system** for hardware controller mapping
- **Session management** with save/load
- **Background processing** - works with VST window closed
- **Automatic sample rate detection** from DAW for perfect audio quality

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

## 🌐 More from innermost47

**🎵 Music & Creative Coding:**

- **[Unexpected Records](https://unexpected.anthony-charretier.fr/)** - Mobile recording studio & web creation for musicians
- **[YouTube Channel](https://www.youtube.com/@innermost9675)** - Original music compositions
- **[Randomizer](https://randomizer.anthony-charretier.fr/)** - Generative music studio in pure HTML/CSS/JS

**🤖 AI Art Projects:**

- **[AutoGenius Daily](https://autogenius.anthony-charretier.fr/)** - Automated media platform with AI personas
- **[Echoes of Extinction](https://echoes-of-extinction.anthony-charretier.fr/)** - 30-day immersive AI takeover installation
- **[AI Harmony Radio](https://autogenius.anthony-charretier.fr/webradio)** - 24/7 experimental webradio: original music + AI personas & stories

**💭 Digital Philosophy:**

- **[Manifesto for a Creative & Free Web](https://manifeste.chroniquesquantique.com/)** - Call for creative web experimentation

---

**OBSIDIAN-Neural** - Where artificial intelligence meets live music performance.

_Developed by innermost47_
