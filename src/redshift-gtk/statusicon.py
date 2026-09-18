# statusicon.py -- GUI status icon source
# This file is part of Redshift.

# Redshift is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# Redshift is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

# Copyright (c) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>


"""GUI status icon for Redshift.

The run method will try to start an appindicator for Redshift. If the
appindicator module isn't present it will fall back to a GTK status icon.
"""

import sys
import os
import socket
import json
import signal
import gettext

import gi
gi.require_version('Gtk', '3.0')

from gi.repository import Gtk, GLib

try:
    gi.require_version('AyatanaAppIndicator3', '0.1')
    from gi.repository import AyatanaAppIndicator3 as appindicator
except (ImportError, ValueError):
    try:
        gi.require_version('AppIndicator3', '0.1')
        from gi.repository import AppIndicator3 as appindicator
    except (ImportError, ValueError):
        appindicator = None

from .controller import RedshiftController
from . import defs
from . import utils

try:
    from .settings_dialog import SettingsDialog
except (ImportError, ValueError):
    try:
        from settings_dialog import SettingsDialog
    except (ImportError, ValueError):
        SettingsDialog = None

_ = gettext.gettext


class RedshiftStatusIcon(object):
    """The status icon tracking the RedshiftController."""

    def __init__(self, controller):
        """Creates a new instance of the status icon."""

        self._controller = controller

        self.icon_theme = Gtk.IconTheme.get_default()
        icon_name = 'jarheart-status-on-symbolic'
        if not self.icon_theme.has_icon(icon_name):
            icon_name = 'jarheart-status-on'
        if not self.icon_theme.has_icon(icon_name):
            icon_name = 'redshift-status-on-symbolic'
        if not self.icon_theme.has_icon(icon_name):
            icon_name = 'redshift-status-on'

        if appindicator:
            # Create indicator
            self.indicator = appindicator.Indicator.new(
                'jarheart',
                icon_name,
                appindicator.IndicatorCategory.APPLICATION_STATUS)
            self.indicator.set_status(appindicator.IndicatorStatus.ACTIVE)
        else:
            # Create status icon
            self.status_icon = Gtk.StatusIcon()
            self.status_icon.set_from_icon_name(icon_name)
            self.status_icon.set_tooltip_text('Jarheart')

        # Create popup menu
        self.status_menu = Gtk.Menu()

        # Add toggle action
        self.toggle_item = Gtk.CheckMenuItem.new_with_label(_('Enabled'))
        self.toggle_item.connect('activate', self.toggle_item_cb)
        self.status_menu.append(self.toggle_item)

        # Add Darkroom mode action
        self.darkroom_item = Gtk.CheckMenuItem.new_with_label(_('Darkroom Mode (Ruby Red)'))
        self.darkroom_item.connect('toggled', self.darkroom_toggle_cb)
        self.status_menu.append(self.darkroom_item)

        # Add Movie mode action
        self.movie_item = Gtk.CheckMenuItem.new_with_label(_('Movie Mode (2½ hours)'))
        self.movie_item.connect('toggled', self.movie_toggle_cb)
        self.status_menu.append(self.movie_item)

        # Add Myopia Protection mode action
        self.myopia_item = Gtk.CheckMenuItem.new_with_label(_('Myopia Protection (2850K)'))
        self.myopia_item.connect('toggled', self.myopia_toggle_cb)
        self.status_menu.append(self.myopia_item)

        # Add Coupled Brightness action (Kruithof rule)
        self.couple_item = Gtk.CheckMenuItem.new_with_label(_('Couple Brightness (Kruithof)'))
        self.couple_item.connect('toggled', self.couple_toggle_cb)
        self.status_menu.append(self.couple_item)

        # Add Presets submenu
        presets_menu_item = Gtk.MenuItem.new_with_label(_('Presets'))
        presets_menu = Gtk.Menu()
        presets_list = [
            ('ember', _('Ember (1200K)')),
            ('candle', _('Candle (1900K)')),
            ('mars', _('Mars (2100K)')),
            ('warm-incandescent', _('Warm Incandescent (2300K)')),
            ('incandescent', _('Incandescent (2700K)')),
            ('myopia-protect', _('Myopia Protect (2850K)')),
            ('jupiter', _('Jupiter (3200K)')),
            ('halogen', _('Halogen (3400K)')),
            ('saturn', _('Saturn (3800K)')),
            ('moon', _('Moon (4100K)')),
            ('fluorescent', _('Fluorescent (4200K)')),
            ('venus', _('Venus (4800K)')),
            ('sunlight', _('Sunlight (5500K)')),
            ('mercury', _('Mercury (5800K)')),
            ('daylight', _('Daylight (6500K)')),
        ]
        for p_name, p_label in presets_list:
            p_item = Gtk.MenuItem.new_with_label(p_label)
            p_item.connect('activate', self.preset_cb, p_name)
            presets_menu.append(p_item)

        presets_menu.append(Gtk.SeparatorMenuItem())
        reset_preset_item = Gtk.MenuItem.new_with_label(_('Reset to Solar Schedule'))
        reset_preset_item.connect('activate', self.reset_preset_cb)
        presets_menu.append(reset_preset_item)

        presets_menu_item.set_submenu(presets_menu)
        self.status_menu.append(presets_menu_item)

        # Add 20-20-20 Ocular Pacer submenu
        pacer_menu_item = Gtk.MenuItem.new_with_label(_('⏱️ 20-20-20 Ocular Pacer'))
        pacer_menu = Gtk.Menu()
        for p_interval, p_title in [
            ('20m', _('Every 20 minutes (Standard)')),
            ('30m', _('Every 30 minutes (Extended)')),
            ('off', _('Disable Pacer'))
        ]:
            p_sub_item = Gtk.MenuItem.new_with_label(p_title)
            p_sub_item.connect('activate', self.pacer_cb, p_interval)
            pacer_menu.append(p_sub_item)

        pacer_menu.append(Gtk.SeparatorMenuItem())
        p_breathe = Gtk.MenuItem.new_with_label(_('Trigger Screen Breathe Now'))
        p_breathe.connect('activate', self.pacer_cb, 'breathe')
        pacer_menu.append(p_breathe)

        p_notify = Gtk.MenuItem.new_with_label(_('Test Notification Now'))
        p_notify.connect('activate', self.pacer_cb, 'notify')
        pacer_menu.append(p_notify)

        pacer_menu_item.set_submenu(pacer_menu)
        self.status_menu.append(pacer_menu_item)

        # Add Color-Critical Pause menu
        suspend_menu_item = Gtk.MenuItem.new_with_label(_('Color-Critical Pause'))
        suspend_menu = Gtk.Menu()
        for minutes, label in [(30, _('30 minutes')),
                               (60, _('1 hour')),
                               (120, _('2 hours')),
                               (240, _('4 hours')),
                               (480, _('8 hours'))]:
            suspend_item = Gtk.MenuItem.new_with_label(label)
            suspend_item.connect('activate', self.suspend_cb, minutes)
            suspend_menu.append(suspend_item)
        suspend_menu_item.set_submenu(suspend_menu)
        self.status_menu.append(suspend_menu_item)

        self.status_menu.append(Gtk.SeparatorMenuItem())

        # Add Preferences & Settings modal action
        settings_item = Gtk.MenuItem.new_with_label(_('⚙️ Preferences & Settings…'))
        settings_item.connect('activate', self.show_settings_cb)
        self.status_menu.append(settings_item)

        # Add autostart option
        if utils.supports_autostart():
            autostart_item = Gtk.CheckMenuItem.new_with_label(_('Autostart'))
            try:
                autostart_item.set_active(utils.get_autostart())
            except IOError as strerror:
                print(strerror)
                autostart_item.set_property('sensitive', False)
            else:
                autostart_item.connect('toggled', self.autostart_cb)
            finally:
                self.status_menu.append(autostart_item)

        # Add info action
        info_item = Gtk.MenuItem.new_with_label(_('Info'))
        info_item.connect('activate', self.show_info_cb)
        self.status_menu.append(info_item)

        # Add quit action
        quit_item = Gtk.ImageMenuItem.new_with_label(_('Quit'))
        quit_item.connect('activate', self.destroy_cb)
        self.status_menu.append(quit_item)

        # Initialize settings dialog
        self.settings_dialog = None

        # Create info dialog
        self.info_dialog = Gtk.Window(title=_('Info'))
        self.info_dialog.set_resizable(False)
        self.info_dialog.set_property('border-width', 6)
        self.info_dialog.connect('delete-event', self.close_info_dialog_cb)

        content_area = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.info_dialog.add(content_area)
        content_area.show()

        self.status_label = Gtk.Label()
        self.status_label.set_alignment(0.0, 0.5)
        self.status_label.set_padding(6, 6)
        content_area.pack_start(self.status_label, True, True, 0)
        self.status_label.show()

        self.location_label = Gtk.Label()
        self.location_label.set_alignment(0.0, 0.5)
        self.location_label.set_padding(6, 6)
        content_area.pack_start(self.location_label, True, True, 0)
        self.location_label.show()

        self.temperature_label = Gtk.Label()
        self.temperature_label.set_alignment(0.0, 0.5)
        self.temperature_label.set_padding(6, 6)
        content_area.pack_start(self.temperature_label, True, True, 0)
        self.temperature_label.show()

        self.period_label = Gtk.Label()
        self.period_label.set_alignment(0.0, 0.5)
        self.period_label.set_padding(6, 6)
        content_area.pack_start(self.period_label, True, True, 0)
        self.period_label.show()

        self.checks_label = Gtk.Label()
        self.checks_label.set_alignment(0.0, 0.5)
        self.checks_label.set_padding(6, 6)
        content_area.pack_start(self.checks_label, True, True, 0)
        self.checks_label.show()

        self.close_button = Gtk.Button(label=_('Close'))
        content_area.pack_start(self.close_button, True, True, 0)
        self.close_button.connect('clicked', self.close_info_dialog_cb)
        self.close_button.show()

        # Setup signals to property changes
        self._controller.connect('inhibit-changed', self.inhibit_change_cb)
        self._controller.connect('period-changed', self.period_change_cb)
        self._controller.connect(
            'temperature-changed', self.temperature_change_cb)
        self._controller.connect('location-changed', self.location_change_cb)
        self._controller.connect('error-occured', self.error_occured_cb)
        self._controller.connect('stopped', self.controller_stopped_cb)
        self.icon_theme.connect('changed', self.on_icon_theme_changed_cb)

        # Set info box text
        self.change_inhibited(self._controller.inhibited)
        self.change_period(self._controller.period)
        self.change_temperature(self._controller.temperature)
        self.change_location(self._controller.location)

        if appindicator:
            self.status_menu.show_all()

            # Set the menu
            self.indicator.set_menu(self.status_menu)
        else:
            # Connect signals for status icon and show
            self.status_icon.connect('activate', self.toggle_cb)
            self.status_icon.connect('popup-menu', self.popup_menu_cb)
            self.status_icon.set_visible(True)

        # Initialize suspend timer
        self.suspend_timer = None

    def remove_suspend_timer(self):
        """Disable any previously set suspend timer."""
        if self.suspend_timer is not None:
            GLib.source_remove(self.suspend_timer)
            self.suspend_timer = None

    def suspend_cb(self, item, minutes):
        """Callback that handles activation of a suspend timer.

        The minutes parameter is the number of minutes to suspend. Even if
        redshift is not disabled when called, it will still set a suspend timer
        and reactive redshift when the timer is up.
        """
        # Inhibit
        self._controller.set_inhibit(True)

        # If "suspend" is clicked while redshift is disabled, we reenable
        # it after the last selected timespan is over.
        self.remove_suspend_timer()

        # If redshift was already disabled we reenable it nonetheless.
        self.suspend_timer = GLib.timeout_add_seconds(
            minutes * 60, self.reenable_cb)

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

    def darkroom_toggle_cb(self, widget):
        if widget.get_active():
            self.send_ipc('darkroom on')
        else:
            self.send_ipc('darkroom off')

    def movie_toggle_cb(self, widget):
        if widget.get_active():
            self.send_ipc('movie on')
        else:
            self.send_ipc('movie off')

    def myopia_toggle_cb(self, widget):
        if widget.get_active():
            self.send_ipc('myopia-protect on')
        else:
            self.send_ipc('myopia-protect off')

    def couple_toggle_cb(self, widget):
        if widget.get_active():
            self.send_ipc('couple-brightness on')
        else:
            self.send_ipc('couple-brightness off')

    def pacer_cb(self, widget, mode):
        self.send_ipc('pacer ' + mode)

    def show_settings_cb(self, widget, data=None):
        if SettingsDialog is None:
            return
        if self.settings_dialog is None:
            self.settings_dialog = SettingsDialog(parent_statusicon=self)
        self.settings_dialog.refresh_state_from_daemon()
        self.settings_dialog.present()

    def preset_cb(self, widget, preset_name):
        self.send_ipc('preset ' + preset_name)

    def reset_preset_cb(self, widget):
        self.send_ipc('reset')

    def reenable_cb(self):
        """Callback to reenable redshift when a suspend timer expires."""
        self._controller.set_inhibit(False)

    def popup_menu_cb(self, widget, button, time, data=None):
        """Callback when the popup menu on the status icon has to open."""
        # Synchronize check item states with daemon
        raw = self.send_ipc('status -j')
        if raw:
            try:
                st = json.loads(raw)
                self.darkroom_item.handler_block_by_func(self.darkroom_toggle_cb)
                self.darkroom_item.set_active(st.get('darkroom') == 'true')
                self.darkroom_item.handler_unblock_by_func(self.darkroom_toggle_cb)

                self.movie_item.handler_block_by_func(self.movie_toggle_cb)
                self.movie_item.set_active(st.get('movie_mode') == 'true')
                self.movie_item.handler_unblock_by_func(self.movie_toggle_cb)

                self.myopia_item.handler_block_by_func(self.myopia_toggle_cb)
                self.myopia_item.set_active(st.get('myopia_protect') == 'true')
                self.myopia_item.handler_unblock_by_func(self.myopia_toggle_cb)

                self.couple_item.handler_block_by_func(self.couple_toggle_cb)
                self.couple_item.set_active(st.get('couple_brightness') == 'true')
                self.couple_item.handler_unblock_by_func(self.couple_toggle_cb)
            except Exception:
                pass

        self.status_menu.show_all()
        self.status_menu.popup(None, None, Gtk.StatusIcon.position_menu,
                               self.status_icon, button, time)

    def toggle_cb(self, widget, data=None):
        """Callback when a request to toggle redshift was made."""
        self.remove_suspend_timer()
        self._controller.set_inhibit(not self._controller.inhibited)

    def toggle_item_cb(self, widget, data=None):
        """Callback when a request to toggle redshift was made.

        This ensures that the state of redshift is synchronised with
        the toggle state of the widget (e.g. Gtk.CheckMenuItem).
        """
        active = not self._controller.inhibited
        if active != widget.get_active():
            self.remove_suspend_timer()
            self._controller.set_inhibit(not self._controller.inhibited)

    # Info dialog callbacks
    def show_info_cb(self, widget, data=None):
        """Callback when the info dialog should be presented."""
        raw = self.send_ipc('check')
        if raw and hasattr(self, 'checks_label'):
            self.checks_label.set_text(raw.strip())
        self.info_dialog.show()

    def response_info_cb(self, widget, data=None):
        """Callback when a button in the info dialog was activated."""
        self.info_dialog.hide()

    def close_info_dialog_cb(self, widget, data=None):
        """Callback when the info dialog is closed."""
        self.info_dialog.hide()
        return True

    def on_icon_theme_changed_cb(self, theme):
        self.update_status_icon()

    def update_status_icon(self):
        """Update the status icon according to the internally recorded state.

        This should be called whenever the internally recorded state
        might have changed.
        """
        prefix = 'jarheart' if self.icon_theme.has_icon('jarheart-status-on') else 'redshift'
        if self._controller.inhibited:
            icon_name = f'{prefix}-status-off-symbolic'
        else:
            icon_name = f'{prefix}-status-on-symbolic'

        if not self.icon_theme.has_icon(icon_name):
            icon_name = icon_name.replace('-symbolic', '')

        if appindicator:
            self.indicator.set_icon(icon_name)
        else:
            self.status_icon.set_from_icon_name(icon_name)

    # State update functions
    def inhibit_change_cb(self, controller, inhibit):
        """Callback when controller changes inhibition status."""
        self.change_inhibited(inhibit)

    def period_change_cb(self, controller, period):
        """Callback when controller changes period."""
        self.change_period(period)

    def temperature_change_cb(self, controller, temperature):
        """Callback when controller changes temperature."""
        self.change_temperature(temperature)

    def location_change_cb(self, controller, lat, lon):
        """Callback when controlled changes location."""
        self.change_location((lat, lon))

    def error_occured_cb(self, controller, error):
        """Callback when an error occurs in the controller."""
        error_dialog = Gtk.MessageDialog(
            None, Gtk.DialogFlags.MODAL, Gtk.MessageType.ERROR,
            Gtk.ButtonsType.CLOSE, '')
        error_dialog.set_markup(
            '<b>Failed to run Redshift</b>\n<i>' + error + '</i>')
        error_dialog.run()

        # Quit when the model dialog is closed
        sys.exit(-1)

    def controller_stopped_cb(self, controller):
        """Callback when controlled is stopped successfully."""
        Gtk.main_quit()

    # Update interface
    def change_inhibited(self, inhibited):
        """Change interface to new inhibition status."""
        self.update_status_icon()
        self.toggle_item.set_active(not inhibited)
        self.status_label.set_markup(
            _('<b>Status:</b> {}').format(
                _('Disabled') if inhibited else _('Enabled')))

    def change_temperature(self, temperature):
        """Change interface to new temperature."""
        self.temperature_label.set_markup(
            '<b>{}:</b> {}K'.format(_('Color temperature'), temperature))
        self.update_tooltip_text()

    def change_period(self, period):
        """Change interface to new period."""
        self.period_label.set_markup(
            '<b>{}:</b> {}'.format(_('Period'), period))
        self.update_tooltip_text()

    def change_location(self, location):
        """Change interface to new location."""
        self.location_label.set_markup(
            '<b>{}:</b> {}, {}'.format(_('Location'), *location))

    def update_tooltip_text(self):
        """Update text of tooltip status icon."""
        if not appindicator:
            self.status_icon.set_tooltip_text('{}: {}K, {}: {}'.format(
                _('Color temperature'), self._controller.temperature,
                _('Period'), self._controller.period))

    def autostart_cb(self, widget, data=None):
        """Callback when a request to toggle autostart is made."""
        utils.set_autostart(widget.get_active())

    def destroy_cb(self, widget, data=None):
        """Callback when a request to quit the application is made."""
        if not appindicator:
            self.status_icon.set_visible(False)
        if self.settings_dialog:
            self.settings_dialog.destroy()
        self.info_dialog.destroy()
        self._controller.terminate_child()
        return False


def run():
    prog_name = 'jarheart-gtk' if 'jarheart' in sys.argv[0] else 'redshift-gtk'
    utils.setproctitle(prog_name)

    # Internationalisation
    for domain in ('jarheart', 'redshift'):
        try:
            gettext.bindtextdomain(domain, defs.LOCALEDIR)
        except Exception:
            pass
    gettext.textdomain('jarheart')

    for help_arg in ('-h', '--help'):
        if help_arg in sys.argv:
            print(_('Please run `jarheart -h` or `redshift -h` for help output.'))
            sys.exit(-1)

    # Ignore SIGHUP so the tray app survives subshell or launcher exit
    if hasattr(signal, 'SIGHUP'):
        signal.signal(signal.SIGHUP, signal.SIG_IGN)

    # Create redshift child process controller
    c = RedshiftController(sys.argv[1:])

    def terminate_child(data=None):
        c.terminate_child()
        return False

    # Install signal handlers
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM,
                         terminate_child, None)
    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGINT,
                         terminate_child, None)

    try:
        # Create status icon
        RedshiftStatusIcon(c)

        # Run main loop
        Gtk.main()
    except:
        c.kill_child()
        raise
