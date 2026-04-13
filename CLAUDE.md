# QSSTV - Claude Code Project Context

## What is QSSTV
Qt5 C++ application for SSTV (Slow Scan Television) and DRM (Digital Radio Mondiale) image transmission over amateur radio. Receives and transmits images via sound card, controls radios via serial port (hamlib).

## Project structure
- `src/` - all source code, `qsstv.pro` is the qmake project file
- `src/sound/` - audio backends: `soundbase.h` (abstract), `soundpulse` (Linux/Mac), `soundalsa` (Linux), `soundportaudio` (Windows)
- `src/rig/` - radio control via hamlib + serial PTT via QSerialPort
- `src/drmrx/` / `src/drmtx/` - DRM receive/transmit (OFDM, LDPC, AVIF)
- `src/sstv/` - SSTV modes
- `src/config/` - configuration dialogs
- `src/mainwidgets/` - main UI (txwidget, rxwidget)

## Current work in progress

### LDPC + AVIF (replacing Viterbi + JP2)
- TX side: LDPC encoder + AVIF image codec implemented, UI controls for FEC mode and LDPC rate added in txwidget
- RX side: LDPC decoder implemented, status display shows LDPC rate
- Profiles: drmprofileconfig supports fecMode and ldpcRate

### Windows port (started 2026-04-13)
See `WINDOWS-BUILD.md` for detailed instructions for the Windows build agent.

**What's done:**
- `soundportaudio.h/cpp` - PortAudio backend implementing soundBase interface
- `.pro` file has `win32:` blocks for PortAudio, no V4L2, no ALSA
- `mainwindow.cpp` / `soundconfig.cpp` select PortAudio on Windows
- Serial PTT uses QSerialPort (cross-platform, replaces POSIX ioctl)
- COM port defaults on Windows in rigconfig.cpp
- POSIX includes replaced with Qt equivalents (QElapsedTimer, QThread::usleep)
- `QT += serialport` added
- **Linux build verified clean** - no regressions

**What's NOT done:**
- Windows build has never been tested yet
- hamlib must be compiled from source on MSYS2 (no pacman package)
- windeployqt packaging not yet tested

## Build instructions

### Linux
```bash
cd src && qmake && make -j$(nproc)
```

### Windows (MSYS2 MinGW64)
See `WINDOWS-BUILD.md`

## Conventions
- Qt5 / C++11
- 2-space indentation in most files
- Signal/slot connections use old-style SIGNAL()/SLOT() macros
- Global config variables declared `extern` in headers (configparams.h, soundconfig.h)
- Platform conditionals: `#ifdef Q_OS_WIN` / `#elif defined(__APPLE__)` / `#else` (Linux)
