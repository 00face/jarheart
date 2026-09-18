/* test_solar.c -- Unit tests for solar astronomy calculations */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "../src/solar.h"

int main(void)
{
	printf("Running test_solar...\n");

	/* Test 1: Equinox solar noon at equator (lat=0, lon=0).
	   Epoch 1710936000 is approx March 20, 2024, 12:00:00 UTC.
	   Sun should be almost directly overhead: elevation ~ 85 - 90 deg. */
	double equinox_noon = 1710936000.0;
	double elev_noon = solar_elevation(equinox_noon, 0.0, 0.0);
	printf("  Equator equinox noon elevation: %.2f deg\n", elev_noon);
	assert(elev_noon > 80.0 && elev_noon <= 90.0);

	/* Test 2: Equinox midnight at equator (lat=0, lon=0).
	   Epoch 1710979200 is approx March 21, 2024, 00:00:00 UTC (12 hrs later).
	   Sun should be far below horizon: elevation < -80 deg. */
	double equinox_midnight = equinox_noon + 12.0 * 3600.0;
	double elev_midnight = solar_elevation(equinox_midnight, 0.0, 0.0);
	printf("  Equator equinox midnight elevation: %.2f deg\n", elev_midnight);
	assert(elev_midnight < -80.0 && elev_midnight >= -90.0);

	/* Test 3: Elevation values must always stay within [-90.0, 90.0]. */
	for (int hour = 0; hour < 24; hour++) {
		double t = equinox_noon + (double)(hour * 3600);
		double el = solar_elevation(t, 45.0, 10.0);
		assert(el >= -90.0 && el <= 90.0);
	}

	/* Test 4: solar_table_fill generates ordered timestamps */
	double table[SOLAR_TIME_MAX];
	solar_table_fill(equinox_noon, 45.0, 10.0, table);

	/* Noon should be valid timestamp */
	assert(!isnan(table[SOLAR_TIME_NOON]));
	assert(table[SOLAR_TIME_NOON] > 0.0);

	/* Sunrise should occur before solar noon, and sunset after */
	if (!isnan(table[SOLAR_TIME_SUNRISE]) && !isnan(table[SOLAR_TIME_SUNSET])) {
		assert(table[SOLAR_TIME_SUNRISE] < table[SOLAR_TIME_NOON]);
		assert(table[SOLAR_TIME_SUNSET] > table[SOLAR_TIME_NOON]);
	}

	printf("test_solar PASSED!\n");
	return 0;
}
