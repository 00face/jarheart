/* checks.c -- Hardware light, weather, and timezone checks
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include "checks.h"
#include "location-timezone.h"

int
checks_check_light(checks_result_t *result)
{
	if (result == NULL) return -1;

	result->backlight_percent = -1;
	result->backlight_device[0] = '\0';
	result->ambient_lux = -1;

	/* 1. Check screen backlight in /sys/class/backlight */
	DIR *dir = opendir("/sys/class/backlight");
	if (dir != NULL) {
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.') continue;

			char path[512];
			snprintf(path, sizeof(path), "/sys/class/backlight/%.128s/actual_brightness", ent->d_name);
			FILE *f = fopen(path, "r");
			if (f == NULL) {
				snprintf(path, sizeof(path), "/sys/class/backlight/%.128s/brightness", ent->d_name);
				f = fopen(path, "r");
			}

			long actual = -1;
			if (f != NULL) {
				if (fscanf(f, "%ld", &actual) != 1) actual = -1;
				fclose(f);
			}

			long max_b = -1;
			snprintf(path, sizeof(path), "/sys/class/backlight/%.128s/max_brightness", ent->d_name);
			f = fopen(path, "r");
			if (f != NULL) {
				if (fscanf(f, "%ld", &max_b) != 1) max_b = -1;
				fclose(f);
			}

			if (actual >= 0 && max_b > 0) {
				result->backlight_percent = (int)((actual * 100) / max_b);
				snprintf(result->backlight_device, sizeof(result->backlight_device),
					 "%.63s", ent->d_name);
				break;
			}
		}
		closedir(dir);
	}

	/* 2. Check ambient light sensor in /sys/bus/iio/devices */
	dir = opendir("/sys/bus/iio/devices");
	if (dir != NULL) {
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (strncmp(ent->d_name, "iio:device", 10) != 0) continue;

			char path[512];
			snprintf(path, sizeof(path), "/sys/bus/iio/devices/%.128s/in_illuminance_input", ent->d_name);
			FILE *f = fopen(path, "r");
			if (f == NULL) {
				snprintf(path, sizeof(path), "/sys/bus/iio/devices/%.128s/in_illuminance_raw", ent->d_name);
				f = fopen(path, "r");
			}

			if (f != NULL) {
				long lux = -1;
				if (fscanf(f, "%ld", &lux) == 1 && lux >= 0) {
					result->ambient_lux = (int)lux;
				}
				fclose(f);
				if (result->ambient_lux >= 0) break;
			}
		}
		closedir(dir);
	}

	/* Format light summary */
	if (result->backlight_percent >= 0 && result->ambient_lux >= 0) {
		snprintf(result->light_summary, sizeof(result->light_summary),
			 "Backlight at %d%% (%s), Ambient light: %d lux",
			 result->backlight_percent, result->backlight_device, result->ambient_lux);
	} else if (result->backlight_percent >= 0) {
		snprintf(result->light_summary, sizeof(result->light_summary),
			 "Backlight at %d%% (%s), Ambient sensor: not present",
			 result->backlight_percent, result->backlight_device);
	} else if (result->ambient_lux >= 0) {
		snprintf(result->light_summary, sizeof(result->light_summary),
			 "Ambient light: %d lux, Screen backlight: not present",
			 result->ambient_lux);
	} else {
		snprintf(result->light_summary, sizeof(result->light_summary),
			 "Hardware light sensors unavailable");
	}

	return 0;
}

int
checks_check_weather(checks_result_t *result, int force_refresh)
{
	if (result == NULL) return -1;

	time_t now = time(NULL);
	/* Use cache if queried within last 30 minutes (1800s) */
	if (!force_refresh && result->weather_available && (now - result->weather_last_check < 1800)) {
		return 0;
	}

	FILE *p = popen("/usr/bin/curl -s --max-time 2 'https://wttr.in/?format=%C:+%t' 2>/dev/null", "r");
	if (p != NULL) {
		char buf[128];
		if (fgets(buf, sizeof(buf), p) != NULL) {
			/* Trim whitespace and newlines */
			char *end = buf + strlen(buf) - 1;
			while (end >= buf && isspace((unsigned char)*end)) {
				*end = '\0';
				end--;
			}
			char *start = buf;
			while (*start != '\0' && isspace((unsigned char)*start)) {
				start++;
			}

			/* Ensure not HTML error page */
			if (*start != '\0' && strchr(start, '<') == NULL && strchr(start, '{') == NULL) {
				snprintf(result->weather_summary, sizeof(result->weather_summary), "%s", start);
				result->weather_available = 1;
				result->weather_last_check = now;
				pclose(p);
				return 0;
			}
		}
		pclose(p);
	}

	if (!result->weather_available) {
		snprintf(result->weather_summary, sizeof(result->weather_summary),
			 "Weather unavailable (network or offline)");
	}
	return 0;
}

static char last_known_tz[64] = "";

int
checks_check_timezone(checks_result_t *result)
{
	if (result == NULL) return -1;

	location_t loc;
	char tz_name[64];
	tz_name[0] = '\0';

	int r = location_timezone_resolve(&loc, tz_name, sizeof(tz_name));
	if (r == 0) {
		snprintf(result->timezone_name, sizeof(result->timezone_name), "%s", tz_name);
		result->timezone_location = loc;

		if (last_known_tz[0] != '\0' && strcmp(last_known_tz, tz_name) != 0) {
			result->timezone_changed = 1;
		} else {
			result->timezone_changed = 0;
		}
		snprintf(last_known_tz, sizeof(last_known_tz), "%s", tz_name);

		snprintf(result->timezone_summary, sizeof(result->timezone_summary),
			 "%s (%.2f° %c, %.2f° %c)",
			 tz_name,
			 fabsf(loc.lat), loc.lat >= 0 ? 'N' : 'S',
			 fabsf(loc.lon), loc.lon >= 0 ? 'E' : 'W');
	} else {
		result->timezone_changed = 0;
		snprintf(result->timezone_summary, sizeof(result->timezone_summary),
			 "Timezone location unavailable");
	}

	return 0;
}

