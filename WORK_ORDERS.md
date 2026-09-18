# Jarheart Work Order Manager & Task Rites Specification

**Project**: Jarheart (Display Temperature & Circadian Management System)  
**Repository**: `00face/jarheart` (forked from `jonls/redshift`)  
**Standard**: C11 / Meson / POSIX.1-2008 / FreeDesktop.org  
**Target Environment**: Linux Mint 22.3 / Ubuntu 24.04 LTS (X11 + Compiz / Wayland wlroots)  
**Document Version**: 1.0.0  
**Last Updated**: September 18, 2026  

---

## 1. Operational Philosophy & The Task Rites

In the engineering of display-altering systems software, software defects immediately degrade human visual ergonomics and hardware stability. To guarantee mathematical precision, zero display flicker, compositor harmony, and regression immunity, all development on Jarheart is governed by the **Six Task Rites of Jarheart**.

```
+---------------------------------------------------------------------------------------+
|                              THE SIX TASK RITES OF JARHEART                            |
+---------------------------------------------------------------------------------------+
|  RITE I: INCEPTION & SPECIFICATION                                                   |
|    -> Mathematical modeling (Planckian locus, gamma curve transfer functions)         |
|    -> Safety constraints (photon suppression, hardware boundaries, memory limits)     |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|  RITE II: CONSTRUCTION & ARCHITECTURAL INVARIANTS                                     |
|    -> C11 standard adherence, zero compiler warnings (-Wall -Wextra -pedantic)        |
|    -> Subsecond Meson/Ninja rebuilds, bounded buffers, non-blocking poll() reactor    |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|  RITE III: VERIFICATION & HARDWARE-IN-THE-LOOP AUDIT                                  |
|    -> Automated unit test suites (solar, colorramp, ini, ipc, timezone)               |
|    -> Direct CRTC gamma hardware inspection (XRRGetCrtcGamma / memfd inspection)      |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|  RITE IV: REGRESSION PROTECTION & SANITIZATION                                        |
|    -> Startup ramp sanitization (never save warm ramps as baseline neutral)           |
|    -> Direct identity linear writes on disable/reset (R=G=B=65535)                    |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|  RITE V: SYSTEM DEPLOYMENT & INTEGRATION                                              |
|    -> Local prefix deployment (~/.local/bin, ~/.local/share/applications)             |
|    -> Desktop specification, icon cache update, systemd user service registration     |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|  RITE VI: RELEASE, AUDITING & SITREP REPORTING                                        |
|    -> Conventional commit formatting (feat, fix, docs, test, chore)                   |
|    -> Synchronized Work Order Manager & SITREP update                                 |
+---------------------------------------------------------------------------------------+
```

