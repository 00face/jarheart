/* checks.h -- Hardware light, weather, and timezone checks header
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

#ifndef JARHEART_CHECKS_H
#define JARHEART_CHECKS_H

#include <stddef.h>
#include <time.h>
#include "redshift.h"

typedef struct {
	/* Light */
	int backlight_percent; /* -1 if unavailable, 0-100 */
	char backlight_device[64];
	int ambient_lux; /* -1 if unavailable */
	char light_summary[128];

	/* Weather */
	int weather_available;
	char weather_summary[128];
	time_t weather_last_check;

	/* Timezone */
	char timezone_name[64];
	int timezone_changed;
	location_t timezone_location;
	char timezone_summary[128];
} checks_result_t;

/* Run light check (backlight & ambient sensor) */
int checks_check_light(checks_result_t *result);

/* Run weather check (gentle query with cache) */
int checks_check_weather(checks_result_t *result, int force_refresh);

/* Run timezone check */
int checks_check_timezone(checks_result_t *result);

/* Run all checks and format human-readable output into buf */
int checks_format_summary(checks_result_t *result, char *buf, size_t buf_size);

#endif /* ! JARHEART_CHECKS_H */