int
checks_get_battery_status(battery_info_t *info)
{
	if (info == NULL) return -1;
	memset(info, 0, sizeof(*info));
	info->battery_percent = -1;

	/* 1. Check AC status from /sys/class/power_supply */
	int ac_online = -1;
	FILE *f_ac = fopen("/sys/class/power_supply/AC/online", "r");
	if (f_ac != NULL) {
		if (fscanf(f_ac, "%d", &ac_online) != 1) ac_online = -1;
		fclose(f_ac);
	}

	/* 2. Check battery in /sys/class/power_supply */
	DIR *dir = opendir("/sys/class/power_supply");
	if (dir == NULL) return -1;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		if (strncmp(ent->d_name, "BAT", 3) != 0) {
			char type_path[512];
			snprintf(type_path, sizeof(type_path), "/sys/class/power_supply/%s/type", ent->d_name);
			FILE *ft = fopen(type_path, "r");
			int is_bat = 0;
			if (ft != NULL) {
				char type_str[64];
				if (fgets(type_str, sizeof(type_str), ft) != NULL && strstr(type_str, "Battery") != NULL) {
					is_bat = 1;
				}
				fclose(ft);
			}
			if (!is_bat) continue;
		}

		info->available = 1;

		/* Read status */
		char path[512];
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", ent->d_name);
		FILE *fs = fopen(path, "r");
		if (fs != NULL) {
			if (fgets(info->status, sizeof(info->status), fs) != NULL) {
				char *nl = strchr(info->status, '\n');
				if (nl) *nl = '\0';
			}
			fclose(fs);
		}

		/* Read capacity */
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", ent->d_name);
		FILE *fc = fopen(path, "r");
		if (fc != NULL) {
			int cap = -1;
			if (fscanf(fc, "%d", &cap) == 1 && cap >= 0 && cap <= 100) {
				info->battery_percent = cap;
			}
			fclose(fc);
		}
		break;
	}
	closedir(dir);

	if (info->available) {
		if (strcasecmp(info->status, "Discharging") == 0) {
			info->on_battery = 1;
		} else if (ac_online == 0) {
			info->on_battery = 1;
		} else if (ac_online == 1) {
			info->on_battery = 0;
		} else {
			info->on_battery = (strcasecmp(info->status, "Charging") != 0 && strcasecmp(info->status, "Full") != 0);
		}
		return 0;
	}

	return -1;
}

int
checks_format_summary(checks_result_t *result, char *buf, size_t buf_size)
{
	if (result == NULL || buf == NULL || buf_size == 0) return -1;

	checks_check_light(result);
	checks_check_weather(result, 0);
	checks_check_timezone(result);

	battery_info_t bat;
	char bat_buf[128] = "N/A (Desktop / AC)";
	if (checks_get_battery_status(&bat) == 0 && bat.available) {
		snprintf(bat_buf, sizeof(bat_buf), "%d%% (%s, %s)",
			 bat.battery_percent, bat.status,
			 bat.on_battery ? "Battery Power" : "AC Connected");
	}

	snprintf(buf, buf_size,
		 "Environment & System Checks:\n"
		 "  Light:      %s\n"
		 "  Weather:    %s\n"
		 "  Timezone:   %s\n"
		 "  Power:      %s\n",
		 result->light_summary,
		 result->weather_summary,
		 result->timezone_summary,
		 bat_buf);

	return 0;
}

int
checks_ensure_pwm_free(void)
{
	DIR *dir = opendir("/sys/class/backlight");
	if (dir == NULL) return -1;

	struct dirent *ent;
	int count = 0;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.') continue;

		char max_path[512], b_path[512];
		snprintf(max_path, sizeof(max_path), "/sys/class/backlight/%.128s/max_brightness", ent->d_name);
		snprintf(b_path, sizeof(b_path), "/sys/class/backlight/%.128s/brightness", ent->d_name);

		FILE *f_max = fopen(max_path, "r");
		long max_b = -1;
		if (f_max != NULL) {
			if (fscanf(f_max, "%ld", &max_b) != 1) max_b = -1;
			fclose(f_max);
		}

		if (max_b > 0) {
			FILE *f_b = fopen(b_path, "r+");
			if (f_b != NULL) {
				long cur_b = -1;
				if (fscanf(f_b, "%ld", &cur_b) == 1 && cur_b != max_b) {
					rewind(f_b);
					fprintf(f_b, "%ld\n", max_b);
				}
				fclose(f_b);
				count++;
			}
		}
	}
	closedir(dir);
	return count;
}

uint64_t
checks_get_input_interrupts(void)
{
	FILE *f = fopen("/proc/interrupts", "r");
	if (f == NULL) return 0;

	uint64_t total = 0;
	char line[512];
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "i8042") != NULL ||
		    strstr(line, "xhci_hcd") != NULL ||
		    strstr(line, "ehci_hcd") != NULL) {
			char *p = strchr(line, ':');
			if (p == NULL) continue;
			p++;
			while (*p != '\0' && !isalpha((unsigned char)*p)) {
				while (isspace((unsigned char)*p)) p++;
				if (isdigit((unsigned char)*p)) {
					uint64_t cnt = strtoull(p, &p, 10);
					total += cnt;
				} else {
					break;
				}
			}
		}
	}
	fclose(f);
	return total;
}
