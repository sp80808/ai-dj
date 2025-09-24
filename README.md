# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

📄 **[Late Breaking Paper - AIMLA 2025](https://drive.google.com/file/d/1cwqmrV0_qC462LLQgQUz-5Cd422gL-8F/view)** - Presented at the first AES International Conference on Artificial Intelligence and Machine Learning for Audio (Queen Mary University London, Sept 8-10, 2025)  
🎓 **[Tutorial](https://youtu.be/-qdFo_PcKoY)** - From DAW setup to live performance (French + English subtitles)

---

## What it does

Type words → Get musical loops instantly. No stopping your creative flow.

- **8-track sampler** with MIDI triggering (C3-B3)
- **4 pages per track** (A/B/C/D) - Switch variations instantly by clicking page buttons
- **Perfect DAW sync** - Auto time-stretch to project tempo
- **Real-time generation** - No pre-recorded samples
- **Stems separation** - Isolated drums, bass, vocals

**Example:** Type "dark techno kick" → AI generates techno loop → Trigger with MIDI while jamming

---

## Quick Start

### 🔵 Server + GPU (Recommended)

**Best quality for live performance and production.**

1. Get [Stability AI access](https://huggingface.co/stabilityai/stable-audio-open-1.0)
2. Follow [build from source instructions](INSTALLATION.md#option-3-build-from-source)
3. Run server interface: `python server_interface.py`
4. Download VST3 from [Releases](https://github.com/innermost47/ai-dj/releases)
5. Configure VST with server URL and API key

**Benefits:** Variable duration, STEMS separation, better timing, GPU acceleration

### 🟢 Local Models (Offline)

**Runs completely offline. No servers, Python, or GPU needed.**

1. Get [Stability AI access](https://huggingface.co/stabilityai/stable-audio-open-small)
2. Download models from [innermost47/stable-audio-open-small-tflite](https://huggingface.co/innermost47/stable-audio-open-small-tflite)
3. Copy to `%APPDATA%\OBSIDIAN-Neural\stable-audio\`
4. Download VST3 from [Releases](https://github.com/innermost47/ai-dj/releases)
5. Choose "Local Model" in plugin

**Requirements:** 16GB+ RAM, Windows (macOS/Linux coming soon)

### 🔴 Free API (No Setup)

Contact b03caa1n5@mozmail.com for free API key (10 slots available)

---

## Community

**🎯 Share your jams!** I'm the only one posting OBSIDIAN videos so far. Show me how YOU use it!

📧 **Email:** b03caa1n5@mozmail.com  
💬 **Discussions:** [GitHub Discussions](https://github.com/innermost47/ai-dj/discussions)  
📺 **Examples:** [Community Sessions](YOUTUBE.md)

[![Jungle Session](https://img.youtube.com/vi/cFmRJIFUOCU/maxresdefault.jpg)](https://youtu.be/cFmRJIFUOCU)

---

## Download

**VST3 Plugin:**

- [Windows](https://github.com/innermost47/ai-dj/releases)
- [macOS](https://github.com/innermost47/ai-dj/releases)
- [Linux](https://github.com/innermost47/ai-dj/releases)

**Install to:**

- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Linux: `~/.vst3/`

---

## Status & Support

🚀 **Active development** - Updates pushed regularly  
⭐ **110+ GitHub stars** - Thanks for the support!  
🐛 **Issues:** [Report bugs here](https://github.com/innermost47/ai-dj/issues/new)

**Current limitations (local mode):**

- Fixed 10-second generation
- Some timing/quantization issues
- High RAM usage
- No STEMS separation yet

Server mode still provides better quality for live performance.

---

## License

**Dual licensed:**

- 🆓 **GNU Affero General Public License v3.0** (Open source)
- 💼 **Commercial license** available (Contact: b03caa1n5@mozmail.com)

---

## More Projects

🎵 **[YouTube](https://www.youtube.com/@innermost9675)** - Original compositions  
🎙️ **[AI Harmony Radio](https://autogenius.anthony-charretier.fr/webradio)** - 24/7 experimental radio  
🎛️ **[Randomizer](https://randomizer.anthony-charretier.fr/)** - Generative music studio

---

**OBSIDIAN-Neural** - Where artificial intelligence meets live music performance.

_Developed by InnerMost47_
