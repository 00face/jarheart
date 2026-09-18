/* ipc.c -- Inter-process communication source for Jarheart
   This file is part of Jarheart.

   Jarheart is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Jarheart is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Jarheart.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2026  Jarheart Contributors
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ipc.h"
#include "colorramp.h"
#include "checks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#ifndef _WIN32
# include <sys/socket.h>
# include <sys/un.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

/* Period names in English */
static const char *period_names_ipc[] = {
	"None",
	"Daytime",
	"Night",
	"Transition"
};

/* Parse human duration string into seconds */
int
ipc_parse_duration(const char *str)
{
	if (str == NULL) return -1;

	while (isspace((unsigned char)*str)) str++;
	if (*str == '\0') return -1;

	char *end = NULL;
	errno = 0;
	long val = strtol(str, &end, 10);
	if (errno != 0 || end == str || val < 0) {
		return -1;
	}

	while (isspace((unsigned char)*end)) end++;

	char unit[16];
	size_t ulen = 0;
	while (*end != '\0' && !isspace((unsigned char)*end) && ulen < sizeof(unit) - 1) {
		unit[ulen++] = *end++;
	}
	unit[ulen] = '\0';

	while (isspace((unsigned char)*end)) end++;
	if (*end != '\0') {
		return -1;
	}

	if (unit[0] == '\0' || strcasecmp(unit, "s") == 0 || strcasecmp(unit, "sec") == 0 || strcasecmp(unit, "secs") == 0) {
		return (int)val;
	} else if (strcasecmp(unit, "m") == 0 || strcasecmp(unit, "min") == 0 || strcasecmp(unit, "mins") == 0) {
		return (int)(val * 60);
	} else if (strcasecmp(unit, "h") == 0 || strcasecmp(unit, "hr") == 0 || strcasecmp(unit, "hrs") == 0 || strcasecmp(unit, "hour") == 0 || strcasecmp(unit, "hours") == 0) {
		return (int)(val * 3600);
	} else if (strcasecmp(unit, "d") == 0 || strcasecmp(unit, "day") == 0 || strcasecmp(unit, "days") == 0) {
		return (int)(val * 86400);
	}

	return -1;
}

static int
parse_simple_time_range(const char *str, time_range_t *range)
{
	int h1 = 0, m1 = 0, h2 = 0, m2 = 0;
	if (sscanf(str, "%d:%d-%d:%d", &h1, &m1, &h2, &m2) == 4) {
		range->start = h1 * 3600 + m1 * 60;
		range->end = h2 * 3600 + m2 * 60;
		return 0;
	} else if (sscanf(str, "%d:%d", &h1, &m1) == 2) {
		range->start = h1 * 3600 + m1 * 60;
		range->end = range->start;
		return 0;
	}
	return -1;
}

