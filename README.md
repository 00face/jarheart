# Jarheart

Jarheart is a modernized, feature-rich display color temperature manager for Linux and Unix desktops, forked and evolved from Redshift.

It adjusts the color temperature of your screen according to your surroundings and the position of the sun. This reduces eye strain, prevents sleep disruption, and delivers a smooth visual experience across all modern desktop environments.

[![CI](https://github.com/00face/jarheart/actions/workflows/ci.yml/badge.svg)](https://github.com/00face/jarheart/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

---

## What's New in Jarheart

- **Lightning-Fast Meson + Ninja Build System**: Fully replaces legacy Autotools and obsolete `intltool`. Clean builds in under 1.5 seconds.
- **Native Wayland Support**: Direct implementation of the `wlr-gamma-control-unstable-v1` protocol supporting Sway, Hyprland, Wayfire, River, Labwc, and other wlroots-based compositors.
- **Desktop Compositor Compatibility (Compiz, XFCE, picom)**: Modernized `randr` backend using `libXrandr` (1.5+) direct CRTC gamma control, ensuring flicker-free operation downstream of OpenGL compositing on Compiz and other X11 window managers.
- **Structured Daemon IPC & CLI Subcommands**: No more regex parsing of stdout or raw PID signals. Control the running daemon cleanly from any terminal, script, keybinding, or status bar (Polybar, Waybar, XFCE panel):
  - `jarheart status`: Display current period, temperature, brightness, location, and pause timers.
  - `jarheart toggle`: Instantly toggle adjustments on/off.
  - `jarheart pause [30m|1h|1800]`: Temporarily pause adjustments for a specified duration.
  - `jarheart resume`: Resume automatic scheduling.
  - `jarheart set [temp]`: Apply a temporary color temperature override (e.g. `jarheart set 3500`).
  - `jarheart reset`: Clear overrides and resume normal operation.
  - `jarheart quit`: Cleanly terminate the daemon and restore normal display gamma.
- **Automatic Single-Instance Protection**: Prevents duplicate daemons from running simultaneously and thrashing screen gamma.
- **Modern System Tray Support**: Upgraded `jarheart-gtk` with `AyatanaAppIndicator3` support for modern Linux Mint, Ubuntu, Debian, GNOME, and XFCE desktops.
- **100% Backward Compatibility**: Seamless drop-in replacement for Redshift. Provides `redshift` and `redshift-gtk` symlinks, transparently loads existing `~/.config/redshift/redshift.conf` and `[redshift]` configuration blocks, and supports all legacy CLI flags (`-O`, `-x`, `-v`, `-l`, `-t`).
- **Comprehensive Automated Test Suite**: Built-in test suite verifying astronomical solar calculations against NOAA solar standards, color ramp monotonicity, INI configuration parsing, and IPC communication.

---

## Quick Start

### Starting the Daemon

Start Jarheart with automatic location or coordinates:

```bash
# Manual coordinates (Latitude:Longitude)
jarheart -l 40.71:-74.00

# Using GeoClue2 automatic location
jarheart -l geoclue2

# Setting custom day and night temperatures
jarheart -l 40.71:-74.00 -t 6500:3500
```

### Controlling the Running Daemon

Once `jarheart` is running, control it effortlessly from any terminal or keybinding:

```bash
# Check status
jarheart status

# Toggle on or off
jarheart toggle

# Pause for 45 minutes (e.g. while watching a movie or editing photos)
jarheart pause 45m

# Resume automatic adjustments
jarheart resume

# Set manual temperature override
jarheart set 3200

# Reset to automatic astronomical transition
jarheart reset

# Stop the daemon
jarheart quit
```

### One-Shot & Reset Modes

```bash
# Reset screen gamma immediately
jarheart -x

# Set a one-shot temperature
jarheart -O 4000
```

---

## Configuration

Jarheart searches for configuration files in the following order:

1. Path specified via `-c FILE`
2. `~/.config/jarheart/jarheart.conf` (or `$XDG_CONFIG_HOME/jarheart/jarheart.conf`)
3. `~/.config/redshift/redshift.conf` (or `$XDG_CONFIG_HOME/redshift/redshift.conf`)

Example `~/.config/jarheart/jarheart.conf`:

```ini
[jarheart]
; Day and night temperatures in Kelvin
temp-day=6500
temp-night=3600

; Screen fade speed (smooth transition between temperatures)
fade=1

; Solar elevation thresholds for twilight transition
elevation-high=3
elevation-low=-6

; Gamma adjustment method: 'wayland', 'randr', 'drm', or 'dummy'
adjustment-method=randr

; Location provider: 'manual' or 'geoclue2'
location-provider=manual

[manual]
lat=40.71
lon=-74.00

[randr]
screen=0
```

---

## Building from Source

### Dependencies

- Meson (>= 0.60) & Ninja
- C11 compiler (GCC or Clang)
- `pkg-config`
- `libx11`, `libxrandr`, `libxcb` (for X11 / Compiz support)
- `wayland-client`, `wayland-scanner` (for Wayland support)
- `libdrm` (for direct rendering support)
- `glib-2.0`, `gio-2.0` (for GeoClue2 location support)
- Python 3 with `PyGObject` and `AyatanaAppIndicator3` (for GUI status icon)

On Debian / Ubuntu / Linux Mint:

```bash
sudo apt install meson ninja-build build-essential pkg-config \
    libx11-dev libxrandr-dev libxcb1-dev \
    libwayland-dev wayland-protocols \
    libdrm-dev libglib2.0-dev \
    python3-gi gir1.2-ayatanaappindicator3-0.1 gir1.2-gtk-3.0
```

### Build & Install

```bash
# Configure build
meson setup build --prefix=/usr/local

# Compile
ninja -C build

# Run unit tests
meson test -C build

# Install (optional)
sudo ninja -C build install
```

---

## Compatibility with Desktop Compositors (Compiz, etc.)

Jarheart interacts directly with the display server hardware CRTC via XRandR (`XRRSetCrtcGamma`) and flushes hardware state downstream of desktop composite managers.

- **Compiz**: Fully tested and supported. Works seamlessly with Compiz OpenGL redirection, fullscreen window unredirection, and multi-monitor setups.
- **XFCE (xfwm4)**: Supported out of the box.
- **Picom / Compton**: Supported.
- **Wayland (wlroots)**: Supported via native `wlr-gamma-control-unstable-v1` protocol.

---

## License

Jarheart is licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later). See [COPYING](COPYING) for full license details.
