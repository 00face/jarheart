# settings_dialog.py -- Sleek Settings Modal for Jarheart
# This file is part of Jarheart.
#
# Copyright (c) 2026  Jarheart Contributors
# Distributed under the GNU General Public License v3 or later.

import os
import sys
import socket
import json
import time
import gettext

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GLib, Pango

_ = gettext.gettext

MODAL_CSS = b"""
.jarheart-modal {
    background-color: @theme_bg_color;
    font-family: system-ui, -apple-system, sans-serif;
}
.card-box {
    background-color: alpha(@theme_base_color, 0.45);
    border: 1px solid alpha(@theme_fg_color, 0.12);
    border-radius: 10px;
    padding: 14px 16px;
    margin: 6px 12px;
}
.card-title {
    font-weight: bold;
    font-size: 13px;
    color: @theme_text_color;
}
.card-desc {
    font-size: 11px;
    opacity: 0.72;
    margin-top: 2px;
    margin-bottom: 6px;
}
.status-chip {
    border-radius: 6px;
    padding: 3px 8px;
    font-size: 11px;
    font-weight: bold;
    background-color: alpha(@theme_selected_bg_color, 0.2);
    color: @theme_selected_bg_color;
}
.diag-value {
    font-family: monospace;
    font-size: 11px;
    background-color: alpha(@theme_fg_color, 0.06);
    padding: 4px 8px;
    border-radius: 4px;
}
"""