int
ipc_dispatch_command(
	const char *cmd_line, daemon_ipc_state_t *state,
	char *response_buf, size_t response_buf_size)
{
	if (cmd_line == NULL || state == NULL || response_buf == NULL || response_buf_size == 0) {
		return -1;
	}

	/* Copy and trim command line */
	char buf[256];
	strncpy(buf, cmd_line, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	char *cmd = buf;
	while (isspace((unsigned char)*cmd)) cmd++;
	char *end = cmd + strlen(cmd);
	while (end > cmd && isspace((unsigned char)*(end - 1))) {
		*(--end) = '\0';
	}

	if (*cmd == '\0') {
		snprintf(response_buf, response_buf_size, "Error: Empty command\n");
		return 0;
	}

	/* Split command and arguments */
	char *arg = cmd;
	while (*arg != '\0' && !isspace((unsigned char)*arg)) arg++;
	if (*arg != '\0') {
		*arg = '\0';
		arg++;
		while (isspace((unsigned char)*arg)) arg++;
	}

	time_t now = time(NULL);
	int is_paused = (state->pause_until > now);

	if (strcasecmp(cmd, "status") == 0) {
		const char *status_str = "Enabled";
		if (is_paused) {
			status_str = "Paused";
		} else if (state->disabled) {
			status_str = "Disabled";
		} else if (state->darkroom) {
			status_str = "Darkroom";
		} else if (state->movie_mode) {
			status_str = "Movie Mode";
		} else if (state->myopia_protect) {
			status_str = "Myopia Protect";
		} else if (state->current_preset[0] != '\0') {
			status_str = state->current_preset;
		} else if (state->override_temp > 0) {
			status_str = "Override";
		}

		const char *period_str = "Unknown";
		if ((int)state->period >= 0 && (int)state->period < 4) {
			period_str = period_names_ipc[state->period];
		}

		char loc_str[64];
		if (!isnan(state->location.lat) && !isnan(state->location.lon)) {
			snprintf(loc_str, sizeof(loc_str), "%.2f %c, %.2f %c",
				 fabs(state->location.lat),
				 state->location.lat >= 0 ? 'N' : 'S',
				 fabs(state->location.lon),
				 state->location.lon >= 0 ? 'E' : 'W');
		} else {
			snprintf(loc_str, sizeof(loc_str), "None");
		}

		long remaining = is_paused ? (long)(state->pause_until - now) : 0;
		long movie_remaining = (state->movie_mode && state->movie_mode_until > now) ?
			(long)(state->movie_mode_until - now) : 0;

		if (arg != NULL && (strcasecmp(arg, "--json") == 0 || strcasecmp(arg, "-j") == 0 || strcasecmp(arg, "json") == 0)) {
			snprintf(response_buf, response_buf_size,
				 "{\n"
				 "  \"status\": \"%s\",\n"
				 "  \"period\": \"%s\",\n"
				 "  \"temperature\": %u,\n"
				 "  \"brightness\": %.2f,\n"
				 "  \"gamma\": [%.3f, %.3f, %.3f],\n"
				 "  \"latitude\": %.4f,\n"
				 "  \"longitude\": %.4f,\n"
				 "  \"method\": \"%s\",\n"
				 "  \"paused\": %s,\n"
				 "  \"pause_remaining\": %ld,\n"
				 "  \"darkroom\": %s,\n"
				 "  \"movie_mode\": %s,\n"
				 "  \"movie_remaining\": %ld,\n"
				 "  \"myopia_protect\": %s,\n"
				 "  \"couple_brightness\": %s,\n"
				 "  \"ambient_balancer\": %s,\n"
				 "  \"pacer_interval\": %d,\n"
				 "  \"preset\": \"%s\",\n"
				 "  \"schedule\": \"%s\",\n"
				 "  \"override_temp\": %d,\n"
				 "  \"text\": \"%uK\",\n"
				 "  \"alt\": \"%s\",\n"
				 "  \"tooltip\": \"Jarheart: %s\\nPeriod: %s\\nTemperature: %uK\\nBrightness: %.2f\",\n"
				 "  \"class\": \"%s\"\n"
				 "}\n",
				 status_str,
				 period_str,
				 state->current_setting.temperature,
				 state->current_setting.brightness,
				 state->current_setting.gamma[0],
				 state->current_setting.gamma[1],
				 state->current_setting.gamma[2],
				 isnan(state->location.lat) ? 0.0 : state->location.lat,
				 isnan(state->location.lon) ? 0.0 : state->location.lon,
				 state->method_name ? state->method_name : "none",
				 is_paused ? "true" : "false",
				 remaining,
				 state->darkroom ? "true" : "false",
				 state->movie_mode ? "true" : "false",
				 movie_remaining,
				 state->myopia_protect ? "true" : "false",
				 state->couple_brightness ? "true" : "false",
				 state->ambient_balancer ? "true" : "false",
				 state->pacer_interval,
				 state->current_preset[0] ? state->current_preset : "none",
				 state->schedule_use_time == 2 ? "diurnal" : (state->schedule_use_time ? "time" : "solar"),
				 state->override_temp,
				 state->current_setting.temperature,
				 period_str,
				 status_str, period_str, state->current_setting.temperature, state->current_setting.brightness,
				 state->disabled ? "disabled" : (is_paused ? "paused" : "enabled"));
			return 0;
		}

		if (arg != NULL && strcasecmp(arg, "--xfce") == 0) {
			snprintf(response_buf, response_buf_size,
				 "<txt>%uK</txt><tool>Jarheart: %s (%s, %uK)</tool>\n",
				 state->current_setting.temperature,
				 status_str, period_str, state->current_setting.temperature);
			return 0;
		}

		char movie_str[64];
		if (state->movie_mode) {
			snprintf(movie_str, sizeof(movie_str), "Active (%lds remaining)", movie_remaining);
		} else {
			snprintf(movie_str, sizeof(movie_str), "Inactive");
		}

		char pacer_str[64];
		if (state->pacer_interval > 0) {
			snprintf(pacer_str, sizeof(pacer_str), "Active (%dm)", state->pacer_interval / 60);
		} else {
			snprintf(pacer_str, sizeof(pacer_str), "Inactive");
		}

		snprintf(response_buf, response_buf_size,
			 "Status: %s\n"
			 "Period: %s\n"
			 "Transition: %.2f\n"
			 "Color temperature: %uK\n"
			 "Brightness: %.2f\n"
			 "Gamma: %.3f, %.3f, %.3f\n"
			 "Location: %s\n"
			 "Method: %s\n"
			 "Pause remaining: %lds\n"
			 "Darkroom: %s\n"
			 "Movie mode: %s\n"
			 "Myopia protect: %s\n"
			 "Coupled brightness: %s\n"
			 "Ambient balancer: %s\n"
			 "Pacer: %s\n"
			 "Preset: %s\n"
			 "Schedule: %s\n"
			 "Override temp: %d\n",
			 status_str,
			 period_str,
			 state->transition_prog,
			 state->current_setting.temperature,
			 state->current_setting.brightness,
			 state->current_setting.gamma[0],
			 state->current_setting.gamma[1],
			 state->current_setting.gamma[2],
			 loc_str,
			 state->method_name ? state->method_name : "none",
			 remaining,
			 state->darkroom ? "Active (monochrome red)" : "Inactive",
			 movie_str,
			 state->myopia_protect ? "Active (2850K, 60% lum)" : "Inactive",
			 state->couple_brightness ? "Enabled" : "Disabled",
			 state->ambient_balancer ? "Enabled (dynamic lux matching)" : "Disabled",
			 pacer_str,
			 state->current_preset[0] ? state->current_preset : "None",
			 state->schedule_use_time == 2 ? "Diurnal tri-phasic (alertness -> comfort -> wind-down)" : (state->schedule_use_time ? "Time schedule" : "Solar elevation"),
			 state->override_temp);
		return 0;
	} else if (strcasecmp(cmd, "toggle") == 0) {
		if (is_paused) {
			state->pause_until = 0;
			state->disabled = 0;
		} else {
			state->disabled = !state->disabled;
		}
		state->override_temp = 0;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size, "Status: %s\n",
			 state->disabled ? "Disabled" : "Enabled");
		return 0;
	} else if (strcasecmp(cmd, "pause") == 0 || strcasecmp(cmd, "suspend") == 0) {
		int duration = 1800; /* Default 30 minutes */
		if (*arg != '\0') {
			int parsed = ipc_parse_duration(arg);
			if (parsed <= 0) {
				snprintf(response_buf, response_buf_size,
					 "Error: Invalid pause duration '%s'. Examples: 30m, 1h, 1800\n", arg);
				return 0;
			}
			duration = parsed;
		}
		state->pause_until = now + duration;
		state->disabled = 1;
		state->override_temp = 0;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Status: Paused (Color-critical pause)\n"
			 "Remaining: %ds\n", duration);
		return 0;
	} else if (strcasecmp(cmd, "resume") == 0 || strcasecmp(cmd, "unpause") == 0 ||
		   strcasecmp(cmd, "enable") == 0 || strcasecmp(cmd, "on") == 0) {
		state->pause_until = 0;
		state->disabled = 0;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size, "Status: Enabled\n");
		return 0;
	} else if (strcasecmp(cmd, "disable") == 0 || strcasecmp(cmd, "off") == 0) {
		state->pause_until = 0;
		state->disabled = 1;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size, "Status: Disabled\n");
		return 0;
	} else if (strcasecmp(cmd, "darkroom") == 0) {
		if (strcasecmp(arg, "off") == 0) {
			state->darkroom = 0;
		} else if (strcasecmp(arg, "on") == 0) {
			state->darkroom = 1;
			state->movie_mode = 0;
			state->disabled = 0;
			state->pause_until = 0;
			state->override_temp = 0;
			state->current_preset[0] = '\0';
		} else {
			state->darkroom = !state->darkroom;
			if (state->darkroom) {
				state->movie_mode = 0;
				state->disabled = 0;
				state->pause_until = 0;
				state->override_temp = 0;
				state->current_preset[0] = '\0';
			}
		}
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Darkroom mode: %s (monochrome deep red, zero blue/green)\n",
			 state->darkroom ? "Enabled" : "Disabled");
		return 0;
	} else if (strcasecmp(cmd, "movie") == 0) {
		if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "stop") == 0) {
			state->movie_mode = 0;
			state->movie_mode_until = 0;
			state->override_temp = 0;
			state->current_preset[0] = '\0';
		} else {
			int duration = 9000; /* Default: 2.5 hours = 150 minutes */
			if (*arg != '\0' && strcasecmp(arg, "on") != 0) {
				int parsed = ipc_parse_duration(arg);
				if (parsed > 0) duration = parsed;
			}
			state->movie_mode = 1;
			state->movie_mode_until = now + duration;
			state->darkroom = 0;
			state->disabled = 0;
			state->pause_until = 0;
			state->override_temp = 4200;
			snprintf(state->current_preset, sizeof(state->current_preset), "Movie");
		}
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Movie mode: %s%s%lds%s\n",
			 state->movie_mode ? "Enabled" : "Disabled",
			 state->movie_mode ? " (" : "",
			 state->movie_mode ? (long)(state->movie_mode_until - now) : 0,
			 state->movie_mode ? " remaining, 4200K cinema tone preserving sky and shadow detail)" : "");
		return 0;
	} else if (strcasecmp(cmd, "myopia-protect") == 0 || strcasecmp(cmd, "myopia") == 0) {
		if (strcasecmp(arg, "off") == 0) {
			state->myopia_protect = 0;
			state->override_temp = 0;
			state->current_preset[0] = '\0';
		} else if (strcasecmp(arg, "on") == 0) {
			state->myopia_protect = 1;
			state->darkroom = 0;
			state->movie_mode = 0;
			state->disabled = 0;
			state->pause_until = 0;
			state->override_temp = 2850;
			snprintf(state->current_preset, sizeof(state->current_preset), "Myopia Protect");
		} else {
			state->myopia_protect = !state->myopia_protect;
			if (state->myopia_protect) {
				state->darkroom = 0;
				state->movie_mode = 0;
				state->disabled = 0;
				state->pause_until = 0;
				state->override_temp = 2850;
				snprintf(state->current_preset, sizeof(state->current_preset), "Myopia Protect");
			} else {
				state->override_temp = 0;
				state->current_preset[0] = '\0';
			}
		}
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Myopia protection mode: %s (2850K long-wavelength spectrum, 60%% luminance limit)\n",
			 state->myopia_protect ? "Enabled" : "Disabled");
		return 0;
	} else if (strcasecmp(cmd, "couple-brightness") == 0 || strcasecmp(cmd, "couple") == 0) {
		if (strcasecmp(arg, "off") == 0) {
			state->couple_brightness = 0;
		} else if (strcasecmp(arg, "on") == 0) {
			state->couple_brightness = 1;
		} else {
			state->couple_brightness = !state->couple_brightness;
		}
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Coupled brightness: %s (Kruithof ergonomics: scales luminance with CCT)\n",
			 state->couple_brightness ? "Enabled" : "Disabled");
		return 0;
	} else if (strcasecmp(cmd, "pacer") == 0) {
		if (strcasecmp(arg, "off") == 0) {
			state->pacer_interval = 0;
		} else if (strcasecmp(arg, "breathe") == 0) {
			state->pacer_breathe = 1;
			if (state->pacer_interval <= 0) state->pacer_interval = 1200;
		} else if (strcasecmp(arg, "notify") == 0) {
			state->pacer_breathe = 0;
			if (state->pacer_interval <= 0) state->pacer_interval = 1200;
		} else if (*arg != '\0') {
			int dur = ipc_parse_duration(arg);
			if (dur > 0) {
				state->pacer_interval = dur;
				state->last_pacer_time = now;
			} else {
				snprintf(response_buf, response_buf_size,
					 "Error: Invalid pacer duration '%s'. Examples: 20m, 30m, breathe, notify, off\n", arg);
				return 0;
			}
		} else {
			if (state->pacer_interval > 0) {
				state->pacer_interval = 0;
			} else {
				state->pacer_interval = 1200;
				state->last_pacer_time = now;
			}
		}
		state->state_changed = 1;
		if (state->pacer_interval > 0) {
			snprintf(response_buf, response_buf_size,
				 "20-20-20 Ocular Pacer: Enabled (every %dm, mode: breathe/notify)\n",
				 state->pacer_interval / 60);
		} else {
			snprintf(response_buf, response_buf_size,
				 "20-20-20 Ocular Pacer: Disabled\n");
		}
		return 0;
	} else if (strcasecmp(cmd, "preset") == 0) {
		if (*arg == '\0') {
			return ipc_dispatch_command("presets", state, response_buf, response_buf_size);
		}
		const kelvin_preset_t *preset = colorramp_find_preset(arg);
		if (preset == NULL) {
			snprintf(response_buf, response_buf_size,
				 "Error: Unknown preset '%s'. Type 'jarheart presets' to view available presets.\n", arg);
			return 0;
		}
		state->override_temp = preset->temperature;
		snprintf(state->current_preset, sizeof(state->current_preset), "%s", preset->display_name);
		state->darkroom = 0;
		state->movie_mode = 0;
		state->disabled = 0;
		state->pause_until = 0;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Preset: %s (%dK)\n", preset->display_name, preset->temperature);
		return 0;
	} else if (strcasecmp(cmd, "presets") == 0) {
		const kelvin_preset_t *list = colorramp_get_presets();
		size_t pos = snprintf(response_buf, response_buf_size, "Available Kelvin Presets:\n");
		for (int i = 0; list[i].name != NULL && pos + 80 < response_buf_size; i++) {
			pos += snprintf(response_buf + pos, response_buf_size - pos,
					"  %-18s %5dK  (%s)\n",
					list[i].name, list[i].temperature, list[i].display_name);
		}
		return 0;
	} else if (strcasecmp(cmd, "check") == 0) {
		checks_result_t chk;
		checks_format_summary(&chk, response_buf, response_buf_size);
		return 0;
	} else if (strcasecmp(cmd, "weather") == 0) {
		checks_result_t chk;
		memset(&chk, 0, sizeof(chk));
		checks_check_weather(&chk, 1);
		snprintf(response_buf, response_buf_size, "Weather: %s\n", chk.weather_summary);
		return 0;
	} else if (strcasecmp(cmd, "ambient") == 0 || strcasecmp(cmd, "contrast") == 0) {
		if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "1") == 0 || strcasecmp(arg, "true") == 0) {
			state->ambient_balancer = 1;
		} else if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "0") == 0 || strcasecmp(arg, "false") == 0) {
			state->ambient_balancer = 0;
		} else if (strcasecmp(arg, "toggle") == 0 || *arg == '\0') {
			state->ambient_balancer = !state->ambient_balancer;
		} else {
			snprintf(response_buf, response_buf_size,
				 "Error: Unknown argument '%s'. Usage: ambient [on|off|toggle]\n", arg);
			return 0;
		}
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Ambient contrast balancer: %s (dynamically balances screen luminance against room lux)\n",
			 state->ambient_balancer ? "Enabled" : "Disabled");
		return 0;
	} else if (strcasecmp(cmd, "schedule") == 0) {
		if (strcasecmp(arg, "solar") == 0 || strcasecmp(arg, "auto") == 0) {
			state->schedule_use_time = 0;
			state->state_changed = 1;
			snprintf(response_buf, response_buf_size, "Schedule: Solar elevation (automatic)\n");
			return 0;
		} else if (strcasecmp(arg, "diurnal") == 0 || strcasecmp(arg, "triphasic") == 0) {
			state->schedule_use_time = 2;
			state->state_changed = 1;
			snprintf(response_buf, response_buf_size,
				 "Schedule: Diurnal Tri-Phasic (Morning Focus -> Afternoon Comfort -> Evening Wind-Down)\n");
			return 0;
		} else if (*arg != '\0') {
			char dawn_str[64], dusk_str[64];
			if (sscanf(arg, "%63s %63s", dawn_str, dusk_str) == 2) {
				time_range_t dawn_r, dusk_r;
				if (parse_simple_time_range(dawn_str, &dawn_r) == 0 &&
				    parse_simple_time_range(dusk_str, &dusk_r) == 0) {
					state->dawn = dawn_r;
					state->dusk = dusk_r;
					state->schedule_use_time = 1;
					state->state_changed = 1;
					snprintf(response_buf, response_buf_size,
						 "Schedule: Time-based (Dawn: %s, Dusk: %s)\n",
						 dawn_str, dusk_str);
					return 0;
				}
			}
			snprintf(response_buf, response_buf_size,
				 "Error: Invalid schedule parameters. Usage: schedule <dawn> <dusk> (e.g. schedule 06:30-07:30 19:30-20:45), 'schedule diurnal', or 'schedule solar'\n");
			return 0;
		} else {
			if (state->schedule_use_time == 2) {
				snprintf(response_buf, response_buf_size,
					 "Schedule: Diurnal Tri-Phasic (Morning: 6500K @ 1.0, Afternoon: 4000K @ 0.80, Evening: 2300K @ 0.55, Night: 1900K @ 0.40)\n");
			} else if (state->schedule_use_time == 1) {
				snprintf(response_buf, response_buf_size,
					 "Schedule: Time-based (Dawn: %02d:%02d-%02d:%02d, Dusk: %02d:%02d-%02d:%02d)\n",
					 state->dawn.start / 3600, (state->dawn.start % 3600) / 60,
					 state->dawn.end / 3600, (state->dawn.end % 3600) / 60,
					 state->dusk.start / 3600, (state->dusk.start % 3600) / 60,
					 state->dusk.end / 3600, (state->dusk.end % 3600) / 60);
			} else {
				snprintf(response_buf, response_buf_size,
					 "Schedule: Solar elevation (dynamic sunrise/sunset)\n");
			}
			return 0;
		}
	} else if (strcasecmp(cmd, "set") == 0) {
		if (*arg == '\0') {
			snprintf(response_buf, response_buf_size,
				 "Error: Missing temperature or preset value. Usage: set <TEMP|PRESET>\n");
			return 0;
		}
		const kelvin_preset_t *preset = colorramp_find_preset(arg);
		if (preset != NULL) {
			state->override_temp = preset->temperature;
			snprintf(state->current_preset, sizeof(state->current_preset), "%s", preset->display_name);
			state->darkroom = 0;
			state->movie_mode = 0;
			state->disabled = 0;
			state->pause_until = 0;
			state->state_changed = 1;
			snprintf(response_buf, response_buf_size,
				 "Preset: %s (%dK)\n", preset->display_name, preset->temperature);
			return 0;
		}
		int temp = atoi(arg);
		if (temp < 1000 || temp > 25000) {
			snprintf(response_buf, response_buf_size,
				 "Error: Temperature must be between 1000K and 25000K (got %dK) or a preset name\n", temp);
			return 0;
		}
		state->pause_until = 0;
		state->override_temp = temp;
		state->disabled = 0;
		state->darkroom = 0;
		state->movie_mode = 0;
		state->current_preset[0] = '\0';
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size,
			 "Status: Override\n"
			 "Color temperature: %uK\n", (unsigned int)temp);
		return 0;
	} else if (strcasecmp(cmd, "reset") == 0) {
		state->override_temp = 0;
		state->pause_until = 0;
		state->darkroom = 0;
		state->movie_mode = 0;
		state->movie_mode_until = 0;
		state->current_preset[0] = '\0';
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size, "Status: Normal\n");
		return 0;
	} else if (strcasecmp(cmd, "quit") == 0 || strcasecmp(cmd, "exit") == 0 || strcasecmp(cmd, "stop") == 0) {
		state->requested_exit = 1;
		state->disabled = 1;
		state->state_changed = 1;
		snprintf(response_buf, response_buf_size, "Jarheart daemon stopping...\n");
		return 0;
	} else if (strcasecmp(cmd, "help") == 0) {
		snprintf(response_buf, response_buf_size,
			 "Available commands:\n"
			 "  status           Show current daemon status (or status -j for JSON)\n"
			 "  toggle           Toggle enabled / disabled state\n"
			 "  pause [TIME]     Color-critical pause (e.g. 30m, 1h, 1800)\n"
			 "  resume           Resume adjustments\n"
			 "  darkroom [on|off]Deep monochrome red for stargazing & darkrooms\n"
			 "  movie [TIME|off] Movie Mode (2.5h duration, warm tone, preserves sky & shadows)\n"
			 "  myopia-protect   Myopia protection mode (2850K, 60%% luminance limit)\n"
			 "  couple-brightness Couple screen brightness with Kelvin temperature (Kruithof)\n"
			 "  ambient [on|off] Ambient contrast balancer (scales brightness to room lux)\n"
			 "  pacer [20m|30m]  20-20-20 ocular rest pacer (breathe & desktop notification)\n"
			 "  preset NAME      Set a Kelvin preset (candle, halogen, sunlight...)\n"
			 "  presets          List all Kelvin presets and color temperatures\n"
			 "  schedule [SCHED] Set schedule (solar, diurnal, or dawn dusk times)\n"
			 "  check            Run gentle light, weather, and timezone checks\n"
			 "  weather          Check current local weather\n"
			 "  set TEMP|PRESET  Temporarily override temperature\n"
			 "  reset            Clear manual overrides, presets, and pause\n"
			 "  quit             Terminate the running daemon\n");
		return 0;
	} else {
		snprintf(response_buf, response_buf_size,
			 "Error: Unknown command '%s'. Run 'jarheart help' for available commands.\n", cmd);
		return 0;
	}
}

