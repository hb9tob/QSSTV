# QSSTV

QSSTV is a program for receiving and transmitting SSTV (Slow Scan Television) and HAMDRM/DSSTV (Digital Radio Mondiale) images over amateur radio. Compatible with MMSSTV and EasyPal.

## Features

- **SSTV**: all common modes (Robot, Martin, Scottie, PD, etc.)
- **DRM**: OFDM modulation with selectable FEC:
  - Legacy Viterbi (compatible with EasyPal)
  - LDPC (WiFi 802.11n QC-LDPC codes, rates 1/2 to 5/6)
- **Image codecs**: JPEG2000 (JP2) and AVIF (independently selectable)
- **Sound**: PulseAudio + ALSA (Linux), PulseAudio (macOS), PortAudio (Windows)
- **Radio control** via hamlib + serial PTT via QSerialPort
- **Reed-Solomon** error correction with BSR retransmission

## Installation

### Pre-built `.deb` packages (release downloads)

Each release ships pre-built `.deb` packages on the
[Releases page](https://github.com/hb9tob/QSSTV/releases). Both bundle a
custom Hamlib master build (so newer transceivers — Yaesu FTX-1, Icom IC-705,
Kenwood TS-890, etc. — are supported out of the box) under `/usr/lib/qsstv/`.

| File | Target |
|---|---|
| `qsstv_<ver>-1_amd64.deb` | x86_64 desktop (Debian/Ubuntu/Mint) |
| `qsstv_<ver>-1_arm64.deb` | Raspberry Pi 5 / Pi 4 64-bit (RPi OS Bookworm/Trixie, Ubuntu arm64) |

```bash
sudo apt install ./qsstv_9.5.14-1_arm64.deb     # auto-pulls libavif16, qt5, etc.
```

### Debian/Ubuntu — build the `.deb` yourself

```bash
sudo apt install build-essential debhelper qtbase5-dev qt5-qmake \
  qtbase5-dev-tools libqt5svg5-dev libqt5serialport5-dev pkg-config \
  libfftw3-dev libasound2-dev libpulse-dev \
  libopenjp2-7-dev libavif-dev libv4l-dev \
  autoconf automake libtool texinfo libusb-1.0-0-dev   # for hamlib

# 1. Build & install Hamlib master into /usr/local (needed for FTX-1 et al.)
git clone --depth 1 https://github.com/Hamlib/Hamlib.git
cd Hamlib && ./bootstrap && ./configure --prefix=/usr/local --disable-static \
  --with-cxx-binding && make -j$(nproc) && sudo make install && sudo ldconfig
cd ..

# 2. Build the QSSTV .deb (debian/rules bundles libhamlib.so.5* into the package)
cd QSSTV
dpkg-buildpackage -us -uc -b
sudo apt install ../qsstv_*.deb
```

If you do **not** need any transceiver newer than Hamlib 4.6 you can drop
step 1 and add `libhamlib++-dev` to the `apt install` line — but then you
must remove the `cp -P /usr/local/lib/libhamlib.so.5*` line from
`debian/rules` first.

### Fedora/RHEL/openSUSE

```bash
# Fedora/RHEL
sudo dnf install gcc-c++ qt5-qtbase-devel qt5-qtserialport-devel \
  fftw-devel hamlib-devel alsa-lib-devel pulseaudio-libs-devel \
  openjpeg2-devel libavif-devel libv4l-devel pkg-config

# openSUSE
sudo zypper install gcc-c++ libqt5-qtbase-devel libqt5-qtserialport-devel \
  fftw3-devel hamlib-devel alsa-devel libpulse-devel \
  openjpeg2-devel libavif-devel libv4l-devel pkg-config

cd QSSTV/src
qmake-qt5 && make -j$(nproc)
sudo make install
```

### Arch Linux

```bash
sudo pacman -S base-devel qt5-base qt5-serialport fftw hamlib \
  alsa-lib libpulse openjpeg2 libavif v4l-utils pkg-config

cd QSSTV/src
qmake && make -j$(nproc)
sudo make install
```

### Compile from source (generic Linux)

#### Dependencies

| Library | Debian/Ubuntu package | Purpose |
|---------|----------------------|---------|
| Qt5 Core/Gui/Widgets/Network/Xml | `qtbase5-dev` | GUI framework |
| Qt5 SerialPort | `libqt5serialport5-dev` | PTT via serial port |
| FFTW3 | `libfftw3-dev` | FFT for OFDM/DSP |
| hamlib v5 | _build from source_ — see below | Radio control (CAT) |
| ALSA | `libasound2-dev` | Audio backend (Linux) |
| PulseAudio | `libpulse-dev` | Audio backend |
| OpenJPEG | `libopenjp2-7-dev` | JPEG2000 codec |
| libavif | `libavif-dev` | AVIF codec |
| V4L2 | `libv4l-dev` | Video capture (Linux) |
| pkg-config | `pkg-config` | Build system |

> **Hamlib note:** QSSTV is now built against the v5 Hamlib API
> (`HAMLIB_RIGPORT()` / `HAMLIB_PTTPORT()` accessors), which is what current
> Hamlib `master` ships. Distro packages (`libhamlib++-dev` 4.6.x on
> Debian 13 / RPi OS) still expose the legacy struct layout and *will*
> compile QSSTV after a small revert of `src/rig/rigcontrol.cpp`, but they
> lack the recent rig backends (FTX-1, IC-905, etc.). The recommended path
> is to install Hamlib master into `/usr/local`:
>
> ```bash
> git clone --depth 1 https://github.com/Hamlib/Hamlib.git
> cd Hamlib && ./bootstrap && ./configure --prefix=/usr/local --disable-static \
>   --with-cxx-binding && make -j$(nproc) && sudo make install && sudo ldconfig
> ```
>
> `pkg-config` will then resolve `hamlib` to `/usr/local/lib/pkgconfig/hamlib.pc`
> automatically.

```bash
cd QSSTV/src
qmake PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

### macOS

```bash
brew install qt@5 fftw hamlib openjpeg libavif pulseaudio pkg-config

cd QSSTV/src
/opt/homebrew/opt/qt@5/bin/qmake
make -j$(sysctl -n hw.ncpu)
sudo make install
```

**Note:** PulseAudio must be running: `brew services start pulseaudio`

### Windows (MSYS2 MinGW-w64)

See [WINDOWS-BUILD.md](WINDOWS-BUILD.md) for the full step-by-step build,
including how to compile Hamlib master from source under MSYS2 (no
pacman package is available). The Windows build links against the same
v5 Hamlib API used on Linux, so the rig backend list is identical.

A pre-built Windows installer (`qsstv-setup-<ver>.exe`) and portable zip
(`qsstv-<ver>-portable.zip`) are attached to each release.

## Debug build

```bash
sudo apt install doxygen libqwt-qt5-dev  # additional deps
cd QSSTV/src
qmake CONFIG+=debug
make -j$(nproc)
```

## DRM Profile Configuration

Each DRM profile has independent settings for:
- **FEC mode**: Legacy (Viterbi) or LDPC
- **Image codec**: JP2 or AVIF
- **LDPC rate**: 1/2, 2/3, 3/4, 5/6 (only when FEC=LDPC)
- **QAM**: 4-QAM, 16-QAM, 64-QAM
- **Protection level**, **Bandwidth**, **Reed-Solomon**

The four combinations (Viterbi+JP2, Viterbi+AVIF, LDPC+JP2, LDPC+AVIF) are all valid. Use Viterbi+JP2 for maximum compatibility with legacy stations.

## License

GPL-2.0-or-later. See [COPYING](COPYING) for details.
