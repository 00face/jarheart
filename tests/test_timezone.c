#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "location-timezone.h"

int main(void)
{
	printf("Running test_timezone...\n");

	location_t loc;
	char tz_name[128] = {0};
	int r = location_timezone_resolve(&loc, tz_name, sizeof(tz_name));

	if (r == 0) {
		printf("  Resolved timezone: %s (lat=%.2f, lon=%.2f)\n",
		       tz_name, loc.lat, loc.lon);
		assert(!isnan(loc.lat));
		assert(!isnan(loc.lon));
		assert(loc.lat >= -90.0f && loc.lat <= 90.0f);
		assert(loc.lon >= -180.0f && loc.lon <= 180.0f);
	} else {
		printf("  Timezone resolution skipped (no /etc/localtime or tzdata)\n");
	}

	printf("test_timezone PASSED!\n");
	return 0;
}