#ifndef _WIN32

static int
resolve_socket_paths(char *sock_path, size_t sock_len, char *symlink_path, size_t sym_len)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir != NULL && runtime_dir[0] != '\0') {
		snprintf(sock_path, sock_len, "%s/jarheart.sock", runtime_dir);
		if (symlink_path && sym_len > 0) {
			snprintf(symlink_path, sym_len, "%s/redshift.sock", runtime_dir);
		}
		return 0;
	}

	const char *tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL || tmpdir[0] == '\0') {
		tmpdir = "/tmp";
	}
	snprintf(sock_path, sock_len, "%s/jarheart-%u.sock", tmpdir, (unsigned int)getuid());
	if (symlink_path && sym_len > 0) {
		snprintf(symlink_path, sym_len, "%s/redshift-%u.sock", tmpdir, (unsigned int)getuid());
	}
	return 0;
}

int
ipc_init(ipc_t *ipc)
{
	if (ipc == NULL) return -1;
	memset(ipc, 0, sizeof(*ipc));
	ipc->listen_fd = -1;

	resolve_socket_paths(ipc->socket_path, sizeof(ipc->socket_path),
			     ipc->symlink_path, sizeof(ipc->symlink_path));

	/* Test whether another daemon is actively listening */
	int test_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (test_fd >= 0) {
		struct sockaddr_un test_addr;
		memset(&test_addr, 0, sizeof(test_addr));
		test_addr.sun_family = AF_UNIX;
		strncpy(test_addr.sun_path, ipc->socket_path, sizeof(test_addr.sun_path) - 1);

		if (connect(test_fd, (struct sockaddr *)&test_addr, sizeof(test_addr)) == 0) {
			/* Active socket exists; another instance is running */
			close(test_fd);
			return -EEXIST;
		}
		close(test_fd);
	}

	/* Remove stale socket file if any */
	unlink(ipc->socket_path);
	if (ipc->symlink_path[0] != '\0') {
		unlink(ipc->symlink_path);
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket(AF_UNIX)");
		return -1;
	}

	/* Set nonblocking and cloexec */
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
	int fd_flags = fcntl(fd, F_GETFD, 0);
	if (fd_flags >= 0) {
		fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC);
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ipc->socket_path, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind(AF_UNIX)");
		close(fd);
		return -1;
	}

	/* Restrict socket permissions to user only (0600) */
	chmod(ipc->socket_path, S_IRUSR | S_IWUSR);

	if (listen(fd, 10) < 0) {
		perror("listen(AF_UNIX)");
		close(fd);
		unlink(ipc->socket_path);
		return -1;
	}

	/* Create compatibility symlink (e.g. redshift.sock -> jarheart.sock) */
	if (ipc->symlink_path[0] != '\0') {
		const char *target = strrchr(ipc->socket_path, '/');
		target = (target != NULL) ? target + 1 : ipc->socket_path;
		symlink(target, ipc->symlink_path);
	}

	ipc->listen_fd = fd;
	return 0;
}

