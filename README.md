# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

OBSIDIAN-Neural transforms AI into a live music instrument using intelligent LLM prompts and Stable Audio Open to create contextually aware loops directly in your DAW.

**📖 [Read the full story](https://medium.com/@innermost47/obsidian-neural-when-ai-becomes-your-jam-partner-5203726a3840) - Why I built an AI that jams with you in real-time**

---

## 🚀 **MAJOR UPDATE: Local Models Now Available!**

**🎉 The current release runs completely offline with local TensorFlow Lite models!**

Special thanks to [@fcaspe](https://github.com/fcaspe) for introducing me to audiogen and putting me on this path! This completely solves OBSIDIAN-Neural's main pain point: server dependencies and resource requirements.

### 📦 **Current Release**

**No more servers, Python, or GPU requirements!**

#### What You'll Need:

1. **Get Stability AI access**: [Request access](https://huggingface.co/stabilityai/stable-audio-open-small) to Stable Audio Open on Hugging Face
2. **Download models**: Get all files from [innermost47/stable-audio-open-small-tflite](https://huggingface.co/innermost47/stable-audio-open-small-tflite)
3. **Simple setup**: Copy models to `%APPDATA%\OBSIDIAN-Neural\stable-audio\`
4. **Launch**: Choose "Local Model" option

**✅ Benefits:** Completely offline, no GPU server, no Python installation, instant generation  
**⚠️ Requirements:** 16GB+ RAM recommended, Windows only initially

### ⚠️ **Current Limitations**

**The TensorFlow Lite models have some quality trade-offs:**

- **Timing issues**: Generated rhythms may not be perfectly quantized
- **Quality reduction**: TFLite quantization affects audio precision
- **High RAM usage**: Expect significant memory consumption during generation
- **Fixed duration**: Audio generation locked to 10 seconds (audiogen/TFLite limitation)
- **No STEMS separation**: DEMUCS stem separation not available in local mode

**For live performance and production use, the server-based approach still provides better quality, timing precision, variable duration, and STEMS separation.**

---

## 🎯 **Alternative Options (if local doesn't work for you):**

### 🚀 **Option 1: Beta Testing**

**Get FREE API access - No setup required!**

- Contact me for a free API key (**only 10 slots available**)
- Download VST3 from [Releases](https://github.com/innermost47/ai-dj/releases)
- **Contact:** b03caa1n5@mozmail.com

### 🔧 **Option 2: Windows Installer (Self-Hosting)**

**Complete setup with local server - addresses installation complexity!**

#### **Prerequisites:**

1. **Get Stability AI access**: [Request access](https://huggingface.co/stabilityai/stable-audio-open-1.0) to Stable Audio Open on Hugging Face
2. **Wait for approval**: Usually takes a few minutes to a few hours
3. **Run as Administrator**: The installer requires admin rights for system-wide installations

#### **Installation:**

- Download the [.exe installer](https://github.com/innermost47/ai-dj/releases)
- **Optional**: Download VST3 separately from [Releases](https://github.com/innermost47/ai-dj/releases)
- **Space requirements**: ~20GB total installation

**What the installer automatically installs:**

- **Python 3.10** (if not detected)
- **Git** (for source code management)
- **CMake** (build system for VST compilation)
- **Visual Studio Build Tools** (Windows compiler)
- **CUDA Toolkit** (if NVIDIA GPU detected)
- **PyTorch** (with CUDA/ROCm support based on your GPU)
- **All Python dependencies** (FastAPI, Stable Audio Tools, etc.)

**Installation process (automatic):**

1. **System Detection**: Analyzes your hardware (CPU, RAM, GPU)
2. **Prerequisites Check**: Installs missing components (Python, CMake, Git, etc.)
3. **Source Download**: Clones OBSIDIAN-Neural from GitHub
4. **Python Environment**: Creates isolated virtual environment
5. **AI Model Download**: Downloads Gemma-3-4B model (2.49 GB)
6. **VST Compilation**: Builds VST3 plugin from source (or skip with manual option)
7. **Desktop Shortcuts**: Creates server launcher shortcuts
8. **Performance Benchmark**: Tests your system capabilities (optional)

**VST3 Installation Options:**

- **Automatic**: Let installer build and install VST3
- **Manual**: Download VST3 separately, uncheck "Build VST3" in installer, manually copy to `C:\Program Files\Common Files\VST3\`

**Installation locations:**

- **Main files**: `C:\ProgramData\OBSIDIAN-Neural\`
- **AI models**: `%USERPROFILE%\.huggingface\hub\`
- **VST3 plugin**: `C:\Program Files\Common Files\VST3\`
- **Configuration database**: `%USERPROFILE%\.obsidian_neural\config.db`

**Complete uninstall:**

1. Delete `C:\ProgramData\OBSIDIAN-Neural\`
2. Delete `%USERPROFILE%\.huggingface\hub\`
3. Delete `C:\Program Files\Common Files\VST3\OBSIDIAN-Neural.vst3`
4. Delete `%USERPROFILE%\.obsidian_neural\config.db` (encrypted database)
5. Manually remove Python/CUDA/CMake if no longer needed

**Step-by-step workflow after installation:**

#### 1. **Server Interface Overview**

The desktop shortcut launches a complete control panel with:

**Server Interface Features:**

- **System Tray Support**: Minimize to tray with green triangle icon, control server from tray menu
- **🚀 Server Control Tab**: Real-time status, start/stop/restart buttons, copy-paste server URL
- **⚙️ Configuration Tab**:
  - API Keys management with credit system (🔓 UNLIMITED, ✅ 50/50 credits, ❌ NO CREDITS)
  - Secure Hugging Face token storage with built-in verification
  - Model settings and server configuration
- **📝 Logs Tab**: Real-time server output with color-coded messages
- **First-Time Setup Wizard**: Guided configuration for new installations
- **Auto-save**: All settings saved automatically, encrypted local database

#### 2. **Configure the Server**

- Launch "OBSIDIAN Server" from desktop shortcut
- **First launch**: Setup wizard will guide you through configuration
- **Hugging Face Token**: Enter your approved token (built-in verification available)
- **API Keys**: Generate keys with credit limits or unlimited access

#### 3. **Then: Launch the Server**

- Start the AI server from the GUI interface
- **API Keys prompt will appear**:
  - **"Use stored API keys for authentication?"**
  - **Yes**: Use API authentication (for production/network access)
  - **No**: Development bypass - no auth needed (for localhost/closed network)
- **Server URL**: Will be displayed in the interface (usually `http://localhost:8000`)
- **Copy-paste friendly**: You can select and copy the server URL directly from the GUI

#### 4. **Finally: Configure the VST**

- Load OBSIDIAN-Neural VST3 in your DAW
- **Server URL**: Paste the URL from server GUI (e.g., `http://localhost:8000`)
- **API Key**: Copy from server interface if you chose "Yes" to API authentication
- Choose "Server Mode" in plugin settings

**Troubleshooting:**

- **No admin rights?** Installer must run as Administrator for system installations
- **No Hugging Face access?** You must be approved for Stable Audio Open first
- **VST3 not found?** Check if it was built or copy manually to Common Files
- **CUDA not detected?** Installer automatically detects NVIDIA GPU for CUDA installation
- **API Keys confusion?** Choose "No" for simple localhost setup, "Yes" if opening server to network
- **"Development bypass"**: Perfect for local testing, no authentication needed
- **Can't find server?** Check the desktop shortcut was created during installation
- **URL not working?** Make sure server is running before configuring VST

### 👨‍💻 **Option 3: Manual Installation (Developers)**

- Full source compilation
- Requires Python, CMake, GPU toolchains
- Most flexible but complex setup

---

## 🎯 What OBSIDIAN-Neural Actually Does

**Think of it as having an AI jam partner directly in your DAW.**

- **Type simple keywords** → AI generates musical loops instantly
- **Real-time generation** → No stopping your creative flow
- **8-track sampler** → Layer AI-generated elements like drums, bass, pads
- **MIDI triggering** → Play AI samples from your keyboard (C3-B3)
- **Perfect DAW sync** → Everything locks to your project tempo

**Example workflow:**

1. Type "dark techno kick" → AI generates a techno kick loop
2. Type "acid bassline" → AI adds a 303-style bass
3. Trigger both with MIDI keys while jamming on hardware synths

**It's like having a TB-303, but instead of tweaking knobs, you describe what you want in plain English.**

---

### 🔴 Latest Live Stream

[![OBSIDIAN Live Stream](https://img.youtube.com/vi/O5j6xa_9_0s/maxresdefault.jpg)](https://www.youtube.com/watch?v=O5j6xa_9_0s)

_57 minutes of real-time AI music generation with all the unpredictability!_

---

## 📰 **Press coverage moved to [PRESS.md](PRESS.md)**

---

![OBSIDIAN-Neural Interface](./screenshot.png)

---

## 🐛 Bug Reports & Feedback

**Found issues?** [Create GitHub Issue](https://github.com/innermost47/ai-dj/issues/new)

Include: DAW name/version, OS, steps to reproduce, expected vs actual behavior

---

## 📈 Project Status

🚀 **Local models**: Available now (with some limitations)  
✅ **Server option**: Still the best for live performance  
⚠️ **Pre-release**: Active development, frequent updates  
🌟 **Star count**: 65+ - Thank you for the support!

---

## 📝 License

MIT License - Feel free to modify, but please keep attribution to InnerMost47

---

## 🌐 More Projects

**Music & Creative Coding:**

- **[YouTube Channel](https://www.youtube.com/@innermost9675)** - Original compositions
- **[Unexpected Records](https://unexpected.anthony-charretier.fr/)** - Mobile recording studio
- **[Randomizer](https://randomizer.anthony-charretier.fr/)** - Generative music studio

**AI Art Projects:**

- **[AutoGenius Daily](https://autogenius.anthony-charretier.fr/)** - AI personas platform
- **[AI Harmony Radio](https://autogenius.anthony-charretier.fr/webradio)** - 24/7 experimental radio

---

**OBSIDIAN-Neural** - Where artificial intelligence meets live music performance.

_Developed by InnerMost47_