### Rite I: The Rite of Inception (Triage, Scoping & Evidence-Based Photometrics)
1. **Photometric / Mathematical Foundation**: Every color alteration must be defined by closed-form equations (Robertson's method for correlated color temperature, CIE 1931 xy chromaticity coordinates, or explicit gamma transfer functions).
2. **Evidence-Based Physiological & Ophthalmic Bounds**: Features modifying color balance and display properties must be anchored in clinical and empirical ocular research:
   - *Luminance-CCT Co-Modulation Invariant (NYU Langone Eye Center RCT)*: Shifting color temperature alone does not significantly alleviate digital eye strain; reducing screen brightness (to 50–60%) provides a statistically significant, clinically meaningful reduction in ocular fatigue (P = 0.0007). Display temperature transitions must therefore support coupled luminance attenuation to eliminate pupillary conflict under warm spectra.
   - *Low-CCT Axial Myopia Retardation Invariant (CAS Macaque Longitudinal Study)*: 365-day controlled primate trials demonstrate that low-CCT artificial illumination (2700K–3000K) rich in long-wavelength spectral components significantly retards ocular axial elongation compared to high CCT (4000K–5000K). Extended reading and juvenile modes must provide low-CCT protection.
   - *Choroidal Thickness & Spectral Fullness (SERI / Duke-NUS IOVS Study)*: Continuous, full-spectrum daylight-mimicking illumination accelerates recovery from axial elongation and prevents choroidal thinning compared to discontinuous spiked fluorescent lighting.
   - *Diurnal Alertness vs. Visual Comfort Split (Shi et al. 2025)*: High CCT (>6000K–6300K) actively promotes daytime alertness and cognitive performance, while 3000K–4000K optimizes subjective visual comfort and fatigue relief, and ≤2700K preserves evening melatonin secretion.
   - *Ocular Tear Film & Ciliary Muscle Pacing (20-20-20 Protocol)*: Prolonged screen gaze suppresses involuntary blink rate, inducing tear film evaporative breakdown and accommodative ciliary spasm. Display tooling should provide micro-break pacing.
   - *Zero-Photon Emission for Darkroom*: Pure monochrome ruby red (green and blue lookup tables strictly locked to `0`).
   - *Highlight Preservation for Cinema*: Dynamic blue floor lift and toe expansion equations for movie viewing.
3. **Control Interface Contract**: Any new capability must specify its interface across all three pillars of Jarheart:
   - Command-line argument and subcommand (`jarheart <command>`).
   - Unix Domain Socket IPC command verb (`src/ipc.c`).
   - Desktop Tray GUI indicator binding (`src/redshift-gtk/statusicon.py`).

### Rite II: The Rite of Construction (C11 Standards & Architectural Invariants)
1. **Dialect & Portability**: Strict ISO C11 standard. Compiler warnings must remain at zero under `-Wall -Wextra -pedantic`.
2. **Subsecond Meson Invariant**: Build times must remain under 0.5s on modern hardware via Ninja. No heavy autotools or recursive make.
3. **Non-Blocking Reactor Model**: No worker thread may block the main display loop. All I/O (IPC socket, display events, timer wakeups, signals) must be multiplexed within the single-threaded `poll()` event loop in [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c).
4. **Memory & Resource Safety**:
   - Zero static buffer overruns; all string formats must use `snprintf` with explicit buffer size tracking.
   - File descriptors must be closed immediately or marked `FD_CLOEXEC`.
   - IPC socket allocations must be cleaned on exit and unlinked gracefully.

### Rite III: The Rite of Verification (Test Harness & Hardware-in-the-Loop)
1. **Unit Test Gate**: Every functional commit must execute and pass all automated unit tests in under 100 milliseconds:
   ```bash
   ninja -C build test
   ```
2. **Hardware-in-the-Loop CRTC Verification**: Changes affecting gamma lookup tables must be verified against live display hardware using direct XRandR queries:
   - Query endpoint values across Red, Green, and Blue lookup tables.
   - Verify that neutral mode (`6500K`) achieves `R=65535, G=65535, B=65535`.
   - Verify that darkroom mode achieves `R > 0, G = 0, B = 0`.
3. **Compositor Harmony Test**: Verify compatibility with active compositors (e.g., Compiz with OpenGL redirect). XRandR hardware gamma operates downstream of the composite manager, ensuring no shader clobbering.

### Rite IV: The Rite of Regression Protection (Ramp Sanitization & Invariants)
1. **Baseline Ramp Sanitization Invariant**:
   - Never assume the startup gamma ramp is neutral. If the screen is already tinted when Jarheart starts, `randr_ramp_is_tinted()` must reject the tinted curve and synthesize a pristine identity linear ramp.
2. **Absolute Reversion Invariant**:
   - Whenever Jarheart is disabled, paused, or reset to 6500K, it must write a pure linear identity curve directly to the hardware CRTC, clearing any residual tint from any previous crash or run.
3. **Single-Instance Invariant**:
   - The daemon must bind an exclusive Unix domain socket (`$XDG_RUNTIME_DIR/jarheart.sock`). Second instances must act as CLI clients directing commands to the primary daemon.

### Rite V: The Rite of Deployment & System Integration
1. **Dual-Branding Continuity**: All paths must resolve `jarheart` first, with graceful backward-compatible fallback to `redshift` (`~/.config/jarheart/` -> `~/.config/redshift/`).
2. **Desktop Integration**:
   - Valid FreeDesktop `.desktop` files in `~/.local/share/applications/`.
   - High-resolution SVG icons installed in `hicolor` scalable icon paths.
   - Functional systemd user service units in `~/.config/systemd/user/`.
3. **Tray Persistence**: The GTK indicator process must handle parent terminal disconnection (`SIGHUP` immunity) and survive independently.

### Rite VI: The Rite of Release, Auditing & SITREP Reporting
1. **Conventional Commit Protocol**: All git commits must adhere to structured prefixes: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`.
2. **Work Order Register Synchronization**: Each completed development increment must transition its corresponding Work Order in this document to `COMPLETED`, documenting acceptance verification.
3. **SITREP Issuance**: Major state transitions must produce a Situation Report documenting hardware state, test logs, and active parameters.

---

## 2. Work Order Lifecycle State Machine

```
   +--------------+      Triage & Spec      +--------------+
   |    DRAFT     | ----------------------> |   TRIAGED    |
   +--------------+                         +--------------+
                                                   |
                                                   | Construction (Rite II)
                                                   v
   +--------------+    Acceptance Pass      +--------------+
   |  COMPLETED   | <---------------------- | VERIFICATION |
   +--------------+     (Rite III & IV)     +--------------+
          ^                                        |
          |                                        | Failure / Regression
          +----------------------------------------+
```

- **DRAFT**: Proposed requirement, idea, or defect report under initial formulation.
- **TRIAGED**: Scoped, assigned, mathematical equations defined, acceptance criteria specified.
- **IN_PROGRESS**: Code actively being constructed in compliance with Rite II.
- **VERIFICATION**: Code built; undergoing unit test suite, hardware-in-the-loop checks, and compositor audit.
- **COMPLETED**: Acceptance criteria 100% verified, documentation synchronized, committed to version control.
- **DEFERRED**: Postponed for future architecture phases.

---

## 3. Master Work Order Index

| Work Order | Title | Priority | Category | Status | Target / Deliverable |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **WO-001** | Meson Build System & Subsecond Toolchain Modernization | P1 | Build / Infra | **COMPLETED** | `meson.build`, subsecond Ninja build |
| **WO-002** | Automated Unit Testing Harness & NOAA Solar Validation | P1 | Quality / Test | **COMPLETED** | `tests/`, 5 test suites passing |
| **WO-003** | X11 & Compiz Compositor Compatibility & Event Multiplexer | P0 | Display / WM | **COMPLETED** | `src/gamma-randr.c`, `src/redshift.c` |
| **WO-004** | Native Wayland `wlr-gamma-control-unstable-v1` Backend | P1 | Wayland | **COMPLETED** | `src/gamma-wayland.c`, Wayland hotplug |
| **WO-005** | Unix Domain Socket IPC & Subcommand Control Interface | P1 | IPC / CLI | **COMPLETED** | `src/ipc.c`, CLI client subcommands |
| **WO-006** | System Timezone Location Provider & GeoClue2 Auto-Fallback | P2 | Location | **COMPLETED** | `src/location-timezone.c`, `/etc/localtime` |
| **WO-007** | Screen Temperature Reversion Defect & Identity Ramp Correction | P0 | Defect / Core | **COMPLETED** | `randr_ramp_is_tinted()`, identity writes |
| **WO-008** | Darkroom Mode (Monochrome Ruby Red / Zero Blue-Green Photons) | P1 | Feature / Mode | **COMPLETED** | `jarheart darkroom`, zero green/blue |
| **WO-009** | Movie Mode (2½h Cinema Tone & Shadow/Highlight Detail) | P1 | Feature / Mode | **COMPLETED** | `jarheart movie`, 9000s countdown |
| **WO-010** | Kelvin Presets Spectrum & Dynamic Time Schedules | P1 | Feature / Mode | **COMPLETED** | 14 Kelvin presets, clock schedules |
| **WO-011** | Hardware Backlight, Ambient Sensor & Environmental Weather | P2 | Checks / Sys | **COMPLETED** | `src/checks.c`, `jarheart check` |
| **WO-012** | AyatanaAppIndicator3 Modern GTK Tray & Start Menu Integration | P1 | GUI / Desktop | **COMPLETED** | `src/redshift-gtk/`, `.desktop`, icons |
| **WO-013** | High-Precision Multi-Monitor Independent CRTC Calibration | P2 | Display | **QUEUED** | Per-CRTC gamma curves in config |
| **WO-014** | Ambient Light Sensor (IIO) Dynamic Auto-Brightness Daemon | P2 | Sensor / HW | **QUEUED** | Adaptive brightness loop via IIO lux |
| **WO-015** | D-Bus Desktop Notification System for Mode & Transition Alerts | P3 | Desktop / UX | **QUEUED** | FreeDesktop notification popups |
| **WO-016** | Redshift-to-Jarheart Legacy Configuration Migration Tooling | P3 | Tooling | **QUEUED** | Automated migration script `jarheart-migrate` |
| **WO-017** | Wayland Gamma Blend Curves (Smooth Per-Output Transitions) | P2 | Wayland | **QUEUED** | Atomic animated transitions on wlroots |
| **WO-018** | Battery Saver / Low Power Adaptive Temp & Backlight Throttling | P3 | Power / Mobile | **QUEUED** | UPower D-Bus integration for battery life |
| **WO-019** | Evidence-Based Dual Brightness-CCT Coupling (Kruithof Ergonomics) | P1 | Ergonomics | **TRIAGED** | Coupled brightness-Kelvin attenuation |
| **WO-020** | Pediatric & Extended Reading Myopia Protection Mode (2700K–3000K) | P1 | Health / Mode | **TRIAGED** | `jarheart myopia-protect`, 2850K @ 60% lum |
| **WO-021** | Ergonomic 20-20-20 Ocular Relaxation & Tear-Film Restoration Pacer| P2 | Ergonomics | **TRIAGED** | `jarheart pacer`, micro-break reminders |
| **WO-022** | Diurnal Bi-Phasic Alertness-to-Comfort Circadian Schedule | P2 | Circadian | **TRIAGED** | Morning alertness -> afternoon comfort -> night |
| **WO-023** | Ambient Contrast & Eye-Level Illuminance Balancer (ALS Dynamic) | P2 | Sensor / HW | **TRIAGED** | Contrast-matching screen to room lux |

---

## 4. Detailed Work Order Specifications

### WO-001: Meson Build System & Subsecond Toolchain Modernization
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Build / Infrastructure Modernization`
- **Components**: [`meson.build`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/meson.build), `meson_options.txt`
- **Problem Statement**: The upstream Autotools build system required heavy shell scripts (`configure`), was slow (5-10s rebuild times), lacked modular dependency resolution, and complicated unit testing.
- **Implementation**:
  - Replaced GNU Autotools with modern declarative **Meson 0.55+** and **Ninja**.
  - Structured target dependencies for `libxrandr`, `libxcb`, `libxcb-randr`, `wayland-client`, `geoclue-2.0`, `libdrm`, and `math`.
  - Added compile flags for C11 (`c_std=c11`), optimization, and warning hygiene.
- **Verification & Acceptance**:
  - Incremental rebuild execution time: `< 0.25s`.
  - Binary cleanly outputs version `1.12.1-jarheart` on `jarheart -V`.

---

### WO-002: Automated Unit Testing Harness & NOAA Solar Calculations
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Quality Assurance / Test Harness`
- **Components**: [`tests/test_solar.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/tests/test_solar.c), [`tests/test_colorramp.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/tests/test_colorramp.c), [`tests/test_config_ini.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/tests/test_config_ini.c), [`tests/test_ipc.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/tests/test_ipc.c), [`tests/test_timezone.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/tests/test_timezone.c)
- **Problem Statement**: Redshift historically had zero automated regression tests. Any refactoring risked breaking astronomical solar calculations or blackbody interpolation.
- **Implementation**:
  - Created 5 self-contained test suites covering astronomical solar elevation, color ramp generation, INI parser, IPC serialization, and timezone lookup.
  - Configured Meson test runner integration.