int
ipc_get_fd(const ipc_t *ipc)
{
	return ipc ? ipc->listen_fd : -1;
}

int
ipc_handle_connection(ipc_t *ipc, daemon_ipc_state_t *state)
{
	if (ipc == NULL || ipc->listen_fd < 0 || state == NULL) return -1;

	struct sockaddr_un client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_fd = accept(ipc->listen_fd, (struct sockaddr *)&client_addr, &addr_len);
	if (client_fd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			return 0;
		}
		return -1;
	}

	/* Ensure accepted client_fd is blocking with a 1-second timeout */
	int flags = fcntl(client_fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);
	}
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	char req_buf[256];
	ssize_t n = read(client_fd, req_buf, sizeof(req_buf) - 1);
	if (n > 0) {
		req_buf[n] = '\0';
		char resp_buf[JARHEART_IPC_BUF_SIZE];
		ipc_dispatch_command(req_buf, state, resp_buf, sizeof(resp_buf));
		size_t resp_len = strlen(resp_buf);
		ssize_t written = write(client_fd, resp_buf, resp_len);
		(void)written;
	}

	close(client_fd);
	return 0;
}

void
ipc_cleanup(ipc_t *ipc)
{
	if (ipc == NULL) return;
	if (ipc->listen_fd >= 0) {
		close(ipc->listen_fd);
		ipc->listen_fd = -1;
	}
	if (ipc->socket_path[0] != '\0') {
		unlink(ipc->socket_path);
	}
	if (ipc->symlink_path[0] != '\0') {
		unlink(ipc->symlink_path);
	}
}

