/* test_colorramp.c -- Unit tests for color temperature gamma ramp calculations */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "../src/redshift.h"
#include "../src/colorramp.h"

static void init_linear_ramp(uint16_t *r, uint16_t *g, uint16_t *b, int size)
{
	for (int i = 0; i < size; i++) {
		uint16_t val = (uint16_t)(((double)i / (size - 1)) * UINT16_MAX);
		r[i] = val;
		g[i] = val;
		b[i] = val;
	}
}

int main(void)
{
	printf("Running test_colorramp...\n");

	const int ramp_size = 256;
	uint16_t r[256], g[256], b[256];

	/* Test 1: Neutral temperature (6500K), default brightness and gamma */
	color_setting_t neutral_setting = {
		.temperature = NEUTRAL_TEMP,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f
	};

	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &neutral_setting);

	/* At neutral 6500K, R, G, B should be equal and full-scale */
	assert(r[0] == 0 && g[0] == 0 && b[0] == 0);
	assert(r[ramp_size - 1] == UINT16_MAX);
	assert(g[ramp_size - 1] == UINT16_MAX);
	assert(b[ramp_size - 1] == UINT16_MAX);

	/* Check monotonicity */
	for (int i = 1; i < ramp_size; i++) {
		assert(r[i] >= r[i - 1]);
		assert(g[i] >= g[i - 1]);
		assert(b[i] >= b[i - 1]);
	}

	/* Test 2: Warm temperature (3000K) -> Red > Green > Blue */
	color_setting_t warm_setting = {
		.temperature = 3000,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f
	};

	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &warm_setting);
	printf("  Warm 3000K max values: R=%u, G=%u, B=%u\n",
	       r[ramp_size - 1], g[ramp_size - 1], b[ramp_size - 1]);

	assert(r[ramp_size - 1] > g[ramp_size - 1]);
	assert(g[ramp_size - 1] > b[ramp_size - 1]);

	/* Test 3: Cool temperature (9000K) -> Blue is prominent */
	color_setting_t cool_setting = {
		.temperature = 9000,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f
	};

	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &cool_setting);
	printf("  Cool 9000K max values: R=%u, G=%u, B=%u\n",
	       r[ramp_size - 1], g[ramp_size - 1], b[ramp_size - 1]);

	assert(b[ramp_size - 1] > r[ramp_size - 1]);

	/* Test 4: Brightness scaling (0.5) */
	color_setting_t half_bright = {
		.temperature = NEUTRAL_TEMP,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 0.5f
	};

	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &half_bright);
	/* Max value should be approx half of UINT16_MAX */
	uint16_t expected_max = (uint16_t)(UINT16_MAX * 0.5f);
	int diff = abs((int)r[ramp_size - 1] - (int)expected_max);
	assert(diff < 500);

	printf("test_colorramp PASSED!\n");
	return 0;
}