- **Verification & Acceptance**:
  - Ran `ninja -C build test`:
  - Result: 5/5 tests pass in **0.03s**.

---

### WO-003: X11 & Compiz Compositor Compatibility & Event Multiplexer
- **Status**: `COMPLETED`
- **Priority**: `P0 - Critical`
- **Type**: `Display Server / Window Manager Integration`
- **Components**: [`src/gamma-randr.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/gamma-randr.c), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c)
- **Problem Statement**: Desktop compositors (specifically Compiz, Picom, Xfwm4) can clash with display adjustment tools if gamma tables are clobbered or if display sleep/wake (DPMS) and resolution changes reset the hardware CRTC without Jarheart noticing.
- **Implementation**:
  - Implemented `XRRSelectInput()` listening for `RRScreenChangeNotifyMask` and `RRCrtcChangeNotifyMask`.
  - Added the X11 connection file descriptor to the master `poll()` loop in `redshift.c`.
  - Automated dynamic CRTC re-enumeration and instant re-application of target color curves upon resolution changes or monitor reconnects.
  - Ensured operations use `XRRSetCrtcGamma()` directly on CRTCs, which sits downstream of the Compiz OpenGL compositing redirect window.
- **Verification & Acceptance**:
  - Tested on live system with active Compiz composite manager (`compiz --replace`).
  - Zero screen flickering, zero compositor tearing, zero stutter.

---

### WO-004: Native Wayland `wlr-gamma-control-unstable-v1` Backend
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Display Server Modernization`
- **Components**: [`src/gamma-wayland.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/gamma-wayland.c), `protocol/wlr-gamma-control-unstable-v1.xml`
- **Problem Statement**: Wayland does not permit client access to X11 gamma ramps. Modern Wayland compositors (Sway, Hyprland, Wayfire, River) require the wlroots gamma control protocol.
- **Implementation**:
  - Compiled Wayland protocol client bindings via `wayland-scanner`.
  - Implemented shared memory file descriptor allocation via `memfd_create()`.
  - Implemented `gamma_control_listener` for atomic hardware gamma table synchronization across all Wayland outputs.
- **Verification & Acceptance**:
  - Successful compilation and protocol negotiation against wlroots gamma control interfaces.

---

### WO-005: Unix Domain Socket IPC & Subcommand Control Interface
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `IPC / Control Architecture`
- **Components**: [`src/ipc.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/ipc.c), [`src/ipc.h`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/ipc.h), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c)
- **Problem Statement**: Previously, interacting with a running Redshift instance required sending raw POSIX signals (`SIGUSR1`) or killing the process. There was no mechanism to inspect status, pause temporarily, toggle modes, or query parameters.
- **Implementation**:
  - Implemented non-blocking UNIX domain socket at `$XDG_RUNTIME_DIR/jarheart.sock`.
  - Multiplexed client connections in the core `poll()` reactor.
  - Added rich CLI subcommands:
    - `jarheart status` & `jarheart status -j` (JSON for status bars: Waybar, Polybar, i3blocks).
    - `jarheart toggle`, `jarheart pause [DURATION]`, `jarheart resume`.
    - `jarheart darkroom`, `jarheart movie`, `jarheart preset <name>`, `jarheart presets`.
    - `jarheart schedule`, `jarheart check`, `jarheart reset`.
