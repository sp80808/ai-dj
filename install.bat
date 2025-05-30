@echo off
echo DJ-IA VST - Complete Installation
echo =====================================

REM Check prerequisites
echo Checking prerequisites...
python --version >nul 2>&1
if errorlevel 1 (
    echo Python not found. Please install Python 3.10
    pause
    exit /b 1
)

cmake --version >nul 2>&1
if errorlevel 1 (
    echo CMake not found. Please install CMake 4.0.2
    pause
    exit /b 1
)

git --version >nul 2>&1
if errorlevel 1 (
    echo Git not found. Please install Git
    pause
    exit /b 1
)

REM Check CUDA
echo Checking CUDA support...
nvidia-smi >nul 2>&1
if errorlevel 1 (
    echo No NVIDIA GPU detected - will use CPU ^(much slower^)
    echo For real-time performance, CUDA is highly recommended
    set /p "choice=Continue anyway? (y/N): "
    if /i not "%choice%"=="y" (
        echo Installation cancelled.
        pause
        exit /b 1
    )
    set CUDA_AVAILABLE=false
) else (
    echo NVIDIA GPU detected:
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits | findstr /n "^" | findstr "^1:"
    set CUDA_AVAILABLE=true
)
echo Prerequisites OK

REM Create virtual environment
echo.
echo Creating Python environment...
python -m venv env
call env\Scripts\activate.bat
echo Virtual environment created

REM Install dependencies
echo.
echo Installing Python dependencies...
if "%CUDA_AVAILABLE%"=="true" (
    echo Installing PyTorch with CUDA support...
    pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
) else (
    echo Installing PyTorch for CPU...
    pip install torch torchvision torchaudio
)
echo Installing audio processing libraries...
pip install stable-audio-tools
pip install llama-cpp-python librosa soundfile
echo Installing web server dependencies...
pip install fastapi uvicorn python-dotenv requests
echo Dependencies installed

REM Download model
echo.
echo Downloading AI model...
echo Gemma-3-4B ^(2.49 GB^) - optimized for music generation
curl.exe -L -o models\gemma-3-4b-it.gguf "https://huggingface.co/unsloth/gemma-3-4b-it-GGUF/resolve/main/gemma-3-4b-it-Q4_K_M.gguf?download=true"
if errorlevel 1 (
    echo Model download failed. Check your internet connection.
    pause
    exit /b 1
)
echo AI model downloaded successfully

REM Create config
echo.
echo Creating configuration...
(
echo # DJ-IA Configuration
echo DJ_IA_API_KEYS=api keys separated by commas
echo LLM_MODEL_PATH=%cd%\models\gemma-3-4b-it.gguf
echo AUDIO_MODEL=stable-audio-open
echo ENVIRONMENT=dev
echo HOST=127.0.0.1
echo PORT=8000
) > .env
echo Configuration file created

REM Build VST
echo.
echo Building VST plugin...
echo Downloading JUCE framework...
cd vst
if not exist "JUCE" (
    git clone https://github.com/juce-framework/JUCE.git --depth 1
)

echo Downloading SoundTouch library...
if not exist "soundtouch" (
    git clone https://codeberg.org/soundtouch/soundtouch.git --depth 1
)

echo Compiling VST plugin...
mkdir build 2>nul
cd build
cmake .. && cmake --build . --config Release
if errorlevel 1 (
    echo VST build failed. Check CMake configuration.
) else (
    echo VST plugin built successfully
    echo Copy the VST from vst\build\ to your VST folder
)
cd ..\..\

REM Create start script
echo.
echo Creating start script...
(
echo @echo off
echo echo Starting DJ-IA Server...
echo call env\Scripts\activate.bat
echo python main.py --host 0.0.0.0 --port 8000
) > start.bat
echo Start script created

echo.
echo Installation complete!
echo ==================================
echo Next steps:
echo    1. Copy the VST plugin to your DAW's VST folder
echo    2. Run: start.bat
echo    3. Load DJ-IA VST in your DAW
echo    4. Set server URL to: http://localhost:8000
echo.
echo Ready to jam!
pause