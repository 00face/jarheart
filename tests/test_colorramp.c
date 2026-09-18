/* test_colorramp.c -- Unit tests for color temperature gamma ramp calculations */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

	/* Test 5: Darkroom Mode (Monochrome Red, Zero Green/Blue) */
	color_setting_t darkroom_setting = {
		.temperature = 3000,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.darkroom = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &darkroom_setting);
	for (int i = 0; i < ramp_size; i++) {
		assert(g[i] == 0);
		assert(b[i] == 0);
	}
	assert(r[ramp_size - 1] > 30000); /* Strong red channel */
	printf("  Darkroom mode verified: Green=0, Blue=0, Red max=%u\n", r[ramp_size - 1]);

	/* Test 6: Movie Mode (Shadow lift + Sky preservation) */
	color_setting_t movie_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.movie_mode = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &movie_setting);
	/* In movie mode, red and green should be warm, but blue at highlight should be elevated */
	assert(b[ramp_size - 1] > 40000);
	printf("  Movie mode verified: R=%u, G=%u, B=%u (sky preserved)\n",
	       r[ramp_size - 1], g[ramp_size - 1], b[ramp_size - 1]);

	/* Test 7: Kelvin Presets table lookup */
	const kelvin_preset_t *p_ember = colorramp_find_preset("ember");
	assert(p_ember != NULL && p_ember->temperature == 1200);

	const kelvin_preset_t *p_candle = colorramp_find_preset("candle");
	assert(p_candle != NULL && p_candle->temperature == 1900);

	const kelvin_preset_t *p_mars = colorramp_find_preset("mars");
	assert(p_mars != NULL && p_mars->temperature == 2100);

	const kelvin_preset_t *p_warm = colorramp_find_preset("warm-incandescent");
	assert(p_warm != NULL && p_warm->temperature == 2300);

	const kelvin_preset_t *p_warm2 = colorramp_find_preset("warm incandescent");
	assert(p_warm2 != NULL && p_warm2->temperature == 2300);

	const kelvin_preset_t *p_jupiter = colorramp_find_preset("jupiter");
	assert(p_jupiter != NULL && p_jupiter->temperature == 3200);

	const kelvin_preset_t *p_saturn = colorramp_find_preset("saturn");
	assert(p_saturn != NULL && p_saturn->temperature == 3800);

	const kelvin_preset_t *p_moon = colorramp_find_preset("moon");
	assert(p_moon != NULL && p_moon->temperature == 4100);

	const kelvin_preset_t *p_venus = colorramp_find_preset("venus");
	assert(p_venus != NULL && p_venus->temperature == 4800);

	const kelvin_preset_t *p_sun = colorramp_find_preset("sunlight");
	assert(p_sun != NULL && p_sun->temperature == 5500);

	const kelvin_preset_t *p_mercury = colorramp_find_preset("mercury");
	assert(p_mercury != NULL && p_mercury->temperature == 5800);

	const kelvin_preset_t *p_day = colorramp_find_preset("daylight");
	assert(p_day != NULL && p_day->temperature == 6500);

	const kelvin_preset_t *p_myopia = colorramp_find_preset("myopia-protect");
	assert(p_myopia != NULL && p_myopia->temperature == 2850);
	const kelvin_preset_t *p_myopia_alias = colorramp_find_preset("myopia");
	assert(p_myopia_alias != NULL && p_myopia_alias->temperature == 2850);

	const kelvin_preset_t *p_sky = colorramp_find_preset("clear-sky");
	assert(p_sky != NULL && p_sky->temperature == 7500);
	const kelvin_preset_t *p_sky_alias = colorramp_find_preset("outdoor");
	assert(p_sky_alias != NULL && p_sky_alias->temperature == 7500);

	const kelvin_preset_t *p_sunboost = colorramp_find_preset("sunlight-boost");
	assert(p_sunboost != NULL && p_sunboost->temperature == 8000);

	printf("  All Kelvin presets verified!\n");

	/* Test 8: Sunlight / Outdoor Mode (Anti-Glare Toe-Lift Curve) */
	color_setting_t sunlight_setting = {
		.temperature = 7500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.sunlight_mode = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &sunlight_setting);
	/* Toe-lift lifts low luminance values: r[0], g[0], b[0] must be > 0 (approx 0.18 of scale) */
	assert(r[0] > 5000);
	assert(g[0] > 5000);
	assert(b[0] > 5000);
	/* White point at ramp_size - 1 must reach full-scale on blue (7500K) without clipping */
	assert(b[ramp_size - 1] == UINT16_MAX);
	assert(r[ramp_size - 1] > 50000);
	assert(g[ramp_size - 1] > 50000);
	printf("  Sunlight mode toe-lift verified: blacks lifted (R[0]=%u), whites preserved (R[max]=%u)\n",
	       r[0], r[ramp_size - 1]);

	/* Test 9: Circadian Heart Emoji Mapping */
	assert(strcmp(colorramp_get_heart_emoji(6500, 1, 0, 0, 0, 0), "🖤") == 0);
	assert(strcmp(colorramp_get_heart_emoji(6500, 0, 1, 0, 0, 0), "❤️") == 0);
	assert(strcmp(colorramp_get_heart_emoji(4000, 0, 0, 0, 1, 0), "🤎") == 0);
	assert(strcmp(colorramp_get_heart_emoji(6500, 0, 0, 0, 0, CVD_PROTANOPIA), "💜") == 0);
	assert(strcmp(colorramp_get_heart_emoji(7500, 0, 0, 1, 0, 0), "💙") == 0);
	assert(strcmp(colorramp_get_heart_emoji(1200, 0, 0, 0, 0, 0), "❤️") == 0);
	assert(strcmp(colorramp_get_heart_emoji(1900, 0, 0, 0, 0, 0), "❤️‍🔥") == 0);
	assert(strcmp(colorramp_get_heart_emoji(2700, 0, 0, 0, 0, 0), "🧡") == 0);
	assert(strcmp(colorramp_get_heart_emoji(3400, 0, 0, 0, 0, 0), "💛") == 0);
	assert(strcmp(colorramp_get_heart_emoji(4200, 0, 0, 0, 0, 0), "💛") == 0);
	assert(strcmp(colorramp_get_heart_emoji(5500, 0, 0, 0, 0, 0), "🤍") == 0);
	assert(strcmp(colorramp_get_heart_emoji(6500, 0, 0, 0, 0, 0), "🤍") == 0);
	assert(strcmp(colorramp_get_heart_emoji(8000, 0, 0, 0, 0, 0), "💙") == 0);
	printf("  Circadian Heart Emoji mapping verified!\n");

	/* Test 10: E-Paper / Monochromatic Reading Mode */
	color_setting_t reading_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.reading_mode = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &reading_setting);
	/* Warm parchment: R >= G >= B */
	assert(r[ramp_size - 1] == UINT16_MAX);
	assert(g[ramp_size - 1] > 60000 && g[ramp_size - 1] < r[ramp_size - 1]);
	assert(b[ramp_size - 1] > 50000 && b[ramp_size - 1] < g[ramp_size - 1]);
	printf("  E-Paper reading mode verified: monochromatic warm parchment curve (R=%u, G=%u, B=%u)\n",
	       r[ramp_size - 1], g[ramp_size - 1], b[ramp_size - 1]);

	/* Test 11: Astigmatism Halation Tamer (Dynamic Range Compression) */
	color_setting_t halation_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.halation_tamer = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &halation_setting);
	/* Black floor lifted to ~5%: r[0], g[0], b[0] > 2500 */
	assert(r[0] > 2500 && r[0] < 4500);
	assert(g[0] > 2500 && g[0] < 4500);
	assert(b[0] > 2500 && b[0] < 4500);
	/* White ceiling compressed to ~86%: r[max] < 58000 */
	assert(r[ramp_size - 1] < 58000);
	assert(g[ramp_size - 1] < 58000);
	assert(b[ramp_size - 1] < 58000);
	printf("  Astigmatism halation tamer verified: black floor lifted (%u) and peak glare compressed (%u)\n",
	       r[0], r[ramp_size - 1]);

	/* Test 12: 480nm Melanopic Cyan Notch Filter */
	color_setting_t notch_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.melanopic_notch = 1
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &notch_setting);
	/* Blue channel attenuated by notch factor (~0.65 of 65535) */
	assert(b[ramp_size - 1] < 45000 && b[ramp_size - 1] > 40000);
	assert(r[ramp_size - 1] == UINT16_MAX);
	printf("  Melanopic notch filter verified: cyan-blue attenuated to %u\n", b[ramp_size - 1]);

	/* Test 13: Protanopia Assistance (Red Lumen Boost) */
	color_setting_t protan_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.cvd_mode = CVD_PROTANOPIA
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &protan_setting);
	/* Red channel boosted significantly above green in midtones */
	assert(r[ramp_size / 2] > g[ramp_size / 2] * 1.5);
	assert(r[0] > 2000); /* Lifted red black floor */
	printf("  Protanopia assistance verified: red midtone boost (%u) vs green (%u)\n",
	       r[ramp_size / 2], g[ramp_size / 2]);

	/* Test 14: Deuteranopia Assistance (Luminance Contrast Split) */
	color_setting_t deutan_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.cvd_mode = CVD_DEUTERANOPIA
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &deutan_setting);
	/* Red boosted, green attenuated for unambiguous brightness differentiation */
	assert(r[ramp_size / 2] > 40000);
	assert(g[ramp_size / 2] < 25000);
	printf("  Deuteranopia assistance verified: red (%u) vs green (%u) luminance contrast split\n",
	       r[ramp_size / 2], g[ramp_size / 2]);

	/* Test 15: Tritanopia Assistance (Blue-Yellow Contrast Shift) */
	color_setting_t tritan_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.cvd_mode = CVD_TRITANOPIA
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &tritan_setting);
	assert(b[ramp_size / 2] > g[ramp_size / 2]);
	printf("  Tritanopia assistance verified: blue (%u) vs green (%u)\n",
	       b[ramp_size / 2], g[ramp_size / 2]);

	/* Test 16: Achromatopsia Assistance (High-Contrast S-Curve Monochromacy) */
	color_setting_t achromat_setting = {
		.temperature = 6500,
		.gamma = { 1.0f, 1.0f, 1.0f },
		.brightness = 1.0f,
		.cvd_mode = CVD_ACHROMATOPSIA
	};
	init_linear_ramp(r, g, b, ramp_size);
	colorramp_fill(r, g, b, ramp_size, &achromat_setting);
	/* All channels identical (monochrome) */
	assert(r[ramp_size / 2] == g[ramp_size / 2]);
	assert(g[ramp_size / 2] == b[ramp_size / 2]);
	/* High contrast: deep blacks below midtone, high ceiling above midtone */
	assert(r[ramp_size / 5] < 10000);
	assert(r[(ramp_size * 4) / 5] > 55000);
	printf("  Achromatopsia assistance verified: pure monochrome S-curve (mid=%u)\n", r[ramp_size / 2]);

	printf("test_colorramp PASSED!\n");
	return 0;
}