- **Verification & Acceptance**:
  - Subcommands respond in `< 2ms`.
  - JSON output validated for parsing in Polybar / Waybar.

---

### WO-006: System Timezone Location Provider & GeoClue2 Auto-Fallback
- **Status**: `COMPLETED`
- **Priority**: `P2 - Normal`
- **Type**: `Location Provider / Automation`
- **Components**: [`src/location-timezone.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/location-timezone.c), [`src/location-timezone.h`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/location-timezone.h)
- **Problem Statement**: GeoClue2 frequently fails or hangs on machines without GPS hardware or when D-Bus location services are disabled, causing Redshift to abort on startup.
- **Implementation**:
  - Created automatic `/etc/localtime` parser mapping system IANA timezones (e.g., `America/Chicago`) to geographical reference coordinates (41.85°N, 87.65°W).
  - Configured as automatic fallback when GeoClue2 is unavailable.
- **Verification & Acceptance**:
  - Validated on system with `America/Chicago`: cleanly populated latitude 41.85 and longitude -87.65 without network calls.

---

### WO-007: Screen Temperature Reversion Defect & Identity Ramp Correction
- **Status**: `COMPLETED`
- **Priority**: `P0 - Critical / Core Defect`
- **Type**: `Display Hardware Defect Resolution`
- **Components**: [`src/gamma-randr.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/gamma-randr.c), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c)
- **Problem Statement**: When disabling, resetting, or pausing Redshift, the screen previously failed to revert back to default linear neutral colors, remaining tinted orange/warm.
- **Root Cause**:
  1. Startup gamma capture saved already-tinted hardware curves as baseline.
  2. Setting neutral white point `(1.0, 1.0, 1.0)` multiplied against the saved warm baseline curve instead of restoring hardware identity.
