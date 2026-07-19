@echo off
REM ─────────────────────────────────────────────────────────────────────────────
REM  Nothing Browser — Windows Build Script
REM  Requirements:
REM    - Qt6 installed via Qt Online Installer (https://www.qt.io/download)
REM    - CMake (https://cmake.org/download/)
REM    - Visual Studio 2022 with C++ workload
REM
REM  Usage: run this from the repo root in a Developer Command Prompt
REM ─────────────────────────────────────────────────────────────────────────────

setlocal EnableDelayedExpansion

set VERSION=0.1.0
set BUILD_DIR=build-windows
set DIST_DIR=dist

REM ── Locate Qt6 ──────────────────────────────────────────────────────────────
REM Adjust this path to where YOUR Qt6 is installed
set QT6_DIR=C:\Qt\6.7.0\msvc2019_64

if not exist "%QT6_DIR%" (
    echo [!] Qt6 not found at %QT6_DIR%
    echo     Edit QT6_DIR in this script to match your Qt installation path.
    echo     Download Qt: https://www.qt.io/download-open-source
    pause
    exit /b 1
)

echo [*] Nothing Browser v%VERSION% — Windows Build
echo [*] Qt6 path: %QT6_DIR%
echo.

REM ── Create build dir ─────────────────────────────────────────────────────────
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM ── CMake configure ──────────────────────────────────────────────────────────
echo [*] Configuring with CMake...
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT6_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A x64

if errorlevel 1 (
    echo [!] CMake configuration failed.
    cd ..
    pause
    exit /b 1
)

REM ── Build ────────────────────────────────────────────────────────────────────
echo [*] Building...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo [!] Build failed.
    cd ..
    pause
    exit /b 1
)

cd ..

REM ── Package with windeployqt ──────────────────────────────────────────────────
echo [*] Packaging with windeployqt...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

set EXE=%BUILD_DIR%\Release\nothing-browser.exe
set DEPLOY_DIR=%DIST_DIR%\nothing-browser-win64

if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%"
copy "%EXE%" "%DEPLOY_DIR%\nothing-browser.exe"

REM Run windeployqt to copy Qt DLLs
"%QT6_DIR%\bin\windeployqt.exe" ^
    --release ^
    --no-compiler-runtime ^
    --webengine ^
    "%DEPLOY_DIR%\nothing-browser.exe"

echo.
echo [✓] Build complete.
echo     Output: %DEPLOY_DIR%\nothing-browser.exe
echo.
echo     To distribute: zip the entire %DEPLOY_DIR% folder.
echo     Users run nothing-browser.exe directly — no install needed.
pause