/* location-timezone.c -- System timezone location provider source
   This file is part of Jarheart.

   Jarheart is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Copyright (c) 2026 Jarheart Contributors
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "location-timezone.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) s
#endif

typedef struct {
	location_t loc;
	char tz_name[256];
} location_timezone_state_t;

/* Parse ISO 6709 coordinates format: ±DDMM[SS]±DDDMM[SS] */
static int
parse_iso6709(const char *str, float *lat, float *lon)
{
	if (!str || (*str != '+' && *str != '-')) return -1;
	char lat_sign = *str++;
	if (strlen(str) < 4) return -1;
	int lat_deg = (str[0] - '0') * 10 + (str[1] - '0');
	int lat_min = (str[2] - '0') * 10 + (str[3] - '0');
	str += 4;
	int lat_sec = 0;
	if (*str != '+' && *str != '-') {
		lat_sec = (str[0] - '0') * 10 + (str[1] - '0');
		str += 2;
	}
	if (*str != '+' && *str != '-') return -1;
	char lon_sign = *str++;
	if (strlen(str) < 5) return -1;
	int lon_deg = (str[0] - '0') * 100 + (str[1] - '0') * 10 + (str[2] - '0');
	int lon_min = (str[3] - '0') * 10 + (str[4] - '0');
	str += 5;
	int lon_sec = 0;
	if (*str >= '0' && *str <= '9' && *(str + 1) >= '0' && *(str + 1) <= '9') {
		lon_sec = (str[0] - '0') * 10 + (str[1] - '0');
	}
	*lat = (float)(lat_deg + lat_min / 60.0 + lat_sec / 3600.0) * (lat_sign == '-' ? -1.0f : 1.0f);
	*lon = (float)(lon_deg + lon_min / 60.0 + lon_sec / 3600.0) * (lon_sign == '-' ? -1.0f : 1.0f);
	return 0;
}

int
location_timezone_resolve(location_t *loc, char *tz_name_out, size_t tz_name_len)
{
	char tz_name[256] = {0};
	char link_target[PATH_MAX];
	ssize_t len = readlink("/etc/localtime", link_target, sizeof(link_target) - 1);
	if (len > 0) {
		link_target[len] = '\0';
		const char *p = strstr(link_target, "zoneinfo/");
		if (p) {
			strncpy(tz_name, p + strlen("zoneinfo/"), sizeof(tz_name) - 1);
		}
	}
	if (tz_name[0] == '\0') {
		FILE *f = fopen("/etc/timezone", "r");
		if (f) {
			if (fgets(tz_name, sizeof(tz_name), f)) {
				size_t l = strlen(tz_name);
				while (l > 0 && (tz_name[l-1] == '\n' || tz_name[l-1] == '\r')) {
					tz_name[--l] = '\0';
				}
			}
			fclose(f);
		}
	}
	if (tz_name[0] == '\0') {
		const char *env_tz = getenv("TZ");
		if (env_tz && *env_tz) {
			strncpy(tz_name, env_tz, sizeof(tz_name) - 1);
		}
	}
	if (tz_name[0] == '\0') {
		return -1;
	}

	if (tz_name_out && tz_name_len > 0) {
		snprintf(tz_name_out, tz_name_len, "%s", tz_name);
	}

	const char *tab_paths[] = {
		"/usr/share/zoneinfo/zone1970.tab",
		"/usr/share/zoneinfo/zone.tab",
		"/usr/lib/zoneinfo/zone1970.tab",
		"/usr/lib/zoneinfo/zone.tab",
		NULL
	};

	for (int i = 0; tab_paths[i]; i++) {
		FILE *f = fopen(tab_paths[i], "r");
		if (!f) continue;
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			if (line[0] == '#' || line[0] == '\n') continue;
			char *tab1 = strchr(line, '\t');
			if (!tab1) continue;
			char *coord = tab1 + 1;
			char *tab2 = strchr(coord, '\t');
			if (!tab2) continue;
			*tab2 = '\0';
			char *tz = tab2 + 1;
			char *tab3 = strchr(tz, '\t');
			if (tab3) *tab3 = '\0';
			else {
				char *nl = strchr(tz, '\n');
				if (nl) *nl = '\0';
			}
			if (strcmp(tz, tz_name) == 0) {
				float lat, lon;
				int res = parse_iso6709(coord, &lat, &lon);
				fclose(f);
				if (res == 0) {
					loc->lat = lat;
					loc->lon = lon;
					return 0;
				}
				return -1;
			}
		}
		fclose(f);
	}

	return -1;
}

static int
location_timezone_init(location_timezone_state_t **state)
{
	*state = malloc(sizeof(location_timezone_state_t));
	if (*state == NULL) return -1;

	location_timezone_state_t *s = *state;
	s->loc.lat = NAN;
	s->loc.lon = NAN;
	s->tz_name[0] = '\0';

	return 0;
}

static int
location_timezone_start(location_timezone_state_t *state)
{
	if (location_timezone_resolve(&state->loc, state->tz_name, sizeof(state->tz_name)) < 0) {
		fputs(_("Could not resolve location from system timezone.\n"), stderr);
		return -1;
	}

	printf(_("Resolved location from system timezone (%s): %.2f N, %.2f W\n"),
	       state->tz_name, state->loc.lat, -state->loc.lon);

	return 0;
}

static void
location_timezone_free(location_timezone_state_t *state)
{
	free(state);
}

static void
location_timezone_print_help(FILE *f)
{
	fputs(_("Resolve location automatically from system timezone (/etc/localtime).\n"), f);
	fputs("\n", f);
}

static int
location_timezone_set_option(location_timezone_state_t *state, const char *key,
			     const char *value)
{
	(void)state;
	(void)value;
	fprintf(stderr, _("Unknown method parameter: `%s'.\n"), key);
	return -1;
}

static int
location_timezone_get_fd(location_timezone_state_t *state)
{
	(void)state;
	return -1;
}

static int
location_timezone_handle(
	location_timezone_state_t *state, location_t *location, int *available)
{
	*location = state->loc;
	*available = 1;
	return 0;
}

const location_provider_t timezone_location_provider = {
	"timezone",
	(location_provider_init_func *)location_timezone_init,
	(location_provider_start_func *)location_timezone_start,
	(location_provider_free_func *)location_timezone_free,
	(location_provider_print_help_func *)location_timezone_print_help,
	(location_provider_set_option_func *)location_timezone_set_option,
	(location_provider_get_fd_func *)location_timezone_get_fd,
	(location_provider_handle_func *)location_timezone_handle
};