- **Implementation**:
  - Implemented `randr_ramp_is_tinted()`: inspects initial hardware ramps; if blue/green are suppressed, synthesizes a true identity ramp.
  - Implemented direct linear identity write in `randr_set_temperature_for_crtc()` when neutral (`is_neutral = (temp == 6500 && brightness == 1.0 && gamma == 1.0)`).
- **Verification & Acceptance**:
  - Live hardware CRTC inspection:
    - Neutral: `CRTC 0: End: R=65535, G=65535, B=65535`.
    - Warm: `CRTC 0: End: R=65535, G=35669, B=5794`.
    - Toggle Off: `CRTC 0: End: R=65535, G=65535, B=65535` (100% clean identity restoration verified).

---

### WO-008: Darkroom Mode (Monochrome Ruby Red / Zero Blue-Green Photons)
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Specialized Mode / Visual Ergonomics`
- **Components**: [`src/colorramp.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/colorramp.c), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c), [`src/ipc.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/ipc.c), [`src/redshift-gtk/statusicon.py`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift-gtk/statusicon.py)
- **Problem Statement**: Nighttime astronomers, darkroom photographers, and users suffering from severe eye fatigue need complete elimination of blue and green photons without changing individual application themes.
- **Implementation**:
  - In `colorramp_fill()`, when `darkroom_mode` is enabled:
    - Set Green and Blue channel lookup tables strictly to `0`.
    - Scale Red channel monotonically through brightness and gamma.
  - Bound CLI command `jarheart darkroom [on|off|toggle]`, IPC dispatch, and GTK checkbox.
- **Verification & Acceptance**:
  - Verified on live CRTC 0: `R=55704, G=0, B=0`. Exact zero photon emission on green and blue channels.

---

### WO-009: Movie Mode (2½h Cinema Tone & Shadow/Highlight Detail)
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Specialized Mode / Multimedia`
- **Components**: [`src/colorramp.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/colorramp.c), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c), [`src/ipc.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/ipc.c)
- **Problem Statement**: Standard night tinting crushes shadows and turns blue skies muddy orange during movie playback.
- **Implementation**:
  - Movie Mode features an automatic 2½-hour timer (9000 seconds) returning smoothly to schedule.
  - Applied shadow toe expansion curve `pow(Y, 0.88)` across low-luminance values to prevent crushed blacks.
  - Dynamically lifted blue highlight floor `white_point[2] + (1 - white_point[2]) * pow(Y, 1.8) * 0.45` above luminance 0.40 to preserve natural blue skies.
- **Verification & Acceptance**:
  - Verified on live CRTC 0: `R=65534, G=55407, B=54183`. Highlights and dark shadows preserved under cinema tone.

---

### WO-010: Kelvin Presets Spectrum & Dynamic Time Schedules
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Core Feature / Usability`
- **Components**: [`src/colorramp.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/colorramp.c), [`src/colorramp.h`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/colorramp.h), [`src/redshift.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift.c), [`src/ipc.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/ipc.c)
- **Problem Statement**: Users frequently need quick access to recognized color temperatures (Candle, Halogen, Daylight, planetary tones) and the ability to define clock-based transition schedules rather than solar elevation.
- **Implementation**:
  - Embedded 14 calibrated presets: `ember` (1200K), `candle` (1900K), `mars` (2100K), `warm-incandescent` (2300K), `incandescent` (2700K), `jupiter` (3200K), `halogen` (3400K), `saturn` (3800K), `moon` (4100K), `fluorescent` (4200K), `venus` (4800K), `sunlight` (5500K), `mercury` (5800K), `daylight` (6500K).
  - Implemented dynamic clock transition scheduling: `jarheart schedule <dawn> <dusk>` and `jarheart schedule solar`.
- **Verification & Acceptance**:
  - CLI `jarheart presets` formats clean table.
  - Selecting preset applies instantly and updates status.

---

### WO-011: Hardware Backlight, Ambient Sensor & Environmental Weather
- **Status**: `COMPLETED`
- **Priority**: `P2 - Normal`
- **Type**: `Hardware & Environmental Integration`
- **Components**: [`src/checks.c`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/checks.c), [`src/checks.h`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/checks.h)
- **Problem Statement**: Display color balance is heavily influenced by ambient light, physical panel backlight level, and outdoor weather conditions.
- **Implementation**:
  - Added Linux `/sys/class/backlight` reader for hardware screen brightness percentage.
  - Added industrial I/O IIO ambient light sensor reader (`/sys/bus/iio/devices/iio:device*/in_illuminance*`).
  - Added non-blocking cached weather queries (`wttr.in`).
  - Added periodic `/etc/localtime` polling (every 60s) to detect geographical relocation.
  - Added CLI `jarheart check` and GTK Environment Info dialog.
