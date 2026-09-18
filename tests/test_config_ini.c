/* test_config_ini.c -- Unit tests for INI configuration parser */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../src/config-ini.h"

int main(void)
{
	printf("Running test_config_ini...\n");

	/* Create temporary test config */
	char temp_path[] = "/tmp/test_jarheart_XXXXXX.conf";
	int fd = mkstemps(temp_path, 5);
	assert(fd >= 0);

	const char test_content[] =
		"# Top-level comment\n"
		"; Semicolon comment\n"
		"\n"
		"[jarheart]\n"
		"temp-day = 5500\n"
		"temp-night = 3700\n"
		"transition = 1\n"
		"gamma = 0.8:0.7:0.9\n"
		"location-provider = manual\n"
		"\n"
		"[manual]\n"
		"lat = 37.77\n"
		"lon = -122.41\n"
		"\n"
		"[randr]\n"
		"screen = 0\n";

	ssize_t written = write(fd, test_content, strlen(test_content));
	assert(written == (ssize_t)strlen(test_content));
	close(fd);

	/* Initialize and parse */
	config_ini_state_t state;
	int r = config_ini_init(&state, temp_path);
	assert(r == 0);

	/* Test 1: Query [jarheart] section */
	config_ini_section_t *sec_jh = config_ini_get_section(&state, "jarheart");
	assert(sec_jh != NULL);
	assert(strcmp(sec_jh->name, "jarheart") == 0);

	/* Test 2: Check settings inside [jarheart] */
	int found_temp_day = 0;
	int found_temp_night = 0;
	for (config_ini_setting_t *s = sec_jh->settings; s != NULL; s = s->next) {
		if (strcmp(s->name, "temp-day") == 0) {
			assert(strcmp(s->value, "5500") == 0);
			found_temp_day = 1;
		} else if (strcmp(s->name, "temp-night") == 0) {
			assert(strcmp(s->value, "3700") == 0);
			found_temp_night = 1;
		}
	}
	assert(found_temp_day == 1);
	assert(found_temp_night == 1);

	/* Test 3: Query [manual] section */
	config_ini_section_t *sec_man = config_ini_get_section(&state, "manual");
	assert(sec_man != NULL);
	int found_lat = 0;
	for (config_ini_setting_t *s = sec_man->settings; s != NULL; s = s->next) {
		if (strcmp(s->name, "lat") == 0) {
			assert(strcmp(s->value, "37.77") == 0);
			found_lat = 1;
		}
	}
	assert(found_lat == 1);

	/* Test 4: Query non-existent section */
	config_ini_section_t *sec_none = config_ini_get_section(&state, "nonexistent");
	assert(sec_none == NULL);

	/* Clean up */
	config_ini_free(&state);
	unlink(temp_path);

	printf("test_config_ini PASSED!\n");
	return 0;
}
