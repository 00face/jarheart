# Jarheart - Quick Build & Installation Guide

This document provides concise instructions for building, installing, and running **Jarheart** from source or pre-built binaries.

---

## 1. Quick Install from Pre-Built Release Archive

If you downloaded `jarheart-1.13.0-linux-x86_64.tar.gz`:

```bash
# 1. Extract the bundle
tar -xzf jarheart-1.13.0-linux-x86_64.tar.gz
cd jarheart-1.13.0-linux-x86_64

# 2. Run portably in-place (no installation needed)
./run-portable.sh -V
./run-portable.sh -l 40.71:-74.00 -t 6500:3400
./run-portable.sh settings

# 3. Or install to ~/.local (user-level, no root required)
./install.sh

# Or install system-wide (requires sudo)
sudo ./install.sh --prefix /usr/local
```

---

## 2. Building from Source

Jarheart uses modern **Meson** and **Ninja**, compiling from clean source in under 2 seconds.

### Prerequisites & Dependencies

#### Debian / Ubuntu / Linux Mint / Pop!_OS
```bash
sudo apt update
sudo apt install -y meson ninja-build build-essential pkg-config \
    libx11-dev libxrandr-dev libxcb1-dev \
    libwayland-dev wayland-protocols \
    libdrm-dev libglib2.0-dev \
    python3-gi gir1.2-ayatanaappindicator3-0.1 gir1.2-gtk-3.0
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S --needed meson ninja gcc pkgconf \
    libx11 libxrandr libxcb \
    wayland wayland-protocols \
    libdrm glib2 \
    python-gobject libappindicator-gtk3 gtk3
```

#### Fedora / RHEL
```bash
sudo dnf install -y meson ninja-build gcc pkgconf-pkg-config \
    libX11-devel libXrandr-devel libxcb-devel \
    wayland-devel wayland-protocols-devel \
    libdrm-devel glib2-devel \
    python3-gobject libappindicator-gtk3 gtk3
```

---

### Compile, Test & Install

```bash
# 1. Clone repository (if not already cloned)
git clone https://github.com/00face/jarheart.git
cd jarheart

# 2. Configure build with Meson (installing to ~/.local)
meson setup build --prefix="${HOME}/.local"

# 3. Compile binaries
ninja -C build

# 4. Run automated test suite
ninja -C build test

# 5. Install to your chosen prefix
ninja -C build install
```

> **Tip:** If installing system-wide, use `meson setup build --prefix=/usr/local` and `sudo ninja -C build install`.

---

## 3. Quick Verification & Running

Ensure `~/.local/bin` is in your `PATH` (default in modern Linux shells):

```bash
# Check version
jarheart -V

# Start Jarheart daemon (auto-detects local coordinates & solar twilight)
jarheart -l geoclue2 &

# Or specify manual latitude and longitude (e.g. Chicago: 41.85, -87.65)
jarheart -l 41.85:-87.65 &
```

### Controlling the Running Daemon

Use the fast UNIX socket IPC subcommands:

```bash
# Show live circadian status
jarheart status

# Toggle adjustments on/off
jarheart toggle

# Temporarily pause for color-critical work
jarheart pause 45m

# Resume automatic scheduling
jarheart resume

# List Kelvin lighting presets
jarheart presets

# Apply a Kelvin preset (e.g. candle, incandescent, halogen, daylight, darkroom)
jarheart preset candle

# Reset to solar defaults
jarheart reset
```

---

## 4. Launching the GUI

### System Tray & Notification Daemon
```bash
jarheart-gtk &
```
Provides an active tray indicator with quick mode toggles (Darkroom ruby-red, Movie mode, E-paper reading mode, Sunlight anti-glare, CVD Daltonization, battery throttle).

### Preferences & Multi-Monitor Control Modal
```bash
jarheart-settings
```
Provides interactive tabs for:
- **Display**: Day/Night temperature curves, brightness, and Kruithof coupling.
- **Monitors**: Independent per-monitor enable/disable, brightness, Kelvin offset, and RGB calibration.
- **Health**: Astigmatism Halation Tamer, 480nm Cyan Notch Filter, PWM-Free dimming, and 20-20-20 Blink Pacer.
- **Schedules**: Diurnal tri-phasic, biphasic, manual fixed, and solar astronomical timing.
- **Diagnostics**: Real-time display server query, CRTC state, and IPC latency.
