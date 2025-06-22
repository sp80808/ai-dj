# OBSIDIAN-Neural

🎵 **Real-time AI music generation VST3 plugin for live performance**

## 🚀 **MAJOR UPDATE: Local Model Support!**

**🎉 No more server headaches! Run completely offline with local TensorFlow Lite models**

Special thanks to [@fcaspe](https://github.com/fcaspe) for introducing me to audiogen and putting me on this path! This addresses the main pain point of OBSIDIAN-Neural: server dependencies and resource requirements.

### 📦 **Local Model Setup (Windows)**

**The easiest way to use OBSIDIAN-Neural - completely offline!**

#### Prerequisites:

1. **Get Stability AI access**: [Request access](https://huggingface.co/stabilityai/stable-audio-open-small) to Stable Audio Open on Hugging Face
2. **Wait for approval**: Usually takes a few minutes to a few hours

#### Setup:

1. **Download complete package**: Once approved, get all files from [innermost47/stable-audio-open-small-tflite](https://huggingface.co/innermost47/stable-audio-open-small-tflite)
2. **Create folder**: `%APPDATA%\OBSIDIAN-Neural\stable-audio\`
3. **Copy model files**: Place all `.tflite` and `.model` files in the folder
4. **Copy executable**: Place `audiogen.exe` in the same folder
5. **Download VST3**: Get latest from [Releases](https://github.com/innermost47/ai-dj/releases)
6. **Launch**: Choose "Local Model (Basic - requires manual setup)"

**✅ Benefits:** No GPU server, no Python, no network dependency  
**⚠️ Requirements:** 16GB+ RAM recommended, Windows only for now

### ⚠️ **Current Limitations (v0.7.0-alpha)**

**The TensorFlow Lite models have some quality trade-offs:**

- **Timing issues**: Generated rhythms may not be perfectly quantized
- **Quality reduction**: TFLite quantization affects audio precision
- **High RAM usage**: Expect significant memory consumption during generation

**For live performance and production use, the server-based approach still provides better quality and timing precision.**

---

**📖 [Read the full story](https://medium.com/@innermost47/obsidian-neural-when-ai-becomes-your-jam-partner-5203726a3840) - Why I built an AI that jams with you in real-time**

---

## 🎯 What OBSIDIAN-Neural Actually Does

**Think of it as having an AI jam partner directly in your DAW.**

- **Type simple keywords** → AI generates musical loops instantly
- **Real-time generation** → No stopping your creative flow
- **8-track sampler** → Layer AI-generated elements like drums, bass, pads
- **Built-in sequencer** → Trigger patterns and build arrangements live
- **MIDI triggering** → Play AI samples from your keyboard (C3-B3)
- **Perfect DAW sync** → Everything locks to your project tempo

**Example workflow:**

1. Type "dark techno kick" → AI generates a techno kick loop
2. Type "acid bassline" → AI adds a 303-style bass
3. Trigger both with MIDI keys while jamming on hardware synths
4. Build patterns with the step sequencer

**It's like having a TB-303, but instead of tweaking knobs, you describe what you want in plain English.**

--

### 🔴 Latest Live Stream

[![OBSIDIAN Live Stream](https://img.youtube.com/vi/O5j6xa_9_0s/maxresdefault.jpg)](https://www.youtube.com/watch?v=O5j6xa_9_0s)

_57 minutes of real-time AI music generation with all the unpredictability!_

---

## 📰 **Press coverage moved to [PRESS.md](PRESS.md)**

---

![OBSIDIAN-Neural Interface](./screenshot.png)

---

## 🔮 Key Features

- **🤖 Intelligent AI Generation**: LLM brain analyzes sessions and generates smart prompts
- **🎹 8-Track Sampler**: MIDI triggering (C3-B3) with advanced waveform editor
- **🥁 Built-in Step Sequencer**: 16-step programmable sequencer per track
- **🎛️ Live Performance Ready**: MIDI Learn, session save/load, DAW sync

---

## 🎯 Usage Workflow

1. **Setup**: Download models, load OBSIDIAN-Neural in DAW
2. **Generate**: Add tracks, enter creative prompts, generate AI audio
3. **Perform**: Trigger tracks with MIDI, auto-sync to DAW tempo
4. **Control**: Map hardware controllers with MIDI Learn

**Example LLM-generated prompt:**
_"Deep techno kick with sidechain compression, 126 BPM, dark atmosphere, minimal hi-hats, rolling bassline"_

---

## 🐛 Bug Reports & Feedback

**Found issues?** [Create GitHub Issue](https://github.com/innermost47/ai-dj/issues/new)

Include: DAW name/version, OS, steps to reproduce, expected vs actual behavior

---

## 📈 Project Status

✅ **Local models**: No more server complexity!  
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
