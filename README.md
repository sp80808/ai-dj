# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

**📖 [Read the full story](https://medium.com/@innermost47/obsidian-neural-when-ai-becomes-your-jam-partner-5203726a3840) - Why I built an AI that jams with you in real-time**

---

## 🚀 **MAJOR UPDATE: Local Models Now Available!**

**🎉 The current release runs completely offline with local TensorFlow Lite models!**

**⚠️ IMPORTANT: The current release is built from the `localModels` branch, NOT this main branch.**

Special thanks to [@fcaspe](https://github.com/fcaspe) for introducing me to audiogen and putting me on this path! This completely solves OBSIDIAN-Neural's main pain point: server dependencies and resource requirements.

### 📦 **Current Release v0.7.0-alpha (localModels branch)**

**No more servers, Python, or GPU requirements!**

#### What You'll Need:

1. **Get Stability AI access**: [Request access](https://huggingface.co/stabilityai/stable-audio-open-small) to Stable Audio Open on Hugging Face
2. **Download models**: Get all files from [innermost47/stable-audio-open-small-tflite](https://huggingface.co/innermost47/stable-audio-open-small-tflite)
3. **Simple setup**: Copy models to `%APPDATA%\OBSIDIAN-Neural\stable-audio\`
4. **Launch**: Choose "Local Model" option

**✅ Benefits:** Completely offline, no GPU server, no Python installation, instant generation  
**⚠️ Requirements:** 16GB+ RAM recommended, Windows only initially

### ⚠️ **Current Limitations (v0.7.0-alpha)**

**The TensorFlow Lite models have some quality trade-offs:**

- **Timing issues**: Generated rhythms may not be perfectly quantized
- **Quality reduction**: TFLite quantization affects audio precision
- **High RAM usage**: Expect significant memory consumption during generation

**For live performance and production use, the server-based approach still provides better quality and timing precision.**

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

## 🎯 **Current Options (main branch)**

### 🚀 **Option 1: Beta Testing (RECOMMENDED)**

**Get FREE API access - No setup required!**

- Contact me for a free API key (**only 10 slots available**)
- Download VST3 from [Releases](https://github.com/innermost47/ai-dj/releases)
- **Contact:** b03caa1n5@mozmail.com

### 🔧 **Option 2: Self-Hosting**

- Download the installer from [Releases](https://github.com/innermost47/ai-dj/releases)
- Requires NVIDIA GPU with 8GB+ VRAM
- Complex setup with Python, CUDA, etc.

---

### 🔴 Latest Live Stream

[![OBSIDIAN Live Stream](https://img.youtube.com/vi/O5j6xa_9_0s/maxresdefault.jpg)](https://www.youtube.com/watch?v=O5j6xa_9_0s)

_57 minutes of real-time AI music generation!_

---

## 📰 **Press coverage moved to [PRESS.md](PRESS.md)**

---

## 🔮 Key Features

- **🤖 Intelligent AI Generation**: LLM brain analyzes sessions and generates smart prompts
- **🎹 8-Track Sampler**: MIDI triggering (C3-B3) with advanced waveform editor
- **🥁 Built-in Step Sequencer**: 16-step programmable sequencer per track
- **🎛️ Live Performance Ready**: MIDI Learn, session save/load, DAW sync

---

## 🐛 Bug Reports & Feedback

**Found issues?** [Create GitHub Issue](https://github.com/innermost47/ai-dj/issues/new)

Include: DAW name/version, OS, steps to reproduce, expected vs actual behavior

---

## 📈 Project Status

🚀 **Next Release**: Local models from `localModels` branch - game changer!  
✅ **Current**: Functional but requires server setup  
⚠️ **Pre-release**: Active development, frequent updates  
🌟 **Star count**: 60+ - Thank you for the support!

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
