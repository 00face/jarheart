# Jarheart

**The next-generation display color temperature and ocular ergonomics engine for Linux desktops.**

[![Release](https://img.shields.io/github/v/release/00face/jarheart?color=red&label=release)](https://github.com/00face/jarheart/releases/latest)
[![CI](https://github.com/00face/jarheart/actions/workflows/ci.yml/badge.svg)](https://github.com/00face/jarheart/actions/workflows/ci.yml)
[![Platform](https://img.shields.io/badge/platform-Linux%20(X11%20%26%20Wayland)-blue.svg)](https://github.com/00face/jarheart)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Jarheart intelligently modulates the color temperature, spectral output, and luminance of your displays based on solar elevation, circadian rhythms, ambient lighting, and medical-grade ophthalmic ergonomics. Designed for both Wayland and X11 compositors, it eliminates eye strain, tames high-contrast halation, preserves dark adaptation, and promotes healthy sleep cycles without degrading workflow productivity.

---

## Standing on the Shoulders of Giants: A Tribute to Redshift

> *"If I have seen further, it is by standing on the shoulders of giants."*

**Jarheart owes its genesis, foundational mathematics, and core inspiration to Redshift**, created in 2010 by **Jon Lund Steffensen** (`jonlst`).

For over a decade, Redshift served as the definitive open-source screen temperature utility for the Linux and BSD communities. When proprietary alternatives were closed or platform-restricted, Redshift provided an elegant, transparent, and mathematically rigorous solution—introducing millions of developers, writers, researchers, and night owls to the benefits of circadian display ergonomics. Jon's implementation of the Planckian blackbody locus approximation and astronomical twilight elevation algorithms established the gold standard for desktop color temperature science.

### The Torch Passed Forward
As Linux desktop architectures transitioned to Wayland compositors, multi-monitor high-DPI displays, and modern build tooling, Redshift's original Autotools architecture began showing its age. Jarheart was forged to honor and preserve Redshift's legacy while answering the demands of modern computing:

- **100% Backward Compatibility**: Seamless drop-in replacement. Symlinks for `redshift` and `redshift-gtk` are provided out of the box, legacy CLI options (`-O`, `-x`, `-v`, `-l`, `-t`) are fully honored, and existing `~/.config/redshift/redshift.conf` files work without modification.
- **Modern Build System**: Replaced Autotools with Meson & Ninja, slashing full clean builds from minutes to under 1.5 seconds.
- **Wayland Native**: First-class `wlr-gamma-control-unstable-v1` protocol integration for Sway, Hyprland, Wayfire, River, and Labwc.
- **Evidence-Based Ocular Ergonomics**: Extended beyond pure Kelvin adjustment to incorporate modern ophthalmic research (melanopic cyan notch filtering, astigmatism halation taming, PWM-free DC dimming, and Daltonization for color vision deficiency).

We express our deepest gratitude to Jon Lund Steffensen and all contributors who maintained Redshift over the years. Jarheart proudly carries that flame forward.

---

## Key Features

### 🖥️ Independent Multi-Monitor Management (WO-031)
Unlike traditional tools that force a uniform global tint across all screens, Jarheart treats each connected display independently:
- **Per-Monitor Calibration**: Assign custom RGB white-point multipliers, brightness scalers, and Kelvin temperature offsets per display output or CRTC.
- **Targeted Toggling**: Disable adjustments on color-critical reference monitors while preserving circadian protection on secondary screens.
- **Dynamic Hotplugging**: Automatically detects connected, disconnected, or DPMS-woken monitors via XRandR and Wayland output events without restarting the daemon.

### ⚙️ Dedicated Preferences Modal (`jarheart-settings`)
A clean, responsive GTK3 control center featuring:
- **Instant Titlebar Actions**: Dedicated **[Save]** (persisting preferences to `~/.config/jarheart/jarheart.conf`) and **[Reset Defaults]** (restoring standard solar parameters and clearing overrides).
- **Tabbed Ergonomics Suite**: Interactive tabs for **Display**, **Monitors**, **Health**, **Schedules**, and **Diagnostics**.
- **Live Preview**: Sliders and toggles update screen state in real time (<2ms socket latency).

### 🧬 Evidence-Based Ocular Health Suite
- **Astigmatism Halation Tamer (WO-026)**: Compresses display gamma contrast and subtly lifts black levels to eliminate the glowing "halo" effect around light text on dark backgrounds.
- **480nm Melanopic Cyan Notch Filter (WO-027)**: Selectively attenuates peak ipRGC (intrinsically photosensitive retinal ganglion cell) blue-cyan wavelengths while maintaining readable white backgrounds.
- **PWM-Free Software Dimming (WO-028)**: Locks display LED backlight at 100% duty cycle (eliminating high-frequency pulse-width modulation flicker and optical migraines) while smoothly dimming perceived luminance via GPU hardware gamma LUTs.
- **Micro-Blink & 20-20-20 Velocity Strain Tracker (WO-029)**: Tracks input cadence and gently cues the user to practice the 20-20-20 rule during prolonged focus sessions.
- **Color Vision Deficiency (CVD) Daltonization (WO-030)**: Real-time Daltonization compensation matrices for **Protanopia**, **Deuteranopia**, **Tritanopia**, and **Achromatopsia**.
- **☀️ Sunlight Anti-Glare Boost (WO-025)**: 7500K daylight overdrive with midtone gamma lift for outdoor laptop visibility.
- **📖 E-Paper Reading Mode (WO-024)**: Transforms screens into a high-legibility monochrome amber/parchment surface.
- **Ambient Contrast Balancer (WO-023)**: Continuously balances display brightness against ambient light sensor readings (`iio-sensor-proxy`).
- **Peripheral Glare Shield**: Soft peripheral radial vignette overlay for wide and ultrawide displays.

### ⚡ Structured Daemon IPC & CLI Subcommands
No regex parsing or PID hacks required. Script and automate the daemon via fast UNIX domain socket commands:
- `jarheart status` (or `jarheart status -j` for Waybar/Polybar JSON)
- `jarheart toggle`
- `jarheart pause [30m|1h|1800]`
- `jarheart resume`
- `jarheart presets` & `jarheart preset [candle|halogen|daylight|darkroom...]`
- `jarheart cvd [protanopia|deuteranopia|tritanopia|off]`
- `jarheart stats` (displays ocular telemetry, cumulative blue-light exposure, and active filters)
- `jarheart reset` & `jarheart quit`

---

## Downloads & Installation

### Option 1: Download Pre-Built Release (Linux x86_64)

Grab the latest binaries directly from the **[GitHub Releases Page](https://github.com/00face/jarheart/releases/latest)**:

- **[jarheart-1.13.0-linux-x86_64.tar.gz](https://github.com/00face/jarheart/releases/latest)**: Complete portable bundle containing the daemon, GTK applet, settings modal, icons, and 1-click installer.
- **[jarheart](https://github.com/00face/jarheart/releases/latest)**: Standalone stripped 64-bit ELF binary.

#### Run Portably (No Installation Required)
```bash
tar -xzf jarheart-1.13.0-linux-x86_64.tar.gz
cd jarheart-1.13.0-linux-x86_64

# Start daemon
./run-portable.sh -l 40.71:-74.00 -t 6500:3400

# Open Settings & Multi-Monitor UI
./run-portable.sh settings
```

#### 1-Click Install
```bash
# User install to ~/.local (no root required)
./install.sh

# Or system-wide install (requires sudo)
sudo ./install.sh --prefix /usr/local
```

---

### Option 2: Build from Source

Jarheart builds in less than 2 seconds using Meson and Ninja.

#### Install Dependencies

**Debian / Ubuntu / Linux Mint / Pop!_OS:**
```bash
sudo apt update
sudo apt install -y meson ninja-build build-essential pkg-config \
    libx11-dev libxrandr-dev libxcb1-dev \
    libwayland-dev wayland-protocols \
    libdrm-dev libglib2.0-dev \
    python3-gi gir1.2-ayatanaappindicator3-0.1 gir1.2-gtk-3.0
```

**Arch Linux / Manjaro:**
```bash
sudo pacman -S --needed meson ninja gcc pkgconf \
    libx11 libxrandr libxcb \
    wayland wayland-protocols \
    libdrm glib2 \
    python-gobject libappindicator-gtk3 gtk3
```

**Fedora / RHEL:**
```bash
sudo dnf install -y meson ninja-build gcc pkgconf-pkg-config \
    libX11-devel libXrandr-devel libxcb-devel \
    wayland-devel wayland-protocols-devel \
    libdrm-devel glib2-devel \
    python3-gobject libappindicator-gtk3 gtk3
```

#### Compile & Install
```bash
# Clone repository
git clone https://github.com/00face/jarheart.git
cd jarheart

# Setup and compile (<2 seconds)
meson setup build --prefix="${HOME}/.local"
ninja -C build

# Run automated test suite
ninja -C build test

# Install
ninja -C build install
```

---

## Quick Start & Usage

### 1. Start the Daemon
```bash
# Automatic location via GeoClue2
jarheart -l geoclue2 &

# Manual coordinates (Latitude:Longitude, e.g. New York)
jarheart -l 40.71:-74.00 &

# Custom day and night temperatures
jarheart -l 40.71:-74.00 -t 6500:3400 &
```

### 2. Launch the GUI
```bash
# Start background tray applet with quick toggle menu
jarheart-gtk &

# Launch Preferences & Multi-Monitor Control Modal
jarheart-settings
```

### 3. Command Line Control
```bash
# Check current temperature, elevation, and active filters
jarheart status

# Instant toggle on/off
jarheart toggle

# Color-critical pause (e.g., photo editing, cinema)
jarheart pause 1h

# Apply Kelvin presets
jarheart preset candle
jarheart preset darkroom

# Check ocular telemetry & blue-light attenuation
jarheart stats

# Resume solar schedule
jarheart reset
```

---

## Window Manager & Status Bar Integration

Jarheart is engineered for seamless integration into modern tiling window managers, desktop bars, and compositors:

### Waybar (`~/.config/waybar/config.jsonc`)
```jsonc
"custom/jarheart": {
    "format": " {text}",
    "tooltip": true,
    "interval": 5,
    "return-type": "json",
    "exec": "jarheart status --json",
    "on-click": "jarheart toggle",
    "on-click-right": "jarheart pause 1h",
    "on-click-middle": "jarheart reset"
}
```

### Polybar (`~/.config/polybar/config.ini`)
```ini
[module/jarheart]
type = custom/script
exec = jarheart status --json | grep -Po '"text": "\K[^"]*'
interval = 5
format-prefix = " "
click-left = jarheart toggle
click-right = jarheart pause 1h
click-middle = jarheart reset
```

### i3 / Sway Keybindings
```bash
bindsym $mod+Shift+n exec --no-startup-id jarheart toggle
bindsym $mod+Shift+p exec --no-startup-id jarheart pause 1h
bindsym $mod+Shift+r exec --no-startup-id jarheart reset
bindsym $mod+Shift+s exec --no-startup-id jarheart-settings
```

### Hyprland (`~/.config/hypr/hyprland.conf`)
```bash
bind = $mainMod SHIFT, N, exec, jarheart toggle
bind = $mainMod SHIFT, P, exec, jarheart pause 1h
bind = $mainMod SHIFT, R, exec, jarheart reset
bind = $mainMod SHIFT, S, exec, jarheart-settings
```

---

## Configuration Reference

Jarheart loads configuration from `~/.config/jarheart/jarheart.conf` (or fallback `~/.config/redshift/redshift.conf`).

```ini
[jarheart]
; Circadian temperature bounds
temp-day=6500
temp-night=3400
brightness=1.00
fade=1

; Solar twilight elevation angles
elevation-high=3
elevation-low=-6

; Adjustment method: 'randr', 'wayland', or 'drm'
adjustment-method=randr

; Location provider: 'manual' or 'geoclue2'
location-provider=manual

; Ocular Ergonomics Features
couple-brightness=false
myopia-protect=false
ambient-balancer=false
halation-tamer=false
melanopic-notch=false
pwm-free=false
strain-tracker=false
vignette-mode=false
auto-brightness=false
battery-saver=off
cvd-mode=none

[manual]
lat=40.71
lon=-74.00

; Multi-Monitor Configuration (optional overrides)
[randr]
crtc0-enabled=true
crtc0-brightness=1.00
crtc0-offset=0
crtc0-gamma=1.000:1.000:1.000

crtc1-enabled=true
crtc1-brightness=0.90
crtc1-offset=-300
crtc1-gamma=1.020:1.000:0.980
```

---

## Automated Test Suite

Jarheart includes an automated C test suite verifying core astronomical and colorimetric algorithms:

```bash
ninja -C build test
```

Tests include:
- `jarheart:solar`: Verifies solar declination, hour angle, and elevation against NOAA Solar Calculator standards.
- `jarheart:colorramp`: Verifies monotonic temperature-to-RGB chromaticity ramps and gamut boundaries.
- `jarheart:config_ini`: Validates INI configuration parser, section handling, and multi-monitor options.
- `jarheart:ipc`: Verifies UNIX domain socket framing, command parsing, and response generation.
- `jarheart:timezone`: Validates system timezone coordinate mapping.

---

## License & Copyright

- **Jarheart Contributors** (c) 2026. Distributed under the **GNU General Public License v3.0 or later** (GPL-3.0-or-later).
- **Redshift Core & Algorithms** (c) 2010–2018 Jon Lund Steffensen and Redshift contributors.
- See [COPYING](COPYING) for complete license terms.
