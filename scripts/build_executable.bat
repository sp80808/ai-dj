@echo off
setlocal enabledelayedexpansion

echo Installing PyInstaller...
"%~1\env\Scripts\python.exe" -m pip install pyinstaller
if %errorlevel% neq 0 (
    echo Failed to install PyInstaller
    exit /b %errorlevel%
)

echo Building executable...
set LOGO_ARG=
if exist "%~1\logo.png" (
    set LOGO_ARG=--icon "%~1\logo.png"
)

"%~1\env\Scripts\python.exe" -m PyInstaller ^
    --onefile ^
    --noconsole ^
    --name "OBSIDIAN-Neural-Server" ^
    --distpath "%~1" ^
    !LOGO_ARG! ^
    "%~1\server_interface.py"

if %errorlevel% neq 0 (
    echo PyInstaller build failed
    exit /b %errorlevel%
)

echo Build completed successfully!