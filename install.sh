#!/bin/bash

echo "🎧 DJ-IA VST - Complete Installation"
echo "====================================="

# Check prerequisites
echo "🔍 Checking prerequisites..."
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 not found. Please install Python 3.8+"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Please install CMake 3.15+"
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo "❌ Git not found. Please install Git"
    exit 1
fi

# Check CUDA
echo "🔥 Checking CUDA support..."
if command -v nvidia-smi &> /dev/null; then
    echo "✅ NVIDIA GPU detected:"
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits | head -1
    CUDA_AVAILABLE=true
else
    echo "⚠️  No NVIDIA GPU detected - will use CPU (much slower)"
    echo "   For real-time performance, CUDA is highly recommended"
    read -p "   Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled."
        exit 1
    fi
    CUDA_AVAILABLE=false
fi

echo "✅ Prerequisites OK"

# Create virtual environment
echo ""
echo "📦 Creating Python environment..."
python3 -m venv env
source env/bin/activate
echo "✅ Virtual environment created"

# Install dependencies
echo ""
echo "⬇️  Installing Python dependencies..."
if [ "$CUDA_AVAILABLE" = true ]; then
    echo "   🔥 Installing PyTorch with CUDA support..."
    pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
else
    echo "   💻 Installing PyTorch for CPU..."
    pip install torch torchvision torchaudio
fi
echo "   🎵 Installing audio processing libraries..."
pip install stable-audio-tools
pip install llama-cpp-python librosa soundfile
echo "   🌐 Installing web server dependencies..."
pip install fastapi uvicorn python-dotenv requests
echo "✅ Dependencies installed"

# Download model
echo ""
echo "🤖 Downloading AI model..."
echo "   📥 Gemma-3-4B (2.49 GB) - optimized for music generation"
mkdir -p models
if wget -O models/gemma-3-4b-it.gguf "https://huggingface.co/unsloth/gemma-3-4b-it-GGUF/resolve/main/gemma-3-4b-it-Q4_K_M.gguf?download=true"; then
    echo "✅ AI model downloaded successfully"
else
    echo "❌ Model download failed. Check your internet connection."
    exit 1
fi

# Create config
echo ""
echo "⚙️  Creating configuration..."
cat > .env << EOF
# DJ-IA Configuration
DJ_IA_API_KEYS=api keys separated by commas
LLM_MODEL_PATH=$(pwd)/models/gemma-3-4b-it.gguf
AUDIO_MODEL=stable-audio-open
ENVIRONMENT=dev
HOST=0.0.0.0
PORT=8000
EOF
echo "✅ Configuration file created"

# Build VST
echo ""
echo "🔨 Building VST plugin..."
echo "   📥 Downloading JUCE framework..."
cd vst
if [ ! -d "JUCE" ]; then
    git clone https://github.com/juce-framework/JUCE.git --depth 1
fi

echo "   📥 Downloading SoundTouch library..."
if [ ! -d "soundtouch" ]; then
    git clone https://codeberg.org/soundtouch/soundtouch.git --depth 1
fi

echo "   🔨 Compiling VST plugin..."
mkdir -p build
cd build
if cmake .. && cmake --build . --config Release; then
    echo "✅ VST plugin built successfully"
    echo "   📁 Copy the VST from vst/build/ to your VST folder"
else
    echo "❌ VST build failed. Check CMake configuration."
fi
cd ../../

# Create start script
echo ""
echo "📝 Creating start script..."
cat > start.sh << 'EOF'
#!/bin/bash
echo "🚀 Starting DJ-IA Server..."
source env/bin/activate
python main.py --host 0.0.0.0 --port 8000
EOF
chmod +x start.sh
echo "✅ Start script created"

echo ""
echo "🎉 Installation complete!"
echo "=================================="
echo "📋 Next steps:"
echo "   1. Copy the VST plugin to your DAW's VST folder"
echo "   2. Run: ./start.sh"
echo "   3. Load DJ-IA VST in your DAW"
echo "   4. Set server URL to: http://localhost:8000"
echo ""
echo "🚀 Ready to create AI music!"