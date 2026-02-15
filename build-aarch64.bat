@echo off
setlocal

set "WSL_DISTRO=Ubuntu"
set "WSL_PROJECT=/mnt/d/Antigravity/Repo/test/awg-manager-alpha"

echo ============================================
echo  awg-manager-alpha: aarch64 build via WSL
echo ============================================
echo.

wsl -d %WSL_DISTRO% bash -lc "cd '%WSL_PROJECT%' && ./scripts/build_aarch64_wsl.sh aarch64-3.10 0.1.0-1"
if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

echo.
echo Done. Package should be in dist\
exit /b 0