- **Verification & Acceptance**:
  - Live query: `Light: Backlight at 100% (intel_backlight), Weather: Overcast : +64°F, Timezone: America/Chicago`.

---

### WO-012: AyatanaAppIndicator3 Modern GTK Tray & Start Menu Integration
- **Status**: `COMPLETED`
- **Priority**: `P1 - High`
- **Type**: `Desktop Integration / GUI`
- **Components**: [`src/redshift-gtk/statusicon.py`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/src/redshift-gtk/statusicon.py), [`data/applications/jarheart.desktop`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/data/applications/jarheart.desktop.in), [`data/applications/jarheart-gtk.desktop`](file:///home/face/Documents/Documents/code/jarheart-repo/jarheart/data/applications/jarheart-gtk.desktop.in)
- **Problem Statement**: Tray icon was deprecated, crashed on modern Ayatana indicator environments, and lacked menu items for new modes, presets, and checks.
- **Implementation**:
  - Updated GTK app to utilize `AyatanaAppIndicator3` with clean fallbacks.
  - Added submenus for Kelvin Presets, Darkroom Mode toggle, Movie Mode toggle, Color-Critical Pause durations, and System Checks dialog.
  - Handled `SIGHUP` signal to prevent tray crash on subshell exit.
  - Created FreeDesktop compliant `.desktop` shortcuts in `~/.local/share/applications/`.
- **Verification & Acceptance**:
  - Shortcut appears in application menu under System and Utilities.
  - Tray icon runs cleanly and controls daemon via IPC.

---

### WO-013: High-Precision Multi-Monitor Independent CRTC Calibration
- **Status**: `QUEUED`
- **Priority**: `P2 - Normal`
- **Type**: `Display Server / Multi-Head`
- **Prerequisites**: WO-003, WO-004
- **Problem Statement**: Multi-monitor desktop setups frequently have panels with differing native white points (e.g. one warm IPS panel alongside a cool OLED or TN panel). Users need per-monitor gamma and white-point offsets.
- **Scope & Technical Plan**:
  - Extend configuration syntax to accept per-monitor sections: `[randr:DP-1]` and `[randr:HDMI-1]`.
  - Store independent calibration matrices per CRTC index.
  - Apply custom gamma multiplier to each CRTC in `randr_set_temperature_for_crtc()`.
- **Verification Protocol**:
  - Dual monitor rig: monitor A calibrated to 6500K baseline, monitor B offset to 6200K baseline; verified via `xrandr --verbose`.

---

### WO-014: Ambient Light Sensor (IIO) Dynamic Auto-Brightness Daemon
- **Status**: `QUEUED`
- **Priority**: `P2 - Normal`
- **Type**: `Sensor / Hardware Automation`
- **Prerequisites**: WO-011
- **Problem Statement**: Currently, ambient light is read on demand via `jarheart check`. Laptops with built-in ambient sensors (MacBooks, ThinkPads, Framework) should dynamically modulate panel brightness and color temperature as ambient room lighting changes.
- **Scope & Technical Plan**:
  - Add optional configuration key `auto-brightness = true` and `als-threshold = 50`.
  - In `src/checks.c`, poll IIO lux sensor every 5 seconds.
  - Smoothly interpolate backlight write via `/sys/class/backlight/*/brightness` or RandR backlight property.
- **Verification Protocol**:
  - Covering sensor reduces brightness smoothly without jitter.

---

### WO-015: D-Bus Desktop Notification System for Mode & Transition Alerts
- **Status**: `QUEUED`
- **Priority**: `P3 - Enhancement`
- **Type**: `Desktop Integration / UX`
- **Prerequisites**: WO-005, WO-008, WO-009
- **Problem Statement**: When transitions begin (e.g., sunset transition starting, Movie Mode 2½h countdown expiring), the user receives no visual notice except the screen tone changing.
- **Scope & Technical Plan**:
  - Implement a lightweight D-Bus notification client (`org.freedesktop.Notifications`) in C (`src/notify.c`).
  - Dispatch low-priority, transient notification when Movie Mode expires, Darkroom is toggled, or Color-Critical pause ends.
  - Add configuration setting `notifications = true/false`.
- **Verification Protocol**:
  - `jarheart movie 5s` sends alert on start and upon return to solar schedule.

---

### WO-016: Redshift-to-Jarheart Legacy Configuration Migration Tooling
- **Status**: `QUEUED`
- **Priority**: `P3 - Enhancement`
- **Type**: `Tooling & Usability`
- **Prerequisites**: WO-001, WO-005
- **Problem Statement**: Users migrating from `~/.config/redshift.conf` or `~/.config/redshift/redshift.conf` need an automated migration utility to update configuration keys, import hooks, and test settings.
- **Scope & Technical Plan**:
  - Provide `jarheart-migrate` script or subcommand.
  - Validates existing `redshift.conf`, creates `~/.config/jarheart/jarheart.conf`, and appends new mode defaults.
- **Verification Protocol**:
  - Successfully parses legacy config and outputs valid modern Jarheart configuration.

---

### WO-017: Wayland Gamma Blend Curves (Smooth Per-Output Transitions)
- **Status**: `QUEUED`
- **Priority**: `P2 - Normal`
- **Type**: `Wayland / Compositor Modernization`
- **Prerequisites**: WO-004
- **Problem Statement**: On Wayland, instantaneous ramp replacement can cause visual stepping if the compositor does not interpolate table updates.
- **Scope & Technical Plan**:
  - Implement smooth client-side interpolation steps (30-60 fps) across 200ms when transitioning Wayland gamma states.
- **Verification Protocol**:
  - Smooth ramp transitions under Sway and Hyprland without visual stepping.

---

### WO-018: Battery Saver / Low Power Adaptive Temp & Backlight Throttling
- **Status**: `QUEUED`
- **Priority**: `P3 - Mobile Optimization`
- **Type**: `Power Management`
- **Prerequisites**: WO-011
- **Problem Statement**: On battery power, warmer color temperatures and lower backlight levels save significant OLED and LCD display power.
- **Scope & Technical Plan**:
  - Monitor UPower D-Bus signals for AC disconnect / battery low.
  - Automatically drop color temperature to 3400K (Halogen) and decrease brightness by 20% when battery drops below 20%.
- **Verification Protocol**:
  - Simulating battery low signal triggers power-saving profile.

---

### WO-019: Evidence-Based Dual Brightness-CCT Coupling (Kruithof Ergonomics)
- **Status**: `TRIAGED`
- **Priority**: `P1 - High`
- **Type**: `Ergonomics & Physiological Vision`
- **Prerequisites**: WO-005, WO-007, WO-011
- **Scientific Foundation**: NYU Langone Eye Center Randomized Controlled Trial (NCT05042960). Color temperature modulation alone (f.lux 2700K) produced no statistically significant reduction in eye strain symptoms. In contrast, **reducing screen brightness to 50–60% yielded a statistically significant, clinically meaningful reduction in ocular fatigue severity (-0.82, P = 0.0007)**.
- **Problem Statement**: Standard night-light utilities shift color temperature to warm/orange while leaving screen luminance at 100%. Under low-CCT spectra, pupillary dilation is restricted while high photon flux continues hitting the retina, inducing severe ocular fatigue.
- **Scope & Technical Plan**:
  - Implement dynamic brightness coupling: as color temperature drops from day (6500K) to night (2700K–3400K), automatically attenuate screen brightness along Kruithof's zone of visual comfort (e.g. from 1.0 down to 0.55–0.60).
  - Add configuration setting `couple-brightness = true` and `night-brightness = 0.55`.
  - Add CLI flag `jarheart --couple-brightness` and IPC verb `couple-brightness [on|off]`.
  - Coordinate software gamma multiplication with hardware panel backlight (`/sys/class/backlight`) when root permissions or logind permits.
- **Verification Protocol**:
  - In night transition, verify that both CCT drops to 3400K and CRTC ramp peak scales down to ~60% (e.g. ~39321/65535).

---

### WO-020: Pediatric & Extended Reading Myopia Protection Mode (2700K–3000K)
- **Status**: `TRIAGED`
- **Priority**: `P1 - High`
- **Type**: `Specialized Mode / Ocular Health`
- **Prerequisites**: WO-008, WO-010, WO-019
- **Scientific Foundation**: Chinese Academy of Sciences (Kunming Institute of Zoology) 365-day longitudinal study on juvenile primates (rhesus macaques). Low-CCT artificial illumination (2700K incandescent and 3000K LED) **significantly slowed ocular axial elongation (0.32mm vs 0.49mm/0.46mm, P < 0.05)** by ~40–50% compared to 4000K and 5000K, demonstrating that long-wavelength dominant spectra protect against juvenile axial myopia development.
- **Problem Statement**: Students, software engineers, and children engaged in extended near-work (reading, coding) are exposed to high-CCT screens with heavy short-wavelength blue spikes that stimulate excessive ocular axial elongation.
- **Scope & Technical Plan**:
  - Create dedicated `myopia-protect` mode:
    - Spectrum: Calibrated 2850K (harmonic mean of 2700K incandescent and 3000K warm LED tested in the study).
    - Luminance Cap: Automatically clamps maximum screen luminance to 55–60% (as established in WO-019) to prevent pupil over-dilation while providing high contrast for text rendering.
  - CLI subcommand: `jarheart myopia-protect [on|off|toggle]`.
  - IPC command verb: `myopia-protect`.
  - GTK tray toggle with child/reading eye-health icon.
- **Verification Protocol**:
  - `jarheart myopia-protect on` applies 2850K color curve and clamps brightness to 0.60; verified via `jarheart status -j`.

---

### WO-021: Ergonomic 20-20-20 Ocular Relaxation & Tear-Film Restoration Pacer
- **Status**: `TRIAGED`
- **Priority**: `P2 - Normal`
- **Type**: `Ergonomic Utility / UX`
- **Prerequisites**: WO-005, WO-012, WO-015
- **Scientific Foundation**: Clinical computer vision syndrome research (Talens-Estarelles et al., 2022; Sheppard & Wolffsohn, 2018; Mehra & Galor, 2020). Digital screen use decreases spontaneous blink rate and blink amplitude, leading to evaporative dry eye, tear film breakup, and ciliary muscle spasm.
- **Problem Statement**: Users stare continuously at near displays for hours without blinking or shifting accommodation, causing physical ocular pain and dry eyes regardless of screen color.
- **Scope & Technical Plan**:
  - Implement an internal non-blocking 20-minute pacer in the main `poll()` reactor.
  - Every 20 minutes of active desktop interaction:
    - Option A (Subtle Visual Breathe): Gently pulses display brightness down by 15% over 1.5s and back up over 1.5s as an ambient biological pacing cue.
    - Option B (Notification): Sends a transient FreeDesktop desktop notification: "20-20-20 Rest: Look 20 feet away for 20 seconds to replenish tear film and relax ciliary muscles."
  - CLI subcommand: `jarheart pacer [20m|30m|breathe|notify|off]`.
  - IPC command verb: `pacer`.
- **Verification Protocol**:
  - Test pacer trigger at 5s interval in test harness; verify smooth brightness pulse and D-Bus signal emission.

---

### WO-022: Diurnal Bi-Phasic Alertness-to-Comfort Circadian Schedule
- **Status**: `TRIAGED`
- **Priority**: `P2 - Normal`
- **Type**: `Circadian Optimization`
- **Prerequisites**: WO-006, WO-010
- **Scientific Foundation**: Shi et al. (Building and Environment 2025) and Najjar et al. (IOVS 2022). High CCT (>6000K–6300K) actively stimulates daytime alertness and cognitive performance, while 3000K–4000K optimizes subjective visual comfort, and ≤2700K avoids evening melatonin suppression.
- **Problem Statement**: Standard circadian curves treat the entire daylight period as a static 6500K block. Users experience afternoon cognitive fatigue and visual strain under static blue-rich lighting.
- **Scope & Technical Plan**:
  - Introduce a tri-phasic diurnal transition curve:
    - *Morning Focus Phase* (08:00–12:00): 6500K @ 100% luminance (Peak alertness, S-cone stimulation).
    - *Afternoon Sustained Focus & Comfort Phase* (12:00–17:00): 3800K–4200K @ 80% luminance (Reduced eye fatigue, sustained comfort).
    - *Evening Circadian Wind-Down Phase* (17:00–22:00): Smooth ramp down to 2300K–2700K @ 55% luminance (Melatonin synthesis, axial rest).
    - *Night Rest / Sleep Protection* (22:00+): 1900K (Candle) @ 40% luminance.
  - Configuration key: `schedule = diurnal-triphasic`.
- **Verification Protocol**:
  - Synthetic clock time step progression through 09:00, 14:00, 19:00, 23:00 produces exact expected intermediate curves.

---

### WO-023: Ambient Contrast & Eye-Level Illuminance Balancer
- **Status**: `TRIAGED`
- **Priority**: `P2 - Normal`
- **Type**: `Sensor / Hardware Automation`
- **Prerequisites**: WO-011, WO-014, WO-019
- **Scientific Foundation**: Shi et al. (Building and Environment 2025) on eye vs ground illuminance, and Kaur et al. (2022). Excessive luminance contrast between screen and ambient room surroundings (>3:1) forces constant pupillary readjustment and drives digital eye strain.
- **Problem Statement**: In dark rooms, a 300-nit screen induces severe glare; in bright rooms, a dimmed screen causes squinting and loss of contrast.
- **Scope & Technical Plan**:
  - Dynamically match display luminance to ambient room illuminance reported by IIO ambient lux sensors.
  - Maintain display-to-ambient contrast within optimal physiological ergonomic ratios (1:1 to 3:1).
- **Verification Protocol**:
  - Changing ambient lux from 50 lx (dim room) to 500 lx (office) modulates target screen brightness smoothly from 40% to 100%.

---

## 5. Architectural Verification Matrix

| Verification Vector | Tool / Command | Invariant Requirement | Status |
| :--- | :--- | :--- | :--- |
| **Unit Test Suite** | `ninja -C build test` | 100% pass, execution < 0.10s | **PASS (0.03s)** |
| **Memory Sanitization** | `valgrind --leak-check=full` | Zero byte leaks, zero invalid reads | **PASS** |
| **Hardware Neutral Reversion**| `XRRGetCrtcGamma` (CRTC 0) | `R=65535, G=65535, B=65535` | **PASS (Verified)** |
| **Darkroom Photon Isolation** | `XRRGetCrtcGamma` (CRTC 0) | `Green=0, Blue=0` | **PASS (Verified)** |
| **Movie Mode Highlight Floor**| `XRRGetCrtcGamma` (CRTC 0) | Toe lift `Y^0.88`, Sky blue preservation | **PASS (Verified)** |
| **Compositor Passthrough** | `compiz --replace` | Zero tearing, CRTC downstream of OpenGL | **PASS (Verified)** |
| **IPC Responsiveness** | `jarheart status -j` | Socket response time < 5ms | **PASS (<2ms)** |
| **Desktop Entry Discovery** | FreeDesktop `update-desktop-database` | Appears in System & Utilities menus | **PASS (Verified)** |

---

## 6. Engineering Sign-Off & Attestation

The task rites and work order specifications defined herein represent the permanent operational standard for the Jarheart project. All past, active, and future work orders must comply with the Six Task Rites prior to production release.
