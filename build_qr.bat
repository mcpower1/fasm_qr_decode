@echo off
setlocal
cd /d "%~dp0"
fasm.exe qrdecode.asm qrdecode.exe
if errorlevel 1 (
    echo FASM build FAILED.
    exit /b 1
)
echo Built qrdecode.exe
exit /b 0