int
ipc_client_send_command(
	const char *cmd, char *response_buf, size_t response_buf_size)
{
	if (cmd == NULL || response_buf == NULL || response_buf_size == 0) {
		return -1;
	}

	char sock_path[108];
	char sym_path[108];
	resolve_socket_paths(sock_path, sizeof(sock_path), sym_path, sizeof(sym_path));

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return -1;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		/* Try fallback symlink / redshift socket path */
		if (sym_path[0] != '\0') {
			memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, sym_path, sizeof(addr.sun_path) - 1);
			if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				close(fd);
				return -1;
			}
		} else {
			close(fd);
			return -1;
		}
	}

	/* Set socket timeout */
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	/* Send command with trailing newline atomically */
	char send_buf[300];
	int sn = snprintf(send_buf, sizeof(send_buf), "%s\n", cmd);
	if (write(fd, send_buf, sn) < 0) {
		close(fd);
		return -1;
	}

	/* Read response until EOF */
	size_t total_read = 0;
	while (total_read < response_buf_size - 1) {
		ssize_t n = read(fd, response_buf + total_read, response_buf_size - 1 - total_read);
		if (n <= 0) break;
		total_read += (size_t)n;
	}
	response_buf[total_read] = '\0';

	close(fd);
	return 0;
}

