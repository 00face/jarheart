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

---

## Window Manager & Compositor Integration

Jarheart is engineered from the ground up to integrate seamlessly into modern window managers, status bars, and desktop compositors:

### 1. Waybar (Sway, Hyprland, River)

Jarheart provides built-in JSON output tailored specifically for Waybar custom modules:

```jsonc
// In ~/.config/waybar/config.jsonc
"custom/jarheart": {
    "format": " {text}",
    "tooltip": true,
    "interval": 5,
    "return-type": "json",
    "exec": "jarheart status --json",
    "on-click": "jarheart toggle",
    "on-click-right": "jarheart pause 1h",
    "on-click-middle": "jarheart resume"
}
```

### 2. Polybar (i3, bspwm, awesomewm, xmonad)

```ini
; In ~/.config/polybar/config.ini
[module/jarheart]
type = custom/script
exec = jarheart status --json | grep -Po '"text": "\K[^"]*'
interval = 5
format-prefix = " "
click-left = jarheart toggle
click-right = jarheart pause 1h
click-middle = jarheart resume
```

### 3. XFCE Panel (Generic Monitor Plugin)

Using the XFCE panel Generic Monitor (`genmon`) plugin, run:

```bash
jarheart status --xfce
# Outputs: <txt>6500K</txt><tool>Jarheart: Enabled (Daytime, 6500K)</tool>
```

### 4. Keybindings (i3, Sway, Hyprland)

**i3 / Sway (`~/.config/i3/config` or `~/.config/sway/config`):**
```bash
bindsym $mod+Shift+n exec --no-startup-id jarheart toggle
bindsym $mod+Shift+p exec --no-startup-id jarheart pause 1h
bindsym $mod+Shift+r exec --no-startup-id jarheart resume
```

**Hyprland (`~/.config/hypr/hyprland.conf`):**
```bash
bind = $mainMod SHIFT, N, exec, jarheart toggle
bind = $mainMod SHIFT, P, exec, jarheart pause 1h
bind = $mainMod SHIFT, R, exec, jarheart resume
```

### 5. Compiz, Picom, & X11 Compositors

In composite window managers like **Compiz**, **Picom/Compton**, and **xfwm4**, windows are redirected into offscreen pixmaps and composited via OpenGL into the frame buffer.

- **Direct Hardware CRTC Control**: Jarheart uses `libXrandr` to program the hardware RAMDAC lookup tables directly in the GPU display engine (`XRRSetCrtcGamma`), completely downstream of Compiz's OpenGL compositing pipeline.
- **Dynamic Hotplugging**: Listens to XRandR events (`RRScreenChangeNotify`, `RRCrtcChangeNotify`). When monitors are plugged in, rotated, or woken from DPMS sleep, the new display geometry is instantly detected and color-adjusted.
- **Fullscreen Unredirection**: Safe with Compiz's `unredirect_fullscreen_windows` feature. Gamma adjustments remain persistent whether windows are composited or unredirected.

### 6. Event Hooks (`~/.config/jarheart/hooks/`)

Executable scripts placed in `~/.config/jarheart/hooks/` (or `~/.config/redshift/hooks/`) are triggered automatically on events:
- `period-changed <prev-period> <new-period>`: When sun elevation crosses civil twilight.
- `status-changed <status>`: When adjustments are toggled on, off, or paused.

See [`data/examples/hooks/notify.sh`](data/examples/hooks/notify.sh) for an example desktop notification hook script.

---

## License

Jarheart is licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later). See [COPYING](COPYING) for full license details.
