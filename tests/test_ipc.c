/* test_ipc.c -- Unit tests for Jarheart IPC protocol and duration parser
   This file is part of Jarheart.

   Jarheart is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "ipc.h"

static void
test_duration_parser(void)
{
	printf("Testing IPC duration parser...\n");

	assert(ipc_parse_duration("30") == 30);
	assert(ipc_parse_duration("45s") == 45);
	assert(ipc_parse_duration("10m") == 600);
	assert(ipc_parse_duration("30min") == 1800);
	assert(ipc_parse_duration("1h") == 3600);
	assert(ipc_parse_duration("2hours") == 7200);
	assert(ipc_parse_duration("1d") == 86400);

	/* Whitespace handling */
	assert(ipc_parse_duration("  15m  ") == 900);
	assert(ipc_parse_duration("  2 h ") == 7200);

	/* Error cases */
	assert(ipc_parse_duration(NULL) == -1);
	assert(ipc_parse_duration("") == -1);
	assert(ipc_parse_duration("   ") == -1);
	assert(ipc_parse_duration("-5m") == -1);
	assert(ipc_parse_duration("foo") == -1);
	assert(ipc_parse_duration("10xyz") == -1);

	printf("  -> Duration parser tests passed.\n");
}

static void
test_command_dispatch(void)
{
	printf("Testing IPC command dispatcher...\n");

	daemon_ipc_state_t state;
	memset(&state, 0, sizeof(state));
	state.period = PERIOD_DAYTIME;
	state.transition_prog = 0.0;
	state.current_setting.temperature = 6500;
	state.current_setting.brightness = 1.0;
	state.current_setting.gamma[0] = 1.0;
	state.current_setting.gamma[1] = 1.0;
	state.current_setting.gamma[2] = 1.0;
	state.location.lat = 38.8951;
	state.location.lon = -77.0364;
	state.method_name = "randr";

	char resp[JARHEART_IPC_BUF_SIZE];

	/* Test 'status' */
	int r = ipc_dispatch_command("status", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "Status: Enabled") != NULL);
	assert(strstr(resp, "Period: Daytime") != NULL);
	assert(strstr(resp, "Color temperature: 6500K") != NULL);
	assert(strstr(resp, "Method: randr") != NULL);

	/* Test 'status --json' */
	r = ipc_dispatch_command("status --json", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "\"status\": \"Enabled\"") != NULL);
	assert(strstr(resp, "\"period\": \"Daytime\"") != NULL);
	assert(strstr(resp, "\"temperature\": 6500") != NULL);
	assert(strstr(resp, "\"method\": \"randr\"") != NULL);
	assert(strstr(resp, "\"emoji\": \"🤍\"") != NULL);
	assert(strstr(resp, "\"text\": \"🤍 6500K\"") != NULL);
	assert(strstr(resp, "\"sunlight_mode\": false") != NULL);
	assert(strstr(resp, "\"reading_mode\": false") != NULL);
	assert(strstr(resp, "\"halation_tamer\": false") != NULL);
	assert(strstr(resp, "\"melanopic_notch\": false") != NULL);
	assert(strstr(resp, "\"pwm_free\": false") != NULL);
	assert(strstr(resp, "\"strain_tracker\": false") != NULL);
	assert(strstr(resp, "\"vignette_mode\": false") != NULL);
	assert(strstr(resp, "\"cvd_mode\": \"none\"") != NULL);

	/* Test 'status --xfce' */
	r = ipc_dispatch_command("status --xfce", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "<txt>6500K</txt>") != NULL);
	assert(strstr(resp, "<tool>Jarheart: Enabled") != NULL);

	/* Test 'toggle' */
	r = ipc_dispatch_command("toggle", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.disabled == 1);
	assert(strstr(resp, "Status: Disabled") != NULL);

	r = ipc_dispatch_command("toggle", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.disabled == 0);
	assert(strstr(resp, "Status: Enabled") != NULL);

	/* Test 'pause 30m' */
	r = ipc_dispatch_command("pause 30m", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.disabled == 1);
	assert(state.pause_until > time(NULL));
	assert(strstr(resp, "Status: Paused") != NULL);
	assert(strstr(resp, "Remaining: 1800s") != NULL);

	/* Test 'resume' */
	r = ipc_dispatch_command("resume", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.disabled == 0);
	assert(state.pause_until == 0);
	assert(strstr(resp, "Status: Enabled") != NULL);

	/* Test 'set 3500' */
	r = ipc_dispatch_command("set 3500", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.override_temp == 3500);
	assert(strstr(resp, "Status: Override") != NULL);
	assert(strstr(resp, "Color temperature: 3500K") != NULL);

	/* Test 'darkroom' */
	r = ipc_dispatch_command("darkroom", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.darkroom == 1);
	assert(strstr(resp, "Darkroom mode: Enabled") != NULL);

	r = ipc_dispatch_command("darkroom off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.darkroom == 0);
	assert(strstr(resp, "Darkroom mode: Disabled") != NULL);

	/* Test 'movie' */
	r = ipc_dispatch_command("movie", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.movie_mode == 1);
	assert(state.movie_mode_until > time(NULL));
	assert(state.override_temp == 4200);
	assert(strstr(resp, "Movie mode: Enabled") != NULL);

	r = ipc_dispatch_command("movie off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.movie_mode == 0);
	assert(strstr(resp, "Movie mode: Disabled") != NULL);

	/* Test 'preset candle' */
	r = ipc_dispatch_command("preset candle", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.override_temp == 1900);
	assert(strcmp(state.current_preset, "Candle") == 0);
	assert(strstr(resp, "Candle (1900K)") != NULL);

	/* Test 'presets' list */
	r = ipc_dispatch_command("presets", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "ember") != NULL);
	assert(strstr(resp, "candle") != NULL);
	assert(strstr(resp, "moon") != NULL);
	assert(strstr(resp, "sunlight") != NULL);

	/* Test 'set daylight' */
	r = ipc_dispatch_command("set daylight", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.override_temp == 6500);

	/* Test 'schedule' */
	r = ipc_dispatch_command("schedule 06:00-07:30 19:00-20:30", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.schedule_use_time == 1);
	assert(state.dawn.start == 6 * 3600);
	assert(state.dawn.end == 7 * 3600 + 30 * 60);
	assert(strstr(resp, "Time-based") != NULL);

	r = ipc_dispatch_command("schedule solar", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.schedule_use_time == 0);
	assert(strstr(resp, "Solar elevation") != NULL);

	/* Test 'myopia-protect' */
	r = ipc_dispatch_command("myopia-protect on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.myopia_protect == 1);
	assert(state.override_temp == 2850);
	assert(strstr(resp, "Myopia protection mode: Enabled") != NULL);

	r = ipc_dispatch_command("myopia-protect off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.myopia_protect == 0);
	assert(strstr(resp, "Myopia protection mode: Disabled") != NULL);

	/* Test 'couple-brightness' */
	r = ipc_dispatch_command("couple-brightness on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.couple_brightness == 1);
	assert(strstr(resp, "Coupled brightness: Enabled") != NULL);

	r = ipc_dispatch_command("couple-brightness off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.couple_brightness == 0);
	assert(strstr(resp, "Coupled brightness: Disabled") != NULL);

	/* Test 'pacer' */
	r = ipc_dispatch_command("pacer 20m", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.pacer_interval == 1200);
	assert(strstr(resp, "20-20-20 Ocular Pacer: Enabled") != NULL);

	r = ipc_dispatch_command("pacer off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.pacer_interval == 0);
	assert(strstr(resp, "20-20-20 Ocular Pacer: Disabled") != NULL);

	/* Test 'ambient' */
	r = ipc_dispatch_command("ambient on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.ambient_balancer == 1);
	assert(strstr(resp, "Ambient contrast balancer: Enabled") != NULL);

	r = ipc_dispatch_command("ambient off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.ambient_balancer == 0);
	assert(strstr(resp, "Ambient contrast balancer: Disabled") != NULL);

	/* Test 'schedule diurnal' */
	r = ipc_dispatch_command("schedule diurnal", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.schedule_use_time == 2);
	assert(strstr(resp, "Schedule: Diurnal Tri-Phasic") != NULL);

	r = ipc_dispatch_command("schedule solar", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.schedule_use_time == 0);
	assert(strstr(resp, "Schedule: Solar elevation") != NULL);

	/* Test 'sunlight' */
	r = ipc_dispatch_command("sunlight on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.sunlight_mode == 1);
	assert(strstr(resp, "Sunlight / Outdoor mode: Enabled") != NULL);

	r = ipc_dispatch_command("status --json", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "\"sunlight_mode\": true") != NULL);
	assert(strstr(resp, "\"emoji\": \"💙\"") != NULL);

	r = ipc_dispatch_command("sunlight off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.sunlight_mode == 0);
	assert(strstr(resp, "Sunlight / Outdoor mode: Disabled") != NULL);

	r = ipc_dispatch_command("sunlight toggle", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.sunlight_mode == 1);
	assert(strstr(resp, "Sunlight / Outdoor mode: Enabled") != NULL);

	/* Test 'reading' / 'epaper' */
	r = ipc_dispatch_command("reading on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.reading_mode == 1);
	assert(strstr(resp, "E-Paper Reading Mode: Enabled") != NULL);

	r = ipc_dispatch_command("status --json", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "\"reading_mode\": true") != NULL);
	assert(strstr(resp, "\"emoji\": \"🤎\"") != NULL);

	r = ipc_dispatch_command("reading off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.reading_mode == 0);
	assert(strstr(resp, "E-Paper Reading Mode: Disabled") != NULL);

	/* Test 'halation' */
	r = ipc_dispatch_command("halation on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.halation_tamer == 1);
	assert(strstr(resp, "Astigmatism Halation Tamer: Enabled") != NULL);

	r = ipc_dispatch_command("halation off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.halation_tamer == 0);

	/* Test 'notch' */
	r = ipc_dispatch_command("notch on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.melanopic_notch == 1);
	assert(strstr(resp, "Melanopic Cyan Notch Filter: Enabled") != NULL);

	r = ipc_dispatch_command("notch off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.melanopic_notch == 0);

	/* Test 'pwm-free' */
	r = ipc_dispatch_command("pwm-free on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.pwm_free == 1);
	assert(strstr(resp, "PWM-Free Dimming Protocol: Enabled") != NULL);

	r = ipc_dispatch_command("pwm-free off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.pwm_free == 0);

	/* Test 'strain' */
	r = ipc_dispatch_command("strain on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.strain_tracker == 1);
	assert(strstr(resp, "Input Strain & Adaptive Blink Pacer: Enabled") != NULL);

	r = ipc_dispatch_command("strain off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.strain_tracker == 0);

	/* Test 'vignette' */
	r = ipc_dispatch_command("vignette on", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.vignette_mode == 1);
	assert(strstr(resp, "Peripheral Glare Shield: Enabled") != NULL);

	r = ipc_dispatch_command("vignette off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.vignette_mode == 0);

	/* Test 'stats' */
	state.total_active_seconds = 7200;
	state.restorative_seconds = 3600;
	state.hev_joules_saved = 142.5;
	state.pacer_breaks_completed = 4;
	r = ipc_dispatch_command("stats", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "Ocular Ergonomics Telemetry:") != NULL);
	assert(strstr(resp, "Active Exposure:       2 hours, 0 mins") != NULL);
	assert(strstr(resp, "Restorative (<3400K):  1 hours, 0 mins") != NULL);

	/* Test 'cvd' */
	r = ipc_dispatch_command("cvd protanopia", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.cvd_mode == CVD_PROTANOPIA);
	assert(strstr(resp, "protanopia") != NULL);

	r = ipc_dispatch_command("status --json", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "\"cvd_mode\": \"protanopia\"") != NULL);
	assert(strstr(resp, "\"emoji\": \"💜\"") != NULL);

	r = ipc_dispatch_command("cvd deutan", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.cvd_mode == CVD_DEUTERANOPIA);

	r = ipc_dispatch_command("cvd tritan", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.cvd_mode == CVD_TRITANOPIA);

	r = ipc_dispatch_command("cvd mono", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.cvd_mode == CVD_ACHROMATOPSIA);

	r = ipc_dispatch_command("cvd off", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.cvd_mode == CVD_NONE);

	/* Test 'reset' clears presets and modes */
	state.darkroom = 1;
	state.movie_mode = 1;
	state.sunlight_mode = 1;
	state.reading_mode = 1;
	state.halation_tamer = 1;
	state.melanopic_notch = 1;
	state.vignette_mode = 1;
	state.cvd_mode = CVD_PROTANOPIA;
	state.override_temp = 2000;
	r = ipc_dispatch_command("reset", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.darkroom == 0);
	assert(state.movie_mode == 0);
	assert(state.sunlight_mode == 0);
	assert(state.reading_mode == 0);
	assert(state.halation_tamer == 0);
	assert(state.melanopic_notch == 0);
	assert(state.vignette_mode == 0);
	assert(state.cvd_mode == CVD_NONE);
	assert(state.override_temp == 0);
	assert(state.current_preset[0] == '\0');
	assert(strstr(resp, "Status: Normal") != NULL);

	/* Test 'quit' */
	r = ipc_dispatch_command("quit", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(state.requested_exit == 1);
	assert(strstr(resp, "Jarheart daemon stopping...") != NULL);

	/* Test unknown command */
	r = ipc_dispatch_command("invalid_cmd_123", &state, resp, sizeof(resp));
	assert(r == 0);
	assert(strstr(resp, "Error: Unknown command") != NULL);

	printf("  -> Command dispatcher tests passed.\n");
}

int
main(void)
{
	printf("Running Jarheart IPC unit tests...\n");
	test_duration_parser();
	test_command_dispatch();
	printf("All IPC unit tests passed successfully!\n");
	return 0;
}