int
ipc_client_dispatch(int argc, char *argv[])
{
	if (argc < 2) return -1;

	const char *subcmd = argv[1];

	if (strcmp(subcmd, "-j") == 0 || strcmp(subcmd, "--json") == 0) {
		char resp_buf[JARHEART_IPC_BUF_SIZE];
		int r = ipc_client_send_command("status --json", resp_buf, sizeof(resp_buf));
		if (r < 0) {
			char sock_path[108];
			resolve_socket_paths(sock_path, sizeof(sock_path), NULL, 0);
			fprintf(stderr, "jarheart: No running daemon found (tried %s)\n", sock_path);
			return 1;
		}
		fputs(resp_buf, stdout);
		return 0;
	}

	if (subcmd[0] == '-') return -1;

	/* Check if subcmd matches supported command names */
	const char *known_cmds[] = {
		"status", "toggle", "pause", "suspend", "resume", "unpause",
		"on", "off", "enable", "disable", "set", "reset",
		"darkroom", "movie", "preset", "presets", "schedule",
		"check", "weather",
		"myopia-protect", "myopia", "reading",
		"couple-brightness", "couple",
		"ambient", "contrast", "pacer",
		"quit", "exit", "stop", "help", NULL
	};

	int is_subcmd = 0;
	for (int i = 0; known_cmds[i] != NULL; i++) {
		if (strcasecmp(subcmd, known_cmds[i]) == 0) {
			is_subcmd = 1;
			break;
		}
	}

	if (!is_subcmd) return -1;

	/* Build full command line from argv[1..argc-1] */
	char cmd_buf[256];
	size_t offset = 0;
	cmd_buf[0] = '\0';

	for (int i = 1; i < argc; i++) {
		size_t arg_len = strlen(argv[i]);
		if (offset + arg_len + 2 >= sizeof(cmd_buf)) break;
		if (i > 1) {
			cmd_buf[offset++] = ' ';
		}
		memcpy(cmd_buf + offset, argv[i], arg_len);
		offset += arg_len;
		cmd_buf[offset] = '\0';
	}

	char resp_buf[JARHEART_IPC_BUF_SIZE];
	int r = ipc_client_send_command(cmd_buf, resp_buf, sizeof(resp_buf));
	if (r < 0) {
		char sock_path[108];
		resolve_socket_paths(sock_path, sizeof(sock_path), NULL, 0);
		fprintf(stderr, "jarheart: No running daemon found (tried %s)\n", sock_path);
		return 1;
	}

	fputs(resp_buf, stdout);
	return 0;
}

#endif /* !_WIN32 */
