#!/bin/bash
# ============================================================
# QSSTV Windows build script for MSYS2 MinGW-w64
# ============================================================
# Run this from an MSYS2 MinGW64 shell (NOT the MSYS shell!)
# Start: C:\msys64\mingw64.exe
# ============================================================

set -e

echo "=== Step 1: Install dependencies ==="
pacman -S --needed --noconfirm \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-make \
  mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-qt5-base \
  mingw-w64-x86_64-qt5-serialport \
  mingw-w64-x86_64-portaudio \
  mingw-w64-x86_64-fftw \
  mingw-w64-x86_64-openjpeg2 \
  mingw-w64-x86_64-libavif \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-autotools \
  git

echo ""
echo "=== Step 2: Build hamlib from source ==="
if [ ! -d "/tmp/hamlib-build" ]; then
  mkdir -p /tmp/hamlib-build
  cd /tmp/hamlib-build
  # Download hamlib 4.5.5 (or latest)
  HAMLIB_VER="4.5.5"
  if [ ! -f "hamlib-${HAMLIB_VER}.tar.gz" ]; then
    echo "Downloading hamlib ${HAMLIB_VER}..."
    curl -L -o "hamlib-${HAMLIB_VER}.tar.gz" \
      "https://github.com/Hamlib/Hamlib/releases/download/${HAMLIB_VER}/hamlib-${HAMLIB_VER}.tar.gz"
  fi
  tar xzf "hamlib-${HAMLIB_VER}.tar.gz"
  cd "hamlib-${HAMLIB_VER}"
  ./configure --prefix=/mingw64 --disable-static --enable-shared
  make -j$(nproc)
  make install
  echo "hamlib installed to /mingw64"
else
  echo "hamlib build dir exists, skipping (delete /tmp/hamlib-build to rebuild)"
fi

echo ""
echo "=== Step 3: Build QSSTV ==="
cd "$(dirname "$0")/src"
qmake qsstv.pro
mingw32-make -j$(nproc)

echo ""
echo "=== Build complete! ==="
echo "Binary: src/release/qsstv.exe (or src/qsstv.exe)"
echo ""
echo "=== Step 4: To create a deployable package, run: ==="
echo "  windeployqt qsstv.exe"
echo "  (this copies all required Qt DLLs next to the exe)"
