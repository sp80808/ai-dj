cd ai-dj
echo "======= LOCAL ENVIRONMENT ======="
echo "OS Info:"
systeminfo | findstr /B /C:"OS Name" /C:"OS Version" /C:"System Type"

echo -e "\n======= PYTHON ENVIRONMENT ======="
python --version
python -c "import sys; print(f'Python executable: {sys.executable}')"
python -c "import platform; print(f'Platform: {platform.platform()}')"

echo -e "\n======= GCC ENVIRONMENT ======="
gcc --version
where gcc
gcc -dumpmachine

echo -e "\n======= PYINSTALLER VERSION ======="
pyinstaller --version

echo -e "\n======= BUILD TEST ======="
# Test compilation simple
echo "print('test')" > test.py
pyinstaller --onefile test.py
sha256sum dist/test.exe
rm -rf build dist test.spec test.py