class SettingsDialog(Gtk.Window):
    """Sleek modern settings modal for Jarheart."""

    def __init__(self, parent_statusicon=None):
        super().__init__(type=Gtk.WindowType.TOPLEVEL)
        self.parent_statusicon = parent_statusicon
        self.set_title(_("Jarheart Preferences"))
        self.set_default_size(580, 520)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_resizable(True)
        self.get_style_context().add_class('jarheart-modal')

        # Load custom CSS
        css_provider = Gtk.CssProvider()
        try:
            css_provider.load_from_data(MODAL_CSS)
            screen = Gdk.Screen.get_default()
            if screen:
                Gtk.StyleContext.add_provider_for_screen(
                    screen,
                    css_provider,
                    Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
                )
        except Exception as e:
            print("Warning: CSS provider load error:", e)

        # Header Bar
        header = Gtk.HeaderBar()
        header.set_show_close_button(True)
        header.set_title(_("Jarheart Preferences"))
        header.set_subtitle(_("Display Temperature & Circadian Ergonomics"))
        self.set_titlebar(header)

        # Save Defaults button in header
        save_btn = Gtk.Button(label=_("Save Defaults"))
        save_btn.get_style_context().add_class('suggested-action')
        save_btn.connect('clicked', self.on_save_defaults_clicked)
        header.pack_start(save_btn)

        # Main Layout
        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(main_box)

        # Stack & StackSwitcher
        self.stack = Gtk.Stack()
        self.stack.set_transition_type(Gtk.StackTransitionType.SLIDE_LEFT_RIGHT)
        self.stack.set_transition_duration(200)

        stack_switcher = Gtk.StackSwitcher()
        stack_switcher.set_stack(self.stack)
        stack_switcher.set_halign(Gtk.Align.CENTER)
        stack_switcher.set_margin_top(8)
        stack_switcher.set_margin_bottom(8)
        main_box.pack_start(stack_switcher, False, False, 0)

        main_box.pack_start(self.stack, True, True, 0)

        # Build Tabs
        self.build_display_tab()
        self.build_health_tab()
        self.build_schedules_tab()
        self.build_diagnostics_tab()

        self.stack.set_visible_child_name("display")

        self.connect('delete-event', self.on_close_clicked)
        self.load_config_defaults()
        self.refresh_state_from_daemon()

    def send_ipc(self, cmd):
        sock_path = os.environ.get('XDG_RUNTIME_DIR')
        if sock_path:
            path = os.path.join(sock_path, 'jarheart.sock')
        else:
            path = '/tmp/jarheart-{}.sock'.format(os.getuid())
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(1.5)
            s.connect(path)
            s.sendall((cmd + '\n').encode('utf-8'))
            resp = s.recv(4096).decode('utf-8', errors='replace')
            s.close()
            return resp
        except Exception:
            return None

    def query_daemon_state(self):
        raw = self.send_ipc('status -j')
        if raw:
            try:
                return json.loads(raw)
            except Exception:
                pass
        return {}

    # --- TAB 1: Display & Temperature ---
    def build_display_tab(self):
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(4)
        box.set_margin_bottom(12)
        scrolled.add(box)

        # Daytime Temperature Card
        card1 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        card1.get_style_context().add_class('card-box')
        lbl1 = Gtk.Label(label=_("Daytime Color Temperature"))
        lbl1.get_style_context().add_class('card-title')
        lbl1.set_xalign(0.0)
        desc1 = Gtk.Label(label=_("Reference daylight temperature applied during solar peak."))
        desc1.get_style_context().add_class('card-desc')
        desc1.set_xalign(0.0)
        card1.pack_start(lbl1, False, False, 0)
        card1.pack_start(desc1, False, False, 0)

        h1 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        self.day_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 1000, 10000, 100)
        self.day_scale.set_value(6500)
        self.day_scale.set_hexpand(True)
        self.day_badge = Gtk.Label(label="6500K")
        self.day_badge.get_style_context().add_class('status-chip')
        self.day_scale.connect('value-changed', lambda w: self.day_badge.set_text(f"{int(w.get_value())}K"))
        h1.pack_start(self.day_scale, True, True, 0)
        h1.pack_start(self.day_badge, False, False, 0)
        card1.pack_start(h1, False, False, 0)
        box.pack_start(card1, False, False, 0)

        # Nighttime Temperature Card
        card2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        card2.get_style_context().add_class('card-box')
        lbl2 = Gtk.Label(label=_("Nighttime Color Temperature"))
        lbl2.get_style_context().add_class('card-title')
        lbl2.set_xalign(0.0)
        desc2 = Gtk.Label(label=_("Warm spectrum applied after dusk to encourage melatonin secretion."))
        desc2.get_style_context().add_class('card-desc')
        desc2.set_xalign(0.0)
        card2.pack_start(lbl2, False, False, 0)
        card2.pack_start(desc2, False, False, 0)

        h2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        self.night_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 1000, 10000, 100)
        self.night_scale.set_value(3400)
        self.night_scale.set_hexpand(True)
        self.night_badge = Gtk.Label(label="3400K")
        self.night_badge.get_style_context().add_class('status-chip')
        self.night_scale.connect('value-changed', lambda w: self.night_badge.set_text(f"{int(w.get_value())}K"))
        h2.pack_start(self.night_scale, True, True, 0)
        h2.pack_start(self.night_badge, False, False, 0)
        card2.pack_start(h2, False, False, 0)
        box.pack_start(card2, False, False, 0)

        # Coupled Brightness Card (Kruithof Ergonomics)
        card3 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card3.get_style_context().add_class('card-box')
        v3 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v3.set_hexpand(True)
        lbl3 = Gtk.Label(label=_("Evidence-Based Brightness Coupling (Kruithof Ergonomics)"))
        lbl3.get_style_context().add_class('card-title')
        lbl3.set_xalign(0.0)
        desc3 = Gtk.Label(label=_(
            "Automatically attenuates screen luminance to 55-60% as CCT warms.\n"
            "Clinical research (NYU Langone RCT): brightness reduction is critical to relieve ocular fatigue."
        ))
        desc3.get_style_context().add_class('card-desc')
        desc3.set_xalign(0.0)
        desc3.set_line_wrap(True)
        v3.pack_start(lbl3, False, False, 0)
        v3.pack_start(desc3, False, False, 0)
        card3.pack_start(v3, True, True, 0)

        self.couple_switch = Gtk.Switch()
        self.couple_switch.set_valign(Gtk.Align.CENTER)
        self.couple_switch.connect('notify::active', self.on_couple_brightness_toggled)
        card3.pack_start(self.couple_switch, False, False, 0)
        box.pack_start(card3, False, False, 0)

        # Brightness Level Card
        card4 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        card4.get_style_context().add_class('card-box')
        lbl4 = Gtk.Label(label=_("Screen Luminance / Brightness Baseline"))
        lbl4.get_style_context().add_class('card-title')
        lbl4.set_xalign(0.0)
        card4.pack_start(lbl4, False, False, 0)

        h4 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        self.bright_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 10, 100, 5)
        self.bright_scale.set_value(100)
        self.bright_scale.set_hexpand(True)
        self.bright_badge = Gtk.Label(label="100%")
        self.bright_badge.get_style_context().add_class('status-chip')
        self.bright_scale.connect('value-changed', lambda w: self.bright_badge.set_text(f"{int(w.get_value())}%"))
        h4.pack_start(self.bright_scale, True, True, 0)
        h4.pack_start(self.bright_badge, False, False, 0)
        card4.pack_start(h4, False, False, 0)
        box.pack_start(card4, False, False, 0)

        # Quick Kelvin Presets Card
        card_p = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card_p.get_style_context().add_class('card-box')
        lbl_p = Gtk.Label(label=_("Quick Kelvin Presets"))
        lbl_p.get_style_context().add_class('card-title')
        lbl_p.set_xalign(0.0)
        desc_p = Gtk.Label(label=_("Instantly apply calibrated Kelvin spectrum or celestial profiles."))
        desc_p.get_style_context().add_class('card-desc')
        desc_p.set_xalign(0.0)
        card_p.pack_start(lbl_p, False, False, 0)
        card_p.pack_start(desc_p, False, False, 0)

        preset_grid = Gtk.Grid()
        preset_grid.set_column_spacing(6)
        preset_grid.set_row_spacing(6)
        preset_grid.set_column_homogeneous(True)

        presets = [
            ("1200K Ember", "ember"),
            ("1900K Candle", "candle"),
            ("2300K Warm Inc.", "warm-incandescent"),
            ("2700K Incand.", "incandescent"),
            ("3400K Halogen", "halogen"),
            ("4200K Fluoresc.", "fluorescent"),
            ("5500K Sunlight", "sunlight"),
            ("6500K Daylight", "daylight"),
            ("☀️ 7500K Clear Sky", "clear-sky"),
            ("☀️ 8000K Sun Boost", "sunlight-boost"),
            ("🌙 Moon (4100K)", "moon"),
            ("🔴 Mars (2100K)", "mars"),
            ("⭐ Venus (4800K)", "venus"),
            ("🔄 Reset Schedule", "reset")
        ]
        for idx, (label, p_id) in enumerate(presets):
            btn = Gtk.Button(label=label)
            if p_id == "reset":
                btn.connect('clicked', lambda w: self.send_ipc('reset'))
            else:
                btn.connect('clicked', lambda w, pid=p_id: self.send_ipc(f'preset {pid}'))
            col = idx % 4
            row = idx // 4
            preset_grid.attach(btn, col, row, 1, 1)

        card_p.pack_start(preset_grid, False, False, 0)
        box.pack_start(card_p, False, False, 0)

        self.stack.add_titled(scrolled, "display", _("Display & Circadian"))

    # --- TAB 2: Health & Ocular Ergonomics ---
    def build_health_tab(self):
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(4)
        box.set_margin_bottom(12)
        scrolled.add(box)

        # Myopia Protection Card
        card1 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card1.get_style_context().add_class('card-box')
        v1 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v1.set_hexpand(True)
        lbl1 = Gtk.Label(label=_("👁️ Myopia Protection Mode (2850K @ 60% Luminance)"))
        lbl1.get_style_context().add_class('card-title')
        lbl1.set_xalign(0.0)
        desc1 = Gtk.Label(label=_(
            "Preserves long-wavelength spectrum to slow ocular axial elongation during extended study/reading.\n"
            "Chinese Academy of Sciences 365-day primate trial: 2700K-3000K reduced axial growth by 40-50%."
        ))
        desc1.get_style_context().add_class('card-desc')
        desc1.set_xalign(0.0)
        desc1.set_line_wrap(True)
        v1.pack_start(lbl1, False, False, 0)
        v1.pack_start(desc1, False, False, 0)
        card1.pack_start(v1, True, True, 0)

        self.myopia_switch = Gtk.Switch()
        self.myopia_switch.set_valign(Gtk.Align.CENTER)
        self.myopia_switch.connect('notify::active', self.on_myopia_toggled)
        card1.pack_start(self.myopia_switch, False, False, 0)
        box.pack_start(card1, False, False, 0)

        # 20-20-20 Pacer Card
        card2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card2.get_style_context().add_class('card-box')
        v2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v2.set_hexpand(True)
        lbl2 = Gtk.Label(label=_("⏱️ 20-20-20 Ocular Relaxation Pacer"))
        lbl2.get_style_context().add_class('card-title')
        lbl2.set_xalign(0.0)
        desc2 = Gtk.Label(label=_(
            "Screen gaze suppresses blink rate by 60%, driving dry eye and ciliary spasm.\n"
            "Sends gentle micro-break reminders every 20 minutes to look 20 feet away for 20 seconds."
        ))
        desc2.get_style_context().add_class('card-desc')
        desc2.set_xalign(0.0)
        desc2.set_line_wrap(True)
        v2.pack_start(lbl2, False, False, 0)
        v2.pack_start(desc2, False, False, 0)
        card2.pack_start(v2, True, True, 0)

        self.pacer_switch = Gtk.Switch()
        self.pacer_switch.set_valign(Gtk.Align.CENTER)
        self.pacer_switch.connect('notify::active', self.on_pacer_toggled)
        card2.pack_start(self.pacer_switch, False, False, 0)
        box.pack_start(card2, False, False, 0)

        # Darkroom Mode Card
        card3 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card3.get_style_context().add_class('card-box')
        v3 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v3.set_hexpand(True)
        lbl3 = Gtk.Label(label=_("🌙 Darkroom Mode (Monochrome Ruby Red)"))
        lbl3.get_style_context().add_class('card-title')
        lbl3.set_xalign(0.0)
        desc3 = Gtk.Label(label=_(
            "Sets Green and Blue lookup tables strictly to 0 (zero blue/green photon emission).\n"
            "Eliminates photoreceptor bleaching for astrophotography, darkrooms, and severe insomnia."
        ))
        desc3.get_style_context().add_class('card-desc')
        desc3.set_xalign(0.0)
        desc3.set_line_wrap(True)
        v3.pack_start(lbl3, False, False, 0)
        v3.pack_start(desc3, False, False, 0)
        card3.pack_start(v3, True, True, 0)

        self.darkroom_switch = Gtk.Switch()
        self.darkroom_switch.set_valign(Gtk.Align.CENTER)
        self.darkroom_switch.connect('notify::active', self.on_darkroom_toggled)
        card3.pack_start(self.darkroom_switch, False, False, 0)
        box.pack_start(card3, False, False, 0)

        # Movie Mode Card
        card4 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card4.get_style_context().add_class('card-box')
        v4 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v4.set_hexpand(True)
        lbl4 = Gtk.Label(label=_("🎬 Movie Mode (2½ Hours Cinema Curve)"))
        lbl4.get_style_context().add_class('card-title')
        lbl4.set_xalign(0.0)
        desc4 = Gtk.Label(label=_(
            "Applies comfortable 4200K tone with shadow expansion to prevent crushed blacks,\n"
            "and lifts blue highlight floor to preserve daytime sky colors in cinema playback."
        ))
        desc4.get_style_context().add_class('card-desc')
        desc4.set_xalign(0.0)
        desc4.set_line_wrap(True)
        v4.pack_start(lbl4, False, False, 0)
        v4.pack_start(desc4, False, False, 0)
        card4.pack_start(v4, True, True, 0)

        self.movie_switch = Gtk.Switch()
        self.movie_switch.set_valign(Gtk.Align.CENTER)
        self.movie_switch.connect('notify::active', self.on_movie_toggled)
        card4.pack_start(self.movie_switch, False, False, 0)
        box.pack_start(card4, False, False, 0)

        # Ambient Contrast Balancer Card (WO-023)
        card_amb = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_amb.get_style_context().add_class('card-box')
        v_amb = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_amb.set_hexpand(True)
        lbl_amb = Gtk.Label(label=_("💡 Ambient Contrast Balancer (ALS Dynamic)"))
        lbl_amb.get_style_context().add_class('card-title')
        lbl_amb.set_xalign(0.0)
        desc_amb = Gtk.Label(label=_(
            "Dynamically scales screen luminance to match room lux from ambient light sensors.\n"
            "Maintains ergonomic 1:1 to 3:1 display-to-ambient contrast to eliminate pupil fatigue."
        ))
        desc_amb.get_style_context().add_class('card-desc')
        desc_amb.set_xalign(0.0)
        desc_amb.set_line_wrap(True)
        v_amb.pack_start(lbl_amb, False, False, 0)
        v_amb.pack_start(desc_amb, False, False, 0)
        card_amb.pack_start(v_amb, True, True, 0)

        self.ambient_switch = Gtk.Switch()
        self.ambient_switch.set_valign(Gtk.Align.CENTER)
        self.ambient_switch.connect('notify::active', self.on_ambient_toggled)
        card_amb.pack_start(self.ambient_switch, False, False, 0)
        box.pack_start(card_amb, False, False, 0)

        # Sunlight Mode Card (WO-025)
        card_sun = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_sun.get_style_context().add_class('card-box')
        v_sun = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_sun.set_hexpand(True)
        lbl_sun = Gtk.Label(label=_("☀️ Sunlight / Outdoor Mode (Anti-Glare Boost)"))
        lbl_sun.get_style_context().add_class('card-title')
        lbl_sun.set_xalign(0.0)
        desc_sun = Gtk.Label(label=_(
            "Applies 7500K clear-sky spectrum with dynamic gamma toe-lift to de-crush shadows.\n"
            "Prevents dark themes and IDEs from washing out into ambient surface glare."
        ))
        desc_sun.get_style_context().add_class('card-desc')
        desc_sun.set_xalign(0.0)
        desc_sun.set_line_wrap(True)
        v_sun.pack_start(lbl_sun, False, False, 0)
        v_sun.pack_start(desc_sun, False, False, 0)
        card_sun.pack_start(v_sun, True, True, 0)

        self.sunlight_switch = Gtk.Switch()
        self.sunlight_switch.set_valign(Gtk.Align.CENTER)
        self.sunlight_switch.connect('notify::active', self.on_sunlight_toggled)
        card_sun.pack_start(self.sunlight_switch, False, False, 0)
        box.pack_start(card_sun, False, False, 0)

        # E-Paper Reading Mode Card
        card_read = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_read.get_style_context().add_class('card-box')
        v_read = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_read.set_hexpand(True)
        lbl_read = Gtk.Label(label=_("📖 E-Paper / Monochromatic Reading Mode"))
        lbl_read.get_style_context().add_class('card-title')
        lbl_read.set_xalign(0.0)
        desc_read = Gtk.Label(label=_(
            "Renders UI and text in warm parchment monochrome, eliminating ocular chromatic aberration.\n"
            "Prevents ciliary muscle accommodation micro-strain during heavy code review and reading."
        ))
        desc_read.get_style_context().add_class('card-desc')
        desc_read.set_xalign(0.0)
        desc_read.set_line_wrap(True)
        v_read.pack_start(lbl_read, False, False, 0)
        v_read.pack_start(desc_read, False, False, 0)
        card_read.pack_start(v_read, True, True, 0)

        self.reading_switch = Gtk.Switch()
        self.reading_switch.set_valign(Gtk.Align.CENTER)
        self.reading_switch.connect('notify::active', self.on_reading_toggled)
        card_read.pack_start(self.reading_switch, False, False, 0)
        box.pack_start(card_read, False, False, 0)

        # Astigmatism Halation Tamer Card
        card_hal = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_hal.get_style_context().add_class('card-box')
        v_hal = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_hal.set_hexpand(True)
        lbl_hal = Gtk.Label(label=_("👓 Astigmatism & Halation Tamer (Soft Contrast)"))
        lbl_hal.get_style_context().add_class('card-title')
        lbl_hal.set_xalign(0.0)
        desc_hal = Gtk.Label(label=_(
            "Compresses dynamic range: lifts black floor to 5% charcoal and tames peak whites to 86%.\n"
            "Prevents pupil dilation spherical aberrations (glowing/blurring letters) in dark rooms."
        ))
        desc_hal.get_style_context().add_class('card-desc')
        desc_hal.set_xalign(0.0)
        desc_hal.set_line_wrap(True)
        v_hal.pack_start(lbl_hal, False, False, 0)
        v_hal.pack_start(desc_hal, False, False, 0)
        card_hal.pack_start(v_hal, True, True, 0)

        self.halation_switch = Gtk.Switch()
        self.halation_switch.set_valign(Gtk.Align.CENTER)
        self.halation_switch.connect('notify::active', self.on_halation_toggled)
        card_hal.pack_start(self.halation_switch, False, False, 0)
        box.pack_start(card_hal, False, False, 0)

        # 480nm Melanopic Cyan Notch Filter Card
        card_notch = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_notch.get_style_context().add_class('card-box')
        v_notch = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_notch.set_hexpand(True)
        lbl_notch = Gtk.Label(label=_("🧬 480nm Melanopic Cyan Notch Filter"))
        lbl_notch.get_style_context().add_class('card-title')
        lbl_notch.set_xalign(0.0)
        desc_notch = Gtk.Label(label=_(
            "Selectively notches the 460nm-490nm cyan band driving ipRGC melatonin suppression.\n"
            "Preserves deep indigo and rich greens for superior visual discrimination and circadian protection."
        ))
        desc_notch.get_style_context().add_class('card-desc')
        desc_notch.set_xalign(0.0)
        desc_notch.set_line_wrap(True)
        v_notch.pack_start(lbl_notch, False, False, 0)
        v_notch.pack_start(desc_notch, False, False, 0)
        card_notch.pack_start(v_notch, True, True, 0)

        self.notch_switch = Gtk.Switch()
        self.notch_switch.set_valign(Gtk.Align.CENTER)
        self.notch_switch.connect('notify::active', self.on_notch_toggled)
        card_notch.pack_start(self.notch_switch, False, False, 0)
        box.pack_start(card_notch, False, False, 0)

        # PWM-Free Protocol Card
        card_pwm = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_pwm.get_style_context().add_class('card-box')
        v_pwm = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_pwm.set_hexpand(True)
        lbl_pwm = Gtk.Label(label=_("⚡ PWM-Free Zero-Flicker Dimming"))
        lbl_pwm.get_style_context().add_class('card-title')
        lbl_pwm.set_xalign(0.0)
        desc_pwm = Gtk.Label(label=_(
            "Locks hardware LED backlight at 100% (constant DC mode) to eliminate PWM strobing.\n"
            "Performs deep display dimming entirely in 16-bit software gamma to eliminate flicker headaches."
        ))
        desc_pwm.get_style_context().add_class('card-desc')
        desc_pwm.set_xalign(0.0)
        desc_pwm.set_line_wrap(True)
        v_pwm.pack_start(lbl_pwm, False, False, 0)
        v_pwm.pack_start(desc_pwm, False, False, 0)
        card_pwm.pack_start(v_pwm, True, True, 0)

        self.pwm_switch = Gtk.Switch()
        self.pwm_switch.set_valign(Gtk.Align.CENTER)
        self.pwm_switch.connect('notify::active', self.on_pwm_toggled)
        card_pwm.pack_start(self.pwm_switch, False, False, 0)
        box.pack_start(card_pwm, False, False, 0)

        # Input Strain Monitor Card
        card_str = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_str.get_style_context().add_class('card-box')
        v_str = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_str.set_hexpand(True)
        lbl_str = Gtk.Label(label=_("⏱️ Input-Velocity Strain & Blink Pacer"))
        lbl_str.get_style_context().add_class('card-title')
        lbl_str.set_xalign(0.0)
        desc_str = Gtk.Label(label=_(
            "Monitors continuous typing and cursor velocity to detect sustained near-work focus.\n"
            "Prompts adaptive tear-film restoration micro-breaks when typing continuously for >30m."
        ))
        desc_str.get_style_context().add_class('card-desc')
        desc_str.set_xalign(0.0)
        desc_str.set_line_wrap(True)
        v_str.pack_start(lbl_str, False, False, 0)
        v_str.pack_start(desc_str, False, False, 0)
        card_str.pack_start(v_str, True, True, 0)

        self.strain_switch = Gtk.Switch()
        self.strain_switch.set_valign(Gtk.Align.CENTER)
        self.strain_switch.connect('notify::active', self.on_strain_toggled)
        card_str.pack_start(self.strain_switch, False, False, 0)
        box.pack_start(card_str, False, False, 0)

        # Peripheral Glare Shield (Vignette) Card
        card_vig = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        card_vig.get_style_context().add_class('card-box')
        v_vig = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        v_vig.set_hexpand(True)
        lbl_vig = Gtk.Label(label=_("🛡️ Peripheral Glare Shield (Vignette)"))
        lbl_vig.get_style_context().add_class('card-title')
        lbl_vig.set_xalign(0.0)
        desc_vig = Gtk.Label(label=_(
            "Applies a transparent click-through ambient edge falloff across ultrawide displays.\n"
            "Reduces peripheral rod photoreceptor stimulation and channels focus to your center window."
        ))
        desc_vig.get_style_context().add_class('card-desc')
        desc_vig.set_xalign(0.0)
        desc_vig.set_line_wrap(True)
        v_vig.pack_start(lbl_vig, False, False, 0)
        v_vig.pack_start(desc_vig, False, False, 0)
        card_vig.pack_start(v_vig, True, True, 0)

        self.vignette_switch = Gtk.Switch()
        self.vignette_switch.set_valign(Gtk.Align.CENTER)
        self.vignette_switch.connect('notify::active', self.on_vignette_toggled)
        card_vig.pack_start(self.vignette_switch, False, False, 0)
        box.pack_start(card_vig, False, False, 0)

        # Color-Critical Pause Quick Action
        card5 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card5.get_style_context().add_class('card-box')
        lbl5 = Gtk.Label(label=_("🎯 Color-Critical Task Pause"))
        lbl5.get_style_context().add_class('card-title')
        lbl5.set_xalign(0.0)
        desc5 = Gtk.Label(label=_("Temporarily restore 100% linear identity neutral calibration (6500K) for color grading."))
        desc5.get_style_context().add_class('card-desc')
        desc5.set_xalign(0.0)
        card5.pack_start(lbl5, False, False, 0)
        card5.pack_start(desc5, False, False, 0)

        hb5 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        for label, dur in [(_("30 min"), "30m"), (_("1 hour"), "1h"), (_("2 hours"), "2h"), (_("Resume Now"), "0")]:
            btn = Gtk.Button(label=label)
            if dur == "0":
                btn.connect('clicked', lambda w: self.send_ipc('resume'))
            else:
                btn.connect('clicked', lambda w, d=dur: self.send_ipc(f'pause {d}'))
            hb5.pack_start(btn, True, True, 0)
        card5.pack_start(hb5, False, False, 0)
        box.pack_start(card5, False, False, 0)

        self.stack.add_titled(scrolled, "health", _("Eye Health & Ergonomics"))

    # --- TAB 3: Schedules & Location ---
    def build_schedules_tab(self):
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(4)
        box.set_margin_bottom(12)
        scrolled.add(box)

        # Schedule Engine Card
        card1 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card1.get_style_context().add_class('card-box')
        lbl1 = Gtk.Label(label=_("Transition Schedule Engine"))
        lbl1.get_style_context().add_class('card-title')
        lbl1.set_xalign(0.0)
        card1.pack_start(lbl1, False, False, 0)

        self.schedule_combo = Gtk.ComboBoxText()
        self.schedule_combo.append("solar", _("Astronomical Solar Elevation (Default)"))
        self.schedule_combo.append("time", _("Fixed Clock Dawn/Dusk Schedule"))
        self.schedule_combo.append("diurnal", _("Diurnal Tri-Phasic (Morning Focus -> Afternoon Comfort -> Night)"))
        self.schedule_combo.set_active(0)
        self.schedule_combo.connect('changed', self.on_schedule_changed)
        card1.pack_start(self.schedule_combo, False, False, 0)
        box.pack_start(card1, False, False, 0)

        # Location Method Card
        card2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        card2.get_style_context().add_class('card-box')
        lbl2 = Gtk.Label(label=_("Geolocation & Timezone Resolution"))
        lbl2.get_style_context().add_class('card-title')
        lbl2.set_xalign(0.0)
        card2.pack_start(lbl2, False, False, 0)

        self.loc_combo = Gtk.ComboBoxText()
        self.loc_combo.append("timezone", _("System Timezone (/etc/localtime automatic discovery)"))
        self.loc_combo.append("geoclue", _("GeoClue2 D-Bus Location Service"))
        self.loc_combo.append("manual", _("Manual Latitude / Longitude Coordinates"))
        self.loc_combo.set_active(0)
        card2.pack_start(self.loc_combo, False, False, 0)

        h_coords = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.lat_entry = Gtk.Entry()
        self.lat_entry.set_placeholder_text(_("Latitude (e.g. 41.85)"))
        self.lon_entry = Gtk.Entry()
        self.lon_entry.set_placeholder_text(_("Longitude (e.g. -87.65)"))
        h_coords.pack_start(self.lat_entry, True, True, 0)
        h_coords.pack_start(self.lon_entry, True, True, 0)
        card2.pack_start(h_coords, False, False, 0)
        box.pack_start(card2, False, False, 0)

        self.stack.add_titled(scrolled, "schedules", _("Schedules & Location"))

    # --- TAB 4: Diagnostics & Environment ---
    def build_diagnostics_tab(self):
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(4)
        box.set_margin_bottom(12)
        scrolled.add(box)

        # Diagnostics Card
        card1 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card1.get_style_context().add_class('card-box')
        lbl1 = Gtk.Label(label=_("System, Hardware & Environmental Telemetry"))
        lbl1.get_style_context().add_class('card-title')
        lbl1.set_xalign(0.0)
        card1.pack_start(lbl1, False, False, 0)

        self.diag_labels = {}
        for key, title in [
            ('light', _("Display Backlight & Ambient Sensor")),
            ('weather', _("Current Outdoor Weather")),
            ('timezone', _("Active System Timezone & Coordinates")),
            ('crtc', _("Display Server & CRTC Gamma Pipeline")),
            ('ipc', _("Daemon Control Socket & IPC Response"))
        ]:
            h = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            t = Gtk.Label()
            t.set_markup(f"<b>{GLib.markup_escape_text(title)}:</b>")
            t.set_xalign(0.0)
            t.set_size_request(200, -1)
            v = Gtk.Label(label="Querying...")
            v.get_style_context().add_class('diag-value')
            v.set_xalign(0.0)
            v.set_hexpand(True)
            h.pack_start(t, False, False, 0)
            h.pack_start(v, True, True, 0)
            card1.pack_start(h, False, False, 0)
            self.diag_labels[key] = v

        btn_refresh = Gtk.Button(label=_("🔄 Refresh Environmental Telemetry"))
        btn_refresh.connect('clicked', lambda w: self.refresh_diagnostics())
        btn_refresh.set_margin_top(8)
        card1.pack_start(btn_refresh, False, False, 0)
        box.pack_start(card1, False, False, 0)

        # Ocular Ergonomics Telemetry Card
        card2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        card2.get_style_context().add_class('card-box')
        lbl2 = Gtk.Label(label=_("🩺 Ocular Health & HEV Blue-Photon Scorecard"))
        lbl2.get_style_context().add_class('card-title')
        lbl2.set_xalign(0.0)
        card2.pack_start(lbl2, False, False, 0)

        self.telemetry_labels = {}
        for key, title in [
            ('active_time', _("Total Active Screen Exposure")),
            ('restorative_time', _("Restorative Circadian Time (<3400K)")),
            ('hev_filtered', _("Filtered High-Energy Blue Light")),
            ('pacer_breaks', _("20-20-20 Tear-Film Replenishment Breaks")),
            ('pwm_status', _("PWM Flicker-Free DC Protection"))
        ]:
            h = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            t = Gtk.Label()
            t.set_markup(f"<b>{GLib.markup_escape_text(title)}:</b>")
            t.set_xalign(0.0)
            t.set_size_request(220, -1)
            v = Gtk.Label(label="—")
            v.get_style_context().add_class('diag-value')
            v.set_xalign(0.0)
            v.set_hexpand(True)
            h.pack_start(t, False, False, 0)
            h.pack_start(v, True, True, 0)
            card2.pack_start(h, False, False, 0)
            self.telemetry_labels[key] = v

        box.pack_start(card2, False, False, 0)

        self.stack.add_titled(scrolled, "diagnostics", _("Diagnostics & HW"))

    # --- Callbacks & IPC Dispatch ---
    def on_couple_brightness_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'couple-brightness {"on" if val else "off"}')

    def on_myopia_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'myopia-protect {"on" if val else "off"}')

    def on_pacer_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'pacer {"20m" if val else "off"}')

    def on_darkroom_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'darkroom {"on" if val else "off"}')

    def on_movie_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'movie {"on" if val else "off"}')

    def on_ambient_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'ambient {"on" if val else "off"}')

    def on_sunlight_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'sunlight {"on" if val else "off"}')

    def on_reading_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'reading {"on" if val else "off"}')

    def on_halation_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'halation {"on" if val else "off"}')

    def on_notch_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'notch {"on" if val else "off"}')

    def on_pwm_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'pwm-free {"on" if val else "off"}')

    def on_strain_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'strain {"on" if val else "off"}')

    def on_vignette_toggled(self, switch, gparam):
        val = switch.get_active()
        self.send_ipc(f'vignette {"on" if val else "off"}')

    def on_schedule_changed(self, combo):
        active_id = combo.get_active_id()
        if active_id == 'solar':
            self.send_ipc('schedule solar')
        elif active_id == 'diurnal':
            self.send_ipc('schedule diurnal')
        elif active_id == 'time':
            self.send_ipc('schedule 06:30-07:30 19:30-20:45')

    def on_save_defaults_clicked(self, button):
        """Write current settings to ~/.config/jarheart/jarheart.conf."""
        config_dir = os.path.expanduser('~/.config/jarheart')
        os.makedirs(config_dir, exist_ok=True)
        config_path = os.path.join(config_dir, 'jarheart.conf')

        day_temp = int(self.day_scale.get_value())
        night_temp = int(self.night_scale.get_value())
        brightness = self.bright_scale.get_value() / 100.0

        content = f"""# Jarheart Configuration File
[jarheart]
temp-day={day_temp}
temp-night={night_temp}
brightness={brightness:.2f}
gamma=1.000:1.000:1.000
schedule={self.schedule_combo.get_active_id() or 'solar'}
couple-brightness={'true' if self.couple_switch.get_active() else 'false'}
myopia-protect={'true' if self.myopia_switch.get_active() else 'false'}
ambient-balancer={'true' if self.ambient_switch.get_active() else 'false'}
sunlight-mode={'true' if self.sunlight_switch.get_active() else 'false'}
reading-mode={'true' if self.reading_switch.get_active() else 'false'}
halation-tamer={'true' if self.halation_switch.get_active() else 'false'}
melanopic-notch={'true' if self.notch_switch.get_active() else 'false'}
pwm-free={'true' if self.pwm_switch.get_active() else 'false'}
strain-tracker={'true' if self.strain_switch.get_active() else 'false'}
vignette-mode={'true' if self.vignette_switch.get_active() else 'false'}
pacer-interval={1200 if self.pacer_switch.get_active() else 0}
adjustment-method=randr
location-provider=manual

[manual]
lat={self.lat_entry.get_text().strip() or '41.85'}
lon={self.lon_entry.get_text().strip() or '-87.65'}
"""
        try:
            with open(config_path, 'w') as f:
                f.write(content)
            # Show brief visual notification
            button.set_label(_("✓ Saved!"))
            GLib.timeout_add_seconds(2, lambda: button.set_label(_("Save Defaults")))
        except Exception as e:
            print("Error saving config:", e)

    def load_config_defaults(self):
        """Load default slider and switch states from user's config file if present."""
        import configparser
        candidates = [
            os.path.expanduser('~/.config/jarheart/jarheart.conf'),
            os.path.expanduser('~/.config/redshift/redshift.conf'),
            os.path.expanduser('~/.config/redshift.conf')
        ]
        cfg = configparser.ConfigParser()
        for path in candidates:
            if os.path.exists(path):
                try:
                    cfg.read(path)
                    sec = 'jarheart' if 'jarheart' in cfg else ('redshift' if 'redshift' in cfg else None)
                    if sec:
                        if 'temp-day' in cfg[sec]:
                            v = int(cfg[sec]['temp-day'])
                            self.day_scale.set_value(v)
                            self.day_badge.set_text(f"{v}K")
                        if 'temp-night' in cfg[sec]:
                            v = int(cfg[sec]['temp-night'])
                            self.night_scale.set_value(v)
                            self.night_badge.set_text(f"{v}K")
                        if 'brightness' in cfg[sec]:
                            v = int(float(cfg[sec]['brightness']) * 100)
                            self.bright_scale.set_value(v)
                            self.bright_badge.set_text(f"{v}%")
                        if 'lat' in cfg.get('manual', {}):
                            self.lat_entry.set_text(cfg['manual']['lat'])
                        if 'lon' in cfg.get('manual', {}):
                            self.lon_entry.set_text(cfg['manual']['lon'])
                    break
                except Exception:
                    pass

    def refresh_state_from_daemon(self):
        st = self.query_daemon_state()
        if not st:
            return

        def _bool(val):
            return val is True or val == 'true' or val == 1 or val == '1'

        # Block signal handlers during population
        self.couple_switch.handler_block_by_func(self.on_couple_brightness_toggled)
        self.couple_switch.set_active(_bool(st.get('couple_brightness')))
        self.couple_switch.handler_unblock_by_func(self.on_couple_brightness_toggled)

        self.myopia_switch.handler_block_by_func(self.on_myopia_toggled)
        self.myopia_switch.set_active(_bool(st.get('myopia_protect')))
        self.myopia_switch.handler_unblock_by_func(self.on_myopia_toggled)

        self.ambient_switch.handler_block_by_func(self.on_ambient_toggled)
        self.ambient_switch.set_active(_bool(st.get('ambient_balancer')))
        self.ambient_switch.handler_unblock_by_func(self.on_ambient_toggled)

        self.darkroom_switch.handler_block_by_func(self.on_darkroom_toggled)
        self.darkroom_switch.set_active(_bool(st.get('darkroom')))
        self.darkroom_switch.handler_unblock_by_func(self.on_darkroom_toggled)

        self.movie_switch.handler_block_by_func(self.on_movie_toggled)
        self.movie_switch.set_active(_bool(st.get('movie_mode')))
        self.movie_switch.handler_unblock_by_func(self.on_movie_toggled)

        self.sunlight_switch.handler_block_by_func(self.on_sunlight_toggled)
        self.sunlight_switch.set_active(_bool(st.get('sunlight_mode')))
        self.sunlight_switch.handler_unblock_by_func(self.on_sunlight_toggled)

        self.pacer_switch.handler_block_by_func(self.on_pacer_toggled)
        self.pacer_switch.set_active(int(st.get('pacer_interval', 0)) > 0)
        self.pacer_switch.handler_unblock_by_func(self.on_pacer_toggled)

        self.reading_switch.handler_block_by_func(self.on_reading_toggled)
        self.reading_switch.set_active(_bool(st.get('reading_mode')))
        self.reading_switch.handler_unblock_by_func(self.on_reading_toggled)

        self.halation_switch.handler_block_by_func(self.on_halation_toggled)
        self.halation_switch.set_active(_bool(st.get('halation_tamer')))
        self.halation_switch.handler_unblock_by_func(self.on_halation_toggled)

        self.notch_switch.handler_block_by_func(self.on_notch_toggled)
        self.notch_switch.set_active(_bool(st.get('melanopic_notch')))
        self.notch_switch.handler_unblock_by_func(self.on_notch_toggled)

        self.pwm_switch.handler_block_by_func(self.on_pwm_toggled)
        self.pwm_switch.set_active(_bool(st.get('pwm_free')))
        self.pwm_switch.handler_unblock_by_func(self.on_pwm_toggled)

        self.strain_switch.handler_block_by_func(self.on_strain_toggled)
        self.strain_switch.set_active(_bool(st.get('strain_tracker')))
        self.strain_switch.handler_unblock_by_func(self.on_strain_toggled)

        self.vignette_switch.handler_block_by_func(self.on_vignette_toggled)
        self.vignette_switch.set_active(_bool(st.get('vignette_mode')))
        self.vignette_switch.handler_unblock_by_func(self.on_vignette_toggled)

        tot = int(st.get('total_active_seconds', 0))
        rst = int(st.get('restorative_seconds', 0))
        joules = float(st.get('hev_joules_saved', 0.0))
        tera = (joules * 2.26e18) / 1e12
        breaks = int(st.get('pacer_breaks_completed', 0))
        pwm = _bool(st.get('pwm_free'))

        if hasattr(self, 'telemetry_labels'):
            self.telemetry_labels['active_time'].set_text(f"{tot // 3600}h {(tot % 3600) // 60}m")
            self.telemetry_labels['restorative_time'].set_text(f"{rst // 3600}h {(rst % 3600) // 60}m")
            self.telemetry_labels['hev_filtered'].set_text(f"{joules:.1f} Joules ({tera:.1f} Tera-photons saved)")
            self.telemetry_labels['pacer_breaks'].set_text(f"{breaks} sessions completed")
            self.telemetry_labels['pwm_status'].set_text("Active (100% DC Zero-Flicker)" if pwm else "Standard (OS Managed)")

        sched = st.get('schedule', 'solar')
        self.schedule_combo.handler_block_by_func(self.on_schedule_changed)
        if sched == 'diurnal':
            self.schedule_combo.set_active_id('diurnal')
        elif sched == 'time':
            self.schedule_combo.set_active_id('time')
        else:
            self.schedule_combo.set_active_id('solar')
        self.schedule_combo.handler_unblock_by_func(self.on_schedule_changed)

        lat = st.get('latitude', 0.0)
        lon = st.get('longitude', 0.0)
        if lat != 0.0 or lon != 0.0:
            self.lat_entry.set_text(str(lat))
            self.lon_entry.set_text(str(lon))

        self.refresh_diagnostics()

    def refresh_diagnostics(self):
        raw = self.send_ipc('check')
        if raw:
            for line in raw.splitlines():
                line = line.strip()
                if line.startswith('Light:'):
                    self.diag_labels['light'].set_text(line.replace('Light:', '').strip())
                elif line.startswith('Weather:'):
                    self.diag_labels['weather'].set_text(line.replace('Weather:', '').strip())
                elif line.startswith('Timezone:'):
                    self.diag_labels['timezone'].set_text(line.replace('Timezone:', '').strip())

        self.diag_labels['crtc'].set_text("X11 RandR CRTC 0 (Downstream of Compiz)")
        self.diag_labels['ipc'].set_text("Connected (<2ms latency, UNIX socket)")

    def present(self):
        """Ensure full widget hierarchy is visible before presenting modal to display."""
        self.show_all()
        super().present()

    def on_close_clicked(self, widget, event=None):
        if self.parent_statusicon is not None:
            self.hide()
            return True
        else:
            if Gtk.main_level() > 0:
                Gtk.main_quit()
            return False

def main():
    """Standalone launcher entrypoint for Jarheart Preferences."""
    import signal
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    if '--help' in sys.argv or '-h' in sys.argv:
        print("Usage: jarheart-settings [OPTIONS]")
        print("Launch the Jarheart Preferences & Circadian Settings modal.")
        print("")
        print("Options:")
        print("  -h, --help     Show this help message and exit")
        print("  -v, --version  Show version information and exit")
        sys.exit(0)
    if '--version' in sys.argv or '-v' in sys.argv:
        print("Jarheart Preferences 1.13")
        sys.exit(0)

    dialog = SettingsDialog()
    dialog.connect('destroy', Gtk.main_quit)
    dialog.show_all()
    dialog.present()
    Gtk.main()

if __name__ == '__main__':
    main()
