# QSSTV Windows Build Guide (for Claude Code agent on Windows)

## Mission
Build QSSTV on Windows using MSYS2 MinGW-w64. This is the first ever Windows build - expect compilation issues that need fixing.

## Prerequisites
MSYS2 must be installed (https://www.msys2.org/). All commands below run in the **MSYS2 MinGW x64** shell (NOT "MSYS2 MSYS").

## Step 1: Install dependencies

```bash
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
  mingw-w64-x86_64-autotools \
  git
```

## Step 2: Build hamlib from source

hamlib is NOT available as an MSYS2 package. Build from source:

```bash
mkdir -p /tmp/hamlib-build && cd /tmp/hamlib-build
HAMLIB_VER="4.5.5"
curl -L -o "hamlib-${HAMLIB_VER}.tar.gz" \
  "https://github.com/Hamlib/Hamlib/releases/download/${HAMLIB_VER}/hamlib-${HAMLIB_VER}.tar.gz"
tar xzf "hamlib-${HAMLIB_VER}.tar.gz"
cd "hamlib-${HAMLIB_VER}"
./configure --prefix=/mingw64 --disable-static --enable-shared
make -j$(nproc)
make install
```

Verify: `pkg-config --libs hamlib` should return `-lhamlib`.

If hamlib 4.5.5 doesn't work, try the latest from https://github.com/Hamlib/Hamlib/releases

## Step 3: Build QSSTV

```bash
cd /path/to/QSSTV/src
qmake qsstv.pro
mingw32-make -j$(nproc)
```

### Expected issues and how to fix them

#### pkg-config not finding libraries
If qmake fails with pkg-config errors, check:
```bash
pkg-config --list-all | grep -i portaudio
pkg-config --list-all | grep -i hamlib
pkg-config --list-all | grep -i fftw
pkg-config --list-all | grep -i openjp2
pkg-config --list-all | grep -i libavif
```
The `.pro` file uses these PKGCONFIG names for win32:
- `portaudio-2` (might be `portaudio` - check with pkg-config)
- `hamlib`
- `fftw3`
- `libopenjp2`
- `libavif`

If a name doesn't match, edit the `win32 { PKGCONFIG += ... }` block in `src/qsstv.pro`.

#### Missing headers
Some headers may need Windows-specific guards. Common patterns:
- `#include <unistd.h>` -> already guarded in `dispatchevents.h` (provides `usleep` macro on Windows)
- `#include <sys/time.h>` -> already removed from `soundbase.cpp`
- If you see other POSIX includes, guard them with `#ifndef Q_OS_WIN`

#### hamlib API differences
The hamlib struct layout may differ between versions. If you get errors about `my_rig->state.rigport`, check the hamlib version - newer versions changed `rigport` to `rigport_deprecated` and use accessor functions instead. Fix in `src/rig/rigcontrol.cpp`.

#### Linker errors about missing symbols
If you get undefined references:
- For PortAudio symbols: check `pkg-config --libs portaudio-2`
- For hamlib symbols: check `pkg-config --libs hamlib`
- For fftw: the project needs both `-lfftw3` and `-lfftw3f` (float version)

#### V4L2 / ALSA accidentally included
The `.pro` file uses `unix:!macx:` to guard Linux-only sources. If V4L2 or ALSA sources are being compiled on Windows, something is wrong with the qmake platform detection. Verify with `qmake -query QMAKE_SPEC`.

## Step 4: Create distributable package

```bash
mkdir -p package
cp src/release/qsstv.exe package/   # or src/qsstv.exe
cd package

# Copy Qt DLLs automatically
windeployqt qsstv.exe

# Manually copy non-Qt DLLs from /mingw64/bin/
cp /mingw64/bin/libportaudio.dll .
cp /mingw64/bin/libhamlib*.dll .
cp /mingw64/bin/libfftw3*.dll .
cp /mingw64/bin/libopenjp2*.dll .
cp /mingw64/bin/libavif*.dll .
# Also copy any transitive dependencies (gcc runtime, etc.)
cp /mingw64/bin/libgcc_s_seh-1.dll .
cp /mingw64/bin/libstdc++-6.dll .
cp /mingw64/bin/libwinpthread-1.dll .
```

Test by running `qsstv.exe` from the package directory. If it fails to start, use `ldd qsstv.exe` to find missing DLLs.

## Architecture notes

### Sound system
- `soundBase` (abstract, QThread) defines the interface: `init()`, `read()`, `write()`, `flushCapture()`, `flushPlayback()`, `closeDevices()`
- On Windows: `soundPortAudio` is used (see `src/sound/soundportaudio.cpp`)
- Capture: mono S16LE at 48000 Hz, reads blocks of DOWNSAMPLESIZE=4096 samples
- Playback: stereo S16LE at 48000 Hz, writes blocks of DOWNSAMPLESIZE samples
- The audio thread polls with `Pa_GetStreamReadAvailable()` then does blocking `Pa_ReadStream()`

### Serial PTT
- Uses `QSerialPort` (cross-platform) for DTR/RTS control
- Port names on Windows: `COM1`, `COM3`, etc. (defaults set in `rigconfig.cpp`)

### Video capture
- Excluded on Windows (same as macOS) - V4L2 is Linux-only
- AVIF encoding/decoding is preserved (libavif is cross-platform)

### Key files for Windows-specific code
- `src/qsstv.pro` - build system, `win32 { }` blocks
- `src/sound/soundportaudio.h/cpp` - Windows audio backend
- `src/mainwindow.cpp` - backend selection (`#ifdef Q_OS_WIN`)
- `src/config/soundconfig.cpp` - audio device enumeration
- `src/config/rigconfig.cpp` - COM port defaults
- `src/rig/rigcontrol.cpp` - serial PTT via QSerialPort
- `src/dispatch/dispatchevents.h` - usleep compatibility macro
