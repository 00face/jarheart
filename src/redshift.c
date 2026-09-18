/* redshift.c -- Main program source
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2009-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <locale.h>
#include <errno.h>
#include <time.h>

/* poll.h is not available on Windows but there is no Windows location provider
   using polling. On Windows, we just define some stubs to make things compile.
   */
#ifndef _WIN32
# include <poll.h>
#else
#define POLLIN 0
struct pollfd {
	int fd;
	short events;
	short revents;
};
int poll(struct pollfd *fds, int nfds, int timeout) { abort(); return -1; }
#endif

#if defined(HAVE_SIGNAL_H) && !defined(__WIN32__)
# include <signal.h>
#endif

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
# define N_(s) (s)
#else
# define _(s) s
# define N_(s) s
# define gettext(s) s
#endif

#include <stdint.h>
#include "redshift.h"
#include "config-ini.h"
#include "solar.h"
#include "systemtime.h"
#include "hooks.h"
#include "signals.h"
#include "options.h"
#include "ipc.h"
#include "checks.h"

/* pause() is not defined on windows platform but is not needed either.
   Use a noop macro instead. */
#ifdef __WIN32__
# define pause()
#endif

#include "gamma-dummy.h"

#ifdef ENABLE_WAYLAND
# include "gamma-wayland.h"
#endif

#ifdef ENABLE_DRM
# include "gamma-drm.h"
#endif

#ifdef ENABLE_RANDR
# include "gamma-randr.h"
#endif

#ifdef ENABLE_VIDMODE
# include "gamma-vidmode.h"
#endif

#ifdef ENABLE_QUARTZ
# include "gamma-quartz.h"
#endif

#ifdef ENABLE_WINGDI
# include "gamma-w32gdi.h"
#endif


#include "location-manual.h"
#include "location-timezone.h"

#ifdef ENABLE_GEOCLUE2
# include "location-geoclue2.h"
#endif

#ifdef ENABLE_CORELOCATION
# include "location-corelocation.h"
#endif

#undef CLAMP
#define CLAMP(lo,mid,up)  (((lo) > (mid)) ? (lo) : (((mid) < (up)) ? (mid) : (up)))


/* Bounds for parameters. */
#define MIN_LAT   -90.0
#define MAX_LAT    90.0
#define MIN_LON  -180.0
#define MAX_LON   180.0
#define MIN_TEMP   1000
#define MAX_TEMP  25000
#define MIN_BRIGHTNESS  0.1
#define MAX_BRIGHTNESS  1.0
#define MIN_GAMMA   0.1
#define MAX_GAMMA  10.0

/* Duration of sleep between screen updates (milliseconds). */
#define SLEEP_DURATION        5000
#define SLEEP_DURATION_SHORT  100

#ifndef _WIN32
static void
send_desktop_notification(const char *summary, const char *body)
{
	pid_t pid = fork();
	if (pid == 0) {
		close(0);
		close(1);
		close(2);
		execl("/usr/bin/notify-send", "notify-send",
		      "-a", "Jarheart",
		      "-i", "jarheart",
		      "-t", "4500",
		      summary, body, (char *)NULL);
		_exit(0);
	}
}
#endif

/* Length of fade in numbers of short sleep durations. */
#define FADE_LENGTH  40


/* Names of periods of day */
static const char *period_names[] = {
	/* TRANSLATORS: Name printed when period of day is unknown */
	N_("None"),
	N_("Daytime"),
	N_("Night"),
	N_("Transition")
};


/* Determine which period we are currently in based on time offset. */
static period_t
get_period_from_time(const transition_scheme_t *transition, int time_offset)
{
	if (time_offset < transition->dawn.start ||
	    time_offset >= transition->dusk.end) {
		return PERIOD_NIGHT;
	} else if (time_offset >= transition->dawn.end &&
		   time_offset < transition->dusk.start) {
		return PERIOD_DAYTIME;
	} else {
		return PERIOD_TRANSITION;
	}
}

/* Determine which period we are currently in based on solar elevation. */
static period_t
get_period_from_elevation(
	const transition_scheme_t *transition, double elevation)
{
	if (elevation < transition->low) {
		return PERIOD_NIGHT;
	} else if (elevation < transition->high) {
		return PERIOD_TRANSITION;
	} else {
		return PERIOD_DAYTIME;
	}
}

/* Determine how far through the transition we are based on time offset. */
static double
get_transition_progress_from_time(
	const transition_scheme_t *transition, int time_offset)
{
	if (time_offset < transition->dawn.start ||
	    time_offset >= transition->dusk.end) {
		return 0.0;
	} else if (time_offset < transition->dawn.end) {
		return (transition->dawn.start - time_offset) /
			(double)(transition->dawn.start -
				transition->dawn.end);
	} else if (time_offset > transition->dusk.start) {
		return (transition->dusk.end - time_offset) /
			(double)(transition->dusk.end -
				transition->dusk.start);
	} else {
		return 1.0;
	}
}

/* Determine how far through the transition we are based on elevation. */
static double
get_transition_progress_from_elevation(
	const transition_scheme_t *transition, double elevation)
{
	if (elevation < transition->low) {
		return 0.0;
	} else if (elevation < transition->high) {
		return (transition->low - elevation) /
			(transition->low - transition->high);
	} else {
		return 1.0;
	}
}

/* Return number of seconds since midnight from timestamp. */
static int
get_seconds_since_midnight(double timestamp)
{
	time_t t = (time_t)timestamp;
	struct tm tm;
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	return tm.tm_sec + tm.tm_min * 60 + tm.tm_hour * 3600;
}

/* Print verbose description of the given period. */
static void
print_period(period_t period, double transition)
{
	switch (period) {
	case PERIOD_NONE:
	case PERIOD_NIGHT:
	case PERIOD_DAYTIME:
		printf(_("Period: %s\n"), gettext(period_names[period]));
		break;
	case PERIOD_TRANSITION:
		printf(_("Period: %s (%.2f%% day)\n"),
		       gettext(period_names[period]),
		       transition*100);
		break;
	}
}

/* Print location */
static void
print_location(const location_t *location)
{
	/* TRANSLATORS: Abbreviation for `north' */
	const char *north = _("N");
	/* TRANSLATORS: Abbreviation for `south' */
	const char *south = _("S");
	/* TRANSLATORS: Abbreviation for `east' */
	const char *east = _("E");
	/* TRANSLATORS: Abbreviation for `west' */
	const char *west = _("W");

	/* TRANSLATORS: Append degree symbols after %f if possible.
	   The string following each number is an abreviation for
	   north, source, east or west (N, S, E, W). */
	printf(_("Location: %.2f %s, %.2f %s\n"),
	       fabs(location->lat), location->lat >= 0.f ? north : south,
	       fabs(location->lon), location->lon >= 0.f ? east : west);
}

/* Interpolate color setting structs given alpha. */
static void
interpolate_color_settings(
	const color_setting_t *first,
	const color_setting_t *second,
	double alpha,
	color_setting_t *result)
{
	alpha = CLAMP(0.0, alpha, 1.0);

	result->temperature = (1.0-alpha)*first->temperature +
		alpha*second->temperature;
	result->brightness = (1.0-alpha)*first->brightness +
		alpha*second->brightness;
	for (int i = 0; i < 3; i++) {
		result->gamma[i] = (1.0-alpha)*first->gamma[i] +
			alpha*second->gamma[i];
	}
	result->darkroom = second->darkroom;
	result->movie_mode = second->movie_mode;
}

/* Interpolate color setting structs transition scheme. */
static void
interpolate_transition_scheme(
	const transition_scheme_t *transition,
	double alpha,
	color_setting_t *result)
{
	const color_setting_t *day = &transition->day;
	const color_setting_t *night = &transition->night;

	alpha = CLAMP(0.0, alpha, 1.0);
	interpolate_color_settings(night, day, alpha, result);
}

/* Return 1 if color settings have major differences, otherwise 0.
   Used to determine if a fade should be applied in continual mode. */
static int
color_setting_diff_is_major(
	const color_setting_t *first,
	const color_setting_t *second)
{
	return (abs(first->temperature - second->temperature) > 25 ||
		fabsf(first->brightness - second->brightness) > 0.1 ||
		fabsf(first->gamma[0] - second->gamma[0]) > 0.1 ||
		fabsf(first->gamma[1] - second->gamma[1]) > 0.1 ||
		fabsf(first->gamma[2] - second->gamma[2]) > 0.1 ||
		first->darkroom != second->darkroom ||
		first->movie_mode != second->movie_mode);
}

/* Reset color setting to default values. */
static void
color_setting_reset(color_setting_t *color)
{
	color->temperature = NEUTRAL_TEMP;
	color->gamma[0] = 1.0;
	color->gamma[1] = 1.0;
	color->gamma[2] = 1.0;
	color->brightness = 1.0;
	color->darkroom = 0;
	color->movie_mode = 0;
}


static int
provider_try_start(const location_provider_t *provider,
		   location_state_t **state, config_ini_state_t *config,
		   char *args)
{
	int r;

	r = provider->init(state);
	if (r < 0) {
		fprintf(stderr, _("Initialization of %s failed.\n"),
			provider->name);
		return -1;
	}

	/* Set provider options from config file. */
	config_ini_section_t *section =
		config_ini_get_section(config, provider->name);
	if (section != NULL) {
		config_ini_setting_t *setting = section->settings;
		while (setting != NULL) {
			r = provider->set_option(*state, setting->name,
						 setting->value);
			if (r < 0) {
				provider->free(*state);
				fprintf(stderr, _("Failed to set %s"
						  " option.\n"),
					provider->name);
				/* TRANSLATORS: `help' must not be
				   translated. */
				fprintf(stderr, _("Try `-l %s:help' for more"
						  " information.\n"),
					provider->name);
				return -1;
			}
			setting = setting->next;
		}
	}

	/* Set provider options from command line. */
	const char *manual_keys[] = { "lat", "lon" };
	size_t i = 0;
	while (args != NULL) {
		char *next_arg = strchr(args, ':');
		if (next_arg != NULL) *(next_arg++) = '\0';

		const char *key = args;
		char *value = strchr(args, '=');
		if (value == NULL) {
			/* The options for the "manual" method can be set
			   without keys on the command line for convencience
			   and for backwards compatability. We add the proper
			   keys here before calling set_option(). */
			if (strcmp(provider->name, "manual") == 0 &&
			    i < sizeof(manual_keys)/sizeof(manual_keys[0])) {
				key = manual_keys[i];
				value = args;
			} else {
				fprintf(stderr, _("Failed to parse option `%s'.\n"),
					args);
				return -1;
			}
		} else {
			*(value++) = '\0';
		}

		r = provider->set_option(*state, key, value);
		if (r < 0) {
			provider->free(*state);
			fprintf(stderr, _("Failed to set %s option.\n"),
				provider->name);
			/* TRANSLATORS: `help' must not be translated. */
			fprintf(stderr, _("Try `-l %s:help' for more"
					  " information.\n"), provider->name);
			return -1;
		}

		args = next_arg;
		i += 1;
	}

	/* Start provider. */
	r = provider->start(*state);
	if (r < 0) {
		provider->free(*state);
		fprintf(stderr, _("Failed to start provider %s.\n"),
			provider->name);
		return -1;
	}

	return 0;
}

static int
method_try_start(const gamma_method_t *method,
		 gamma_state_t **state, config_ini_state_t *config, char *args)
{
	int r;

	r = method->init(state);
	if (r < 0) {
		fprintf(stderr, _("Initialization of %s failed.\n"),
			method->name);
		return -1;
	}

	/* Set method options from config file. */
	config_ini_section_t *section =
		config_ini_get_section(config, method->name);
	if (section != NULL) {
		config_ini_setting_t *setting = section->settings;
		while (setting != NULL) {
			r = method->set_option(
				*state, setting->name, setting->value);
			if (r < 0) {
				method->free(*state);
				fprintf(stderr, _("Failed to set %s"
						  " option.\n"),
					method->name);
				/* TRANSLATORS: `help' must not be
				   translated. */
				fprintf(stderr, _("Try `-m %s:help' for more"
						  " information.\n"),
					method->name);
				return -1;
			}
			setting = setting->next;
		}
	}

	/* Set method options from command line. */
	while (args != NULL) {
		char *next_arg = strchr(args, ':');
		if (next_arg != NULL) *(next_arg++) = '\0';

		const char *key = args;
		char *value = strchr(args, '=');
		if (value == NULL) {
			fprintf(stderr, _("Failed to parse option `%s'.\n"),
				args);
			return -1;
		} else {
			*(value++) = '\0';
		}

		r = method->set_option(*state, key, value);
		if (r < 0) {
			method->free(*state);
			fprintf(stderr, _("Failed to set %s option.\n"),
				method->name);
			/* TRANSLATORS: `help' must not be translated. */
			fprintf(stderr, _("Try -m %s:help' for more"
					  " information.\n"), method->name);
			return -1;
		}

		args = next_arg;
	}

	/* Start method. */
	r = method->start(*state);
	if (r < 0) {
		method->free(*state);
		fprintf(stderr, _("Failed to start adjustment method %s.\n"),
			method->name);
		return -1;
	}

	return 0;
}


/* Check whether gamma is within allowed levels. */
static int
gamma_is_valid(const float gamma[3])
{
	return !(gamma[0] < MIN_GAMMA ||
		 gamma[0] > MAX_GAMMA ||
		 gamma[1] < MIN_GAMMA ||
		 gamma[1] > MAX_GAMMA ||
		 gamma[2] < MIN_GAMMA ||
		 gamma[2] > MAX_GAMMA);
}

/* Check whether location is valid.
   Prints error message on stderr and returns 0 if invalid, otherwise
   returns 1. */
static int
location_is_valid(const location_t *location)
{
	/* Latitude */
	if (location->lat < MIN_LAT || location->lat > MAX_LAT) {
		/* TRANSLATORS: Append degree symbols if possible. */
		fprintf(stderr,
			_("Latitude must be between %.1f and %.1f.\n"),
			MIN_LAT, MAX_LAT);
		return 0;
	}

	/* Longitude */
	if (location->lon < MIN_LON || location->lon > MAX_LON) {
		/* TRANSLATORS: Append degree symbols if possible. */
		fprintf(stderr,
			_("Longitude must be between"
			  " %.1f and %.1f.\n"), MIN_LON, MAX_LON);
		return 0;
	}

	return 1;
}

/* Wait for location to become available from provider.
   Waits until timeout (milliseconds) has elapsed or forever if timeout
   is -1. Writes location to loc. Returns -1 on error,
   0 if timeout was reached, 1 if location became available. */
static int
provider_get_location(
	const location_provider_t *provider, location_state_t *state,
	int timeout, location_t *loc)
{
	int available = 0;
	struct pollfd pollfds[1];
	while (!available) {
		int loc_fd = provider->get_fd(state);
		if (loc_fd >= 0) {
			/* Provider is dynamic. */
			/* TODO: This should use a monotonic time source. */
			double now;
			int r = systemtime_get_time(&now);
			if (r < 0) {
				fputs(_("Unable to read system time.\n"),
				      stderr);
				return -1;
			}

			/* Poll on file descriptor until ready. */
			pollfds[0].fd = loc_fd;
			pollfds[0].events = POLLIN;
			r = poll(pollfds, 1, timeout);
			if (r < 0) {
				perror("poll");
				return -1;
			} else if (r == 0) {
				return 0;
			}

			double later;
			r = systemtime_get_time(&later);
			if (r < 0) {
				fputs(_("Unable to read system time.\n"),
				      stderr);
				return -1;
			}

			/* Adjust timeout by elapsed time */
			if (timeout >= 0) {
				timeout -= (later - now) * 1000;
				timeout = timeout < 0 ? 0 : timeout;
			}
		}


		int r = provider->handle(state, loc, &available);
		if (r < 0) return -1;
	}

	return 1;
}

/* Easing function for fade.
   See https://github.com/mietek/ease-tween */
static double
ease_fade(double t)
{
	if (t <= 0) return 0;
	if (t >= 1) return 1;
	return 1.0042954579734844 * exp(
		-6.4041738958415664 * exp(-7.2908241330981340 * t));
}


/* Run continual mode loop
   This is the main loop of the continual mode which keeps track of the
   current time and continuously updates the screen to the appropriate
   color temperature. */
static int
run_continual_mode(const location_provider_t *provider,
		   location_state_t *location_state,
		   const transition_scheme_t *scheme,
		   const gamma_method_t *method,
		   gamma_state_t *method_state,
		   int use_fade, int preserve_gamma, int verbose)
{
	int r;

	/* Short fade parameters */
	int fade_length = 0;
	int fade_time = 0;
	color_setting_t fade_start_interp;

	r = signals_install_handlers();
	if (r < 0) {
		return r;
	}

#ifndef _WIN32
	ipc_t ipc;
	int ipc_active = 0;
	r = ipc_init(&ipc);
	if (r == -EEXIST) {
		fputs(_("Another instance of jarheart is already running.\n"), stderr);
		return -1;
	} else if (r == 0) {
		ipc_active = 1;
	}
	daemon_ipc_state_t ipc_state;
	memset(&ipc_state, 0, sizeof(ipc_state));
	ipc_state.schedule_use_time = scheme->use_time;
	ipc_state.dawn = scheme->dawn;
	ipc_state.dusk = scheme->dusk;
#endif

	/* Save previous parameters so we can avoid printing status updates if
	   the values did not change. */
	period_t prev_period = PERIOD_NONE;

	/* Previous target color setting and current actual color setting.
	   Actual color setting takes into account the current color fade. */
	color_setting_t prev_target_interp;
	color_setting_reset(&prev_target_interp);

	color_setting_t interp;
	color_setting_reset(&interp);

	location_t loc = { NAN, NAN };
	int need_location = !scheme->use_time;
	if (need_location) {
		fputs(_("Waiting for initial location"
			" to become available...\n"), stderr);

		/* Get initial location from provider. Use a 4s timeout for geoclue2 to prevent hanging. */
		int init_timeout = (strcmp(provider->name, "geoclue2") == 0) ? 4000 : -1;
		r = provider_get_location(provider, location_state, init_timeout, &loc);
		if (r <= 0) {
			if (strcmp(provider->name, "geoclue2") == 0) {
				char tz_name[128] = {0};
				if (location_timezone_resolve(&loc, tz_name, sizeof(tz_name)) == 0) {
					fprintf(stderr, _("GeoClue2 unavailable (no WiFi/GPS). Automatically resolved location from system timezone (%s): %.2f N, %.2f W\n"),
					        tz_name, loc.lat, -loc.lon);
					r = 1;
				}
			}
		}
		if (r <= 0) {
			fputs(_("Unable to get location"
				" from provider.\n"), stderr);
			return -1;
		}

		if (!location_is_valid(&loc)) {
			fputs(_("Invalid location returned from provider.\n"),
			      stderr);
			return -1;
		}

		print_location(&loc);
	}

	if (verbose) {
		printf(_("Color temperature: %uK\n"), interp.temperature);
		printf(_("Brightness: %.2f\n"), interp.brightness);
	}

	/* Continuously adjust color temperature */
	int done = 0;
	int prev_disabled = 1;
	int disabled = 0;
	int location_available = 1;
	while (1) {
#ifndef _WIN32
		time_t now_epoch = time(NULL);
		if (ipc_state.pause_until > 0) {
			if (now_epoch >= ipc_state.pause_until) {
				ipc_state.pause_until = 0;
				disabled = 0;
			} else {
				disabled = 1;
			}
		}

		/* Check movie mode expiration */
		if (ipc_state.movie_mode && ipc_state.movie_mode_until > 0) {
			if (now_epoch >= ipc_state.movie_mode_until) {
				ipc_state.movie_mode = 0;
				ipc_state.movie_mode_until = 0;
				if (strcmp(ipc_state.current_preset, "Movie") == 0) {
					ipc_state.current_preset[0] = '\0';
				}
			}
		}

		/* Occasional gentle timezone check every 60 seconds */
		static time_t last_tz_check = 0;
		if (now_epoch - last_tz_check >= 60) {
			last_tz_check = now_epoch;
			checks_result_t tz_chk;
			if (checks_check_timezone(&tz_chk) == 0 && tz_chk.timezone_changed) {
				if (verbose) {
					printf(_("Timezone update detected: %s\n"), tz_chk.timezone_summary);
				}
				loc = tz_chk.timezone_location;
			}
		}

		/* 20-20-20 Ocular Relaxation Pacer */
		if (ipc_state.pacer_interval > 0 && !disabled) {
			if (ipc_state.last_pacer_time == 0) {
				ipc_state.last_pacer_time = now_epoch;
			} else if (now_epoch - ipc_state.last_pacer_time >= ipc_state.pacer_interval) {
				ipc_state.last_pacer_time = now_epoch;
				ipc_state.pacer_breathe = 3;
				ipc_state.pacer_breaks_completed++;
				send_desktop_notification("20-20-20 Ocular Rest",
					"Look 20 feet away for 20 seconds to relax ciliary muscles and replenish tear film.");
			}
		}

		/* Input-Velocity Strain & Adaptive Blink Pacer */
		static uint64_t last_input_interrupts = 0;
		static time_t strain_active_since = 0;
		static time_t last_strain_check = 0;
		if (ipc_state.strain_tracker && !disabled) {
			if (now_epoch - last_strain_check >= 10) {
				last_strain_check = now_epoch;
				uint64_t cur_interrupts = checks_get_input_interrupts();
				if (last_input_interrupts > 0 && cur_interrupts > last_input_interrupts) {
					if (strain_active_since == 0) strain_active_since = now_epoch;
					if (now_epoch - strain_active_since >= 1800) {
						ipc_state.pacer_breathe = 3;
						ipc_state.pacer_breaks_completed++;
						send_desktop_notification("Tear-Film Replenishment Break",
							"Continuous typing detected for 30m. Rest your eyes and take 10 slow, complete blinks.");
						strain_active_since = now_epoch;
					}
				} else {
					strain_active_since = 0;
				}
				last_input_interrupts = cur_interrupts;
			}
		}

		/* PWM-Free Protocol: Enforce maximum hardware panel backlight */
		static time_t last_pwm_check = 0;
		if (ipc_state.pwm_free && !disabled) {
			if (now_epoch - last_pwm_check >= 5) {
				last_pwm_check = now_epoch;
				checks_ensure_pwm_free();
			}
		}
#endif

		/* Check to see if disable signal was caught */
		if (disable && !done) {
			disabled = !disabled;
			disable = 0;
#ifndef _WIN32
			ipc_state.pause_until = 0;
			ipc_state.override_temp = 0;
#endif
		}

		/* Check to see if exit signal was caught */
		if (exiting) {
			if (done) {
				/* On second signal stop the ongoing fade. */
				break;
			} else {
				done = 1;
				disabled = 1;
			}
			exiting = 0;
		}

		/* Print status change and trigger hooks */
		if (disabled != prev_disabled) {
			if (verbose) {
				printf(_("Status: %s\n"), disabled ?
				       _("Disabled") : _("Enabled"));
			}
			hooks_signal_status_change("status-changed",
				disabled ? "disabled" : "enabled");
		}

		prev_disabled = disabled;

		/* Read timestamp */
		double now;
		r = systemtime_get_time(&now);
		if (r < 0) {
			fputs(_("Unable to read system time.\n"), stderr);
			return -1;
		}

		period_t period;
		double transition_prog;
		color_setting_t target_interp;

#ifndef _WIN32
		if (ipc_state.schedule_use_time == 2) {
			/* WO-022: Diurnal Tri-Phasic Schedule */
			int time_offset = get_seconds_since_midnight(now);
			target_interp.gamma[0] = 1.0f;
			target_interp.gamma[1] = 1.0f;
			target_interp.gamma[2] = 1.0f;
			target_interp.darkroom = 0;
			target_interp.movie_mode = 0;
			if (time_offset < 21600) { /* 00:00 - 06:00 */
				period = PERIOD_NIGHT;
				transition_prog = 0.0;
				target_interp.temperature = 1900;
				target_interp.brightness = 0.40f;
			} else if (time_offset < 28800) { /* 06:00 - 08:00 */
				period = PERIOD_TRANSITION;
				transition_prog = (time_offset - 21600) / 7200.0;
				target_interp.temperature = (int)(1900.0 + transition_prog * (6500 - 1900));
				target_interp.brightness = (float)(0.40 + transition_prog * 0.60);
			} else if (time_offset < 43200) { /* 08:00 - 12:00 */
				period = PERIOD_DAYTIME;
				transition_prog = 1.0;
				target_interp.temperature = 6500;
				target_interp.brightness = 1.00f;
			} else if (time_offset < 48600) { /* 12:00 - 13:30 */
				period = PERIOD_TRANSITION;
				transition_prog = (time_offset - 43200) / 5400.0;
				target_interp.temperature = (int)(6500.0 - transition_prog * (6500 - 4000));
				target_interp.brightness = (float)(1.00 - transition_prog * 0.20);
			} else if (time_offset < 61200) { /* 13:30 - 17:00 */
				period = PERIOD_DAYTIME;
				transition_prog = 1.0;
				target_interp.temperature = 4000;
				target_interp.brightness = 0.80f;
			} else if (time_offset < 79200) { /* 17:00 - 22:00 */
				period = PERIOD_TRANSITION;
				transition_prog = (time_offset - 61200) / 18000.0;
				target_interp.temperature = (int)(4000.0 - transition_prog * (4000 - 2300));
				target_interp.brightness = (float)(0.80 - transition_prog * 0.25);
			} else { /* 22:00 - 24:00 */
				period = PERIOD_NIGHT;
				transition_prog = 0.0;
				target_interp.temperature = 1900;
				target_interp.brightness = 0.40f;
			}
		} else
#endif
		if (scheme->use_time || (
#ifndef _WIN32
			ipc_state.schedule_use_time == 1
#else
			0
#endif
		)) {
			int time_offset = get_seconds_since_midnight(now);
			transition_scheme_t cur_scheme = *scheme;
#ifndef _WIN32
			if (ipc_state.schedule_use_time == 1) {
				cur_scheme.dawn = ipc_state.dawn;
				cur_scheme.dusk = ipc_state.dusk;
			}
#endif
			period = get_period_from_time(&cur_scheme, time_offset);
			transition_prog = get_transition_progress_from_time(
				&cur_scheme, time_offset);
			interpolate_transition_scheme(
				scheme, transition_prog, &target_interp);
		} else {
			/* Current angular elevation of the sun */
			double elevation = solar_elevation(
				now, loc.lat, loc.lon);

			period = get_period_from_elevation(scheme, elevation);
			transition_prog =
				get_transition_progress_from_elevation(
					scheme, elevation);
			interpolate_transition_scheme(
				scheme, transition_prog, &target_interp);
		}

#ifndef _WIN32
		if (ipc_state.darkroom) {
			target_interp.darkroom = 1;
			target_interp.movie_mode = 0;
			target_interp.sunlight_mode = 0;
			target_interp.reading_mode = 0;
		} else if (ipc_state.reading_mode) {
			target_interp.darkroom = 0;
			target_interp.movie_mode = 0;
			target_interp.sunlight_mode = 0;
			target_interp.reading_mode = 1;
			target_interp.temperature = 4000;
		} else if (ipc_state.sunlight_mode) {
			target_interp.darkroom = 0;
			target_interp.movie_mode = 0;
			target_interp.sunlight_mode = 1;
			target_interp.reading_mode = 0;
			target_interp.temperature = 7500;
			target_interp.brightness = 1.00f;
		} else if (ipc_state.movie_mode) {
			target_interp.darkroom = 0;
			target_interp.movie_mode = 1;
			target_interp.sunlight_mode = 0;
			target_interp.reading_mode = 0;
			target_interp.temperature = 4200;
		} else if (ipc_state.myopia_protect) {
			target_interp.darkroom = 0;
			target_interp.movie_mode = 0;
			target_interp.sunlight_mode = 0;
			target_interp.reading_mode = 0;
			target_interp.temperature = 2850;
			if (target_interp.brightness > 0.60f) {
				target_interp.brightness = 0.60f;
			}
		} else {
			target_interp.darkroom = 0;
			target_interp.movie_mode = 0;
			target_interp.sunlight_mode = 0;
			target_interp.reading_mode = 0;
			if (ipc_state.override_temp > 0) {
				target_interp.temperature = ipc_state.override_temp;
			}
		}
		target_interp.halation_tamer = ipc_state.halation_tamer;
		target_interp.melanopic_notch = ipc_state.melanopic_notch;
		target_interp.cvd_mode = ipc_state.cvd_mode;

		/* Coupled brightness: dynamically scale brightness along Kruithof comfort curve */
		if (ipc_state.couple_brightness && !disabled && !ipc_state.darkroom && !ipc_state.sunlight_mode) {
			float t_norm = (float)(target_interp.temperature - 2000) / (float)(6500 - 2000);
			if (t_norm < 0.0f) t_norm = 0.0f;
			if (t_norm > 1.0f) t_norm = 1.0f;
			float coupled_b = 0.55f + 0.45f * t_norm;
			if (target_interp.brightness > coupled_b) {
				target_interp.brightness = coupled_b;
			}
		}

		/* WO-023 & WO-025: Ambient Contrast Balancer & Sunlight Auto-Trigger */
		if (ipc_state.ambient_balancer && !disabled && !ipc_state.darkroom) {
			checks_result_t chk_light;
			if (checks_check_light(&chk_light) == 0) {
				if (chk_light.ambient_lux >= 3000) {
					/* High ambient daylight/sunlight: auto-engage sunlight glare boost */
					target_interp.sunlight_mode = 1;
					target_interp.temperature = 7500;
					target_interp.brightness = 1.00f;
				} else if (chk_light.ambient_lux >= 0) {
					double lux = fmax(1.0, (double)chk_light.ambient_lux);
					float amb_scale = 0.40f + 0.60f * (float)(log10(lux) / 3.0);
					if (amb_scale < 0.40f) amb_scale = 0.40f;
					if (amb_scale > 1.00f) amb_scale = 1.00f;
					target_interp.brightness *= amb_scale;
				} else if (chk_light.backlight_percent >= 0) {
					float amb_scale = 0.35f + 0.65f * ((float)chk_light.backlight_percent / 100.0f);
					target_interp.brightness *= amb_scale;
				}
			}
		}

		/* 20-20-20 subtle screen breathe cue */
		if (ipc_state.pacer_breathe > 0 && !disabled && !ipc_state.darkroom) {
			target_interp.brightness *= 0.85f;
			ipc_state.pacer_breathe--;
		}

		/* Cumulative Telemetry: Track active exposure & filtered blue photon energy */
		static time_t last_telemetry_time = 0;
		if (!disabled) {
			time_t delta_sec = (last_telemetry_time > 0) ? (now_epoch - last_telemetry_time) : 1;
			if (delta_sec > 0 && delta_sec <= 60) {
				ipc_state.total_active_seconds += (uint64_t)delta_sec;
				if (target_interp.temperature <= 3400) {
					ipc_state.restorative_seconds += (uint64_t)delta_sec;
				}
				double b_frac = ((double)target_interp.temperature / 6500.0) * target_interp.brightness;
				if (b_frac > 1.0) b_frac = 1.0;
				if (target_interp.darkroom) b_frac = 0.0;
				if (target_interp.reading_mode) b_frac *= 0.82;
				if (target_interp.melanopic_notch) b_frac *= 0.65;
				double reduction = 1.0 - b_frac;
				if (reduction > 0.0) {
					ipc_state.hev_joules_saved += reduction * (double)delta_sec;
				}
			}
		}
		last_telemetry_time = now_epoch;
#endif

		if (disabled) {
			period = PERIOD_NONE;
			color_setting_reset(&target_interp);
		}

		if (done) {
			period = PERIOD_NONE;
		}

		/* Print period if it changed during this update,
		   or if we are in the transition period. In transition we
		   print the progress, so we always print it in
		   that case. */
		if (verbose && (period != prev_period ||
				period == PERIOD_TRANSITION)) {
			print_period(period, transition_prog);
		}

		/* Activate hooks if period changed */
		if (period != prev_period) {
			hooks_signal_period_change(prev_period, period);
#ifndef _WIN32
			if (prev_period != PERIOD_NONE && !disabled) {
				const char *p_name = "Daytime";
				if (period == PERIOD_NIGHT) p_name = "Night";
				else if (period == PERIOD_TRANSITION) p_name = "Transition";
				char notify_msg[128];
				snprintf(notify_msg, sizeof(notify_msg),
					 "Color temperature adjusting for %s (%uK).",
					 p_name, target_interp.temperature);
				send_desktop_notification("Jarheart Circadian Shift", notify_msg);
			}
#endif
		}

		/* Start fade if the parameter differences are too big to apply
		   instantly. */
		if (use_fade) {
			if ((fade_length == 0 &&
			     color_setting_diff_is_major(
				     &interp,
				     &target_interp)) ||
			    (fade_length != 0 &&
			     color_setting_diff_is_major(
				     &target_interp,
				     &prev_target_interp))) {
				fade_length = FADE_LENGTH;
				fade_time = 0;
				fade_start_interp = interp;
			}
		}

		/* Handle ongoing fade */
		if (fade_length != 0) {
			fade_time += 1;
			double frac = fade_time / (double)fade_length;
			double alpha = CLAMP(0.0, ease_fade(frac), 1.0);

			interpolate_color_settings(
				&fade_start_interp, &target_interp, alpha,
				&interp);

			if (fade_time > fade_length) {
				fade_time = 0;
				fade_length = 0;
			}
		} else {
			interp = target_interp;
		}

		/* Break loop when done and final fade is over */
		if (done && fade_length == 0) break;

		if (verbose) {
			if (prev_target_interp.temperature !=
			    target_interp.temperature) {
				printf(_("Color temperature: %uK\n"),
				       target_interp.temperature);
			}
			if (prev_target_interp.brightness !=
			    target_interp.brightness) {
				printf(_("Brightness: %.2f\n"),
				       target_interp.brightness);
			}
		}

#ifndef _WIN32
		ipc_state.disabled = disabled;
		ipc_state.period = period;
		ipc_state.transition_prog = transition_prog;
		ipc_state.current_setting = interp;
		ipc_state.location = loc;
		ipc_state.method_name = method->name;
#endif

		/* Adjust temperature */
		r = method->set_temperature(
			method_state, &interp, preserve_gamma);
		if (r < 0) {
			fputs(_("Temperature adjustment failed.\n"),
			      stderr);
			return -1;
		}

		/* Save period and target color setting as previous */
		prev_period = period;
		prev_target_interp = target_interp;

		/* Sleep length depends on whether a fade is ongoing. */
		int delay = SLEEP_DURATION;
		if (fade_length != 0) {
			delay = SLEEP_DURATION_SHORT;
		}

		/* Update location and check IPC events */
		int loc_fd = -1;
		if (need_location) {
			loc_fd = provider->get_fd(location_state);
		}

		int gamma_fd = -1;
		if (method->get_fd) {
			gamma_fd = method->get_fd(method_state);
		}

		struct pollfd pollfds[3];
		int nfds = 0;
		int loc_idx = -1;
		int ipc_idx = -1;
		int gamma_idx = -1;

		if (loc_fd >= 0) {
			loc_idx = nfds;
			pollfds[nfds].fd = loc_fd;
			pollfds[nfds].events = POLLIN;
			pollfds[nfds].revents = 0;
			nfds++;
		}

#ifndef _WIN32
		if (ipc_active) {
			ipc_idx = nfds;
			pollfds[nfds].fd = ipc_get_fd(&ipc);
			pollfds[nfds].events = POLLIN;
			pollfds[nfds].revents = 0;
			nfds++;
		}
#endif

		if (gamma_fd >= 0) {
			gamma_idx = nfds;
			pollfds[nfds].fd = gamma_fd;
			pollfds[nfds].events = POLLIN;
			pollfds[nfds].revents = 0;
			nfds++;
		}

		if (nfds > 0) {
			int r = poll(pollfds, nfds, delay);
			if (r < 0) {
				if (errno == EINTR) continue;
				perror("poll");
				fputs(_("Unable to poll descriptors.\n"), stderr);
				return -1;
			} else if (r > 0) {
#ifndef _WIN32
				if (ipc_idx >= 0 && (pollfds[ipc_idx].revents & POLLIN)) {
					ipc_handle_connection(&ipc, &ipc_state);
					if (ipc_state.state_changed) {
						disabled = ipc_state.disabled;
						ipc_state.state_changed = 0;
					}
					if (ipc_state.requested_exit) {
						done = 1;
						disabled = 1;
					}
				}
#endif

				if (gamma_idx >= 0 && (pollfds[gamma_idx].revents & POLLIN)) {
					if (method->handle) {
						int reconfigured = method->handle(method_state);
						if (reconfigured > 0) {
							/* Display reconfigured (hotplug, mode change, wake) - reapply gamma */
							method->set_temperature(method_state, &interp, preserve_gamma);
						}
					}
				}

				if (loc_idx >= 0 && (pollfds[loc_idx].revents & POLLIN)) {
					/* Get new location and availability information. */
					location_t new_loc;
					int new_available;
					r = provider->handle(
						location_state, &new_loc,
						&new_available);
					if (r < 0) {
						fputs(_("Unable to get location"
							" from provider.\n"), stderr);
						return -1;
					}

					if (!new_available &&
					    new_available != location_available) {
						fputs(_("Location is temporarily"
						        " unavailable; Using previous"
							" location until it becomes"
							" available...\n"), stderr);
					}

					if (new_available &&
					    (new_loc.lat != loc.lat ||
					     new_loc.lon != loc.lon ||
					     new_available != location_available)) {
						loc = new_loc;
						print_location(&loc);
					}

					location_available = new_available;

					if (!location_is_valid(&loc)) {
						fputs(_("Invalid location returned"
							" from provider.\n"), stderr);
						return -1;
					}
				}
			}
		} else {
			systemtime_msleep(delay);
		}
	}

#ifndef _WIN32
	if (ipc_active) {
		ipc_cleanup(&ipc);
	}
#endif

	/* Restore saved gamma ramps */
	method->restore(method_state);

	return 0;
}


int
main(int argc, char *argv[])
{
	int r;

#ifdef ENABLE_NLS
	/* Init locale */
	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");

	/* Internationalisation */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	/* List of gamma methods. */
	const gamma_method_t gamma_methods[] = {
#ifdef ENABLE_WAYLAND
		wayland_gamma_method,
#endif
#ifdef ENABLE_DRM
		drm_gamma_method,
#endif
#ifdef ENABLE_RANDR
		randr_gamma_method,
#endif
#ifdef ENABLE_VIDMODE
		vidmode_gamma_method,
#endif
#ifdef ENABLE_QUARTZ
		quartz_gamma_method,
#endif
#ifdef ENABLE_WINGDI
		w32gdi_gamma_method,
#endif
		dummy_gamma_method,
		{ NULL }
	};

	/* List of location providers. */
	const location_provider_t location_providers[] = {
#ifdef ENABLE_GEOCLUE2
		geoclue2_location_provider,
#endif
#ifdef ENABLE_CORELOCATION
		corelocation_location_provider,
#endif
		timezone_location_provider,
		manual_location_provider,
		{ NULL }
	};

	/* Flush messages consistently even if redirected to a pipe or
	   file.  Change the flush behaviour to line-buffered, without
	   changing the actual buffers being used. */
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);

#ifndef _WIN32
	int ipc_cli_res = ipc_client_dispatch(argc, argv);
	if (ipc_cli_res >= 0) {
		exit(ipc_cli_res);
	}
#endif

	options_t options;
	options_init(&options);
	options_parse_args(
		&options, argc, argv, gamma_methods, location_providers);

	/* Load settings from config file. */
	config_ini_state_t config_state;
	r = config_ini_init(&config_state, options.config_filepath);
	if (r < 0) {
		fputs("Unable to load config file.\n", stderr);
		exit(EXIT_FAILURE);
	}

	free(options.config_filepath);

	options_parse_config_file(
		&options, &config_state, gamma_methods, location_providers);

	options_set_defaults(&options);

	if (options.scheme.dawn.start >= 0 || options.scheme.dawn.end >= 0 ||
	    options.scheme.dusk.start >= 0 || options.scheme.dusk.end >= 0) {
		if (options.scheme.dawn.start < 0 ||
		    options.scheme.dawn.end < 0 ||
		    options.scheme.dusk.start < 0 ||
		    options.scheme.dusk.end < 0) {
			fputs(_("Partitial time-configuration not"
				" supported!\n"), stderr);
			exit(EXIT_FAILURE);
		}

		if (options.scheme.dawn.start > options.scheme.dawn.end ||
		    options.scheme.dawn.end > options.scheme.dusk.start ||
		    options.scheme.dusk.start > options.scheme.dusk.end) {
			fputs(_("Invalid dawn/dusk time configuration!\n"),
			      stderr);
			exit(EXIT_FAILURE);
		}

		options.scheme.use_time = 1;
	}

	/* Initialize location provider if needed. If provider is NULL
	   try all providers until one that works is found. */
	location_state_t *location_state;

	/* Location is not needed for reset mode and manual mode. */
	int need_location =
		options.mode != PROGRAM_MODE_RESET &&
		options.mode != PROGRAM_MODE_MANUAL &&
		!options.scheme.use_time;
	if (need_location) {
		if (options.provider != NULL) {
			/* Use provider specified on command line. */
			r = provider_try_start(
				options.provider, &location_state,
				&config_state, options.provider_args);
			if (r < 0) exit(EXIT_FAILURE);
		} else {
			/* Try all providers, use the first that works. */
			for (int i = 0;
			     location_providers[i].name != NULL; i++) {
				const location_provider_t *p =
					&location_providers[i];
				fprintf(stderr,
					_("Trying location provider `%s'...\n"),
					p->name);
				r = provider_try_start(p, &location_state,
						       &config_state, NULL);
				if (r < 0) {
					fputs(_("Trying next provider...\n"),
					      stderr);
					continue;
				}

				/* Found provider that works. */
				printf(_("Using provider `%s'.\n"), p->name);
				options.provider = p;
				break;
			}

			/* Failure if no providers were successful at this
			   point. */
			if (options.provider == NULL) {
				fputs(_("No more location providers"
					" to try.\n"), stderr);
				exit(EXIT_FAILURE);
			}
		}

		/* Solar elevations */
		if (options.scheme.high < options.scheme.low) {
			fprintf(stderr,
				_("High transition elevation cannot be lower than"
				  " the low transition elevation.\n"));
			exit(EXIT_FAILURE);
		}

		if (options.verbose) {
			/* TRANSLATORS: Append degree symbols if possible. */
			printf(_("Solar elevations: day above %.1f, night below %.1f\n"),
			       options.scheme.high, options.scheme.low);
		}
	}

	if (options.mode != PROGRAM_MODE_RESET &&
	    options.mode != PROGRAM_MODE_MANUAL) {
		if (options.verbose) {
			printf(_("Temperatures: %dK at day, %dK at night\n"),
			       options.scheme.day.temperature,
			       options.scheme.night.temperature);
		}

		/* Color temperature */
		if (options.scheme.day.temperature < MIN_TEMP ||
		    options.scheme.day.temperature > MAX_TEMP ||
		    options.scheme.night.temperature < MIN_TEMP ||
		    options.scheme.night.temperature > MAX_TEMP) {
			fprintf(stderr,
				_("Temperature must be between %uK and %uK.\n"),
				MIN_TEMP, MAX_TEMP);
			exit(EXIT_FAILURE);
		}
	}

	if (options.mode == PROGRAM_MODE_MANUAL) {
		/* Check color temperature to be set */
		if (options.temp_set < MIN_TEMP ||
		    options.temp_set > MAX_TEMP) {
			fprintf(stderr,
				_("Temperature must be between %uK and %uK.\n"),
				MIN_TEMP, MAX_TEMP);
			exit(EXIT_FAILURE);
		}
	}

	/* Brightness */
	if (options.scheme.day.brightness < MIN_BRIGHTNESS ||
	    options.scheme.day.brightness > MAX_BRIGHTNESS ||
	    options.scheme.night.brightness < MIN_BRIGHTNESS ||
	    options.scheme.night.brightness > MAX_BRIGHTNESS) {
		fprintf(stderr,
			_("Brightness values must be between %.1f and %.1f.\n"),
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		exit(EXIT_FAILURE);
	}

	if (options.verbose) {
		printf(_("Brightness: %.2f:%.2f\n"),
		       options.scheme.day.brightness,
		       options.scheme.night.brightness);
	}

	/* Gamma */
	if (!gamma_is_valid(options.scheme.day.gamma) ||
	    !gamma_is_valid(options.scheme.night.gamma)) {
		fprintf(stderr,
			_("Gamma value must be between %.1f and %.1f.\n"),
			MIN_GAMMA, MAX_GAMMA);
		exit(EXIT_FAILURE);
	}

	if (options.verbose) {
		/* TRANSLATORS: The string in parenthesis is either
		   Daytime or Night (translated). */
		printf(_("Gamma (%s): %.3f, %.3f, %.3f\n"),
		       _("Daytime"), options.scheme.day.gamma[0],
		       options.scheme.day.gamma[1],
		       options.scheme.day.gamma[2]);
		printf(_("Gamma (%s): %.3f, %.3f, %.3f\n"),
		       _("Night"), options.scheme.night.gamma[0],
		       options.scheme.night.gamma[1],
		       options.scheme.night.gamma[2]);
	}

	transition_scheme_t *scheme = &options.scheme;

	/* Initialize gamma adjustment method. If method is NULL
	   try all methods until one that works is found. */
	gamma_state_t *method_state;

	/* Gamma adjustment not needed for print mode */
	if (options.mode != PROGRAM_MODE_PRINT) {
		if (options.method != NULL) {
			/* Use method specified on command line. */
			r = method_try_start(
				options.method, &method_state, &config_state,
				options.method_args);
			if (r < 0) exit(EXIT_FAILURE);
		} else {
			/* Try all methods, use the first that works. */
			for (int i = 0; gamma_methods[i].name != NULL; i++) {
				const gamma_method_t *m = &gamma_methods[i];
				if (!m->autostart) continue;

				r = method_try_start(
					m, &method_state, &config_state, NULL);
				if (r < 0) {
					fputs(_("Trying next method...\n"), stderr);
					continue;
				}

				/* Found method that works. */
				printf(_("Using method `%s'.\n"), m->name);
				options.method = m;
				break;
			}

			/* Failure if no methods were successful at this point. */
			if (options.method == NULL) {
				fputs(_("No more methods to try.\n"), stderr);
				exit(EXIT_FAILURE);
			}
		}
	}

	config_ini_free(&config_state);

	switch (options.mode) {
	case PROGRAM_MODE_ONE_SHOT:
	case PROGRAM_MODE_PRINT:
	{
		location_t loc = { NAN, NAN };
		if (need_location) {
			fputs(_("Waiting for current location"
				" to become available...\n"), stderr);

			/* Wait for location provider. Use a 4s timeout for geoclue2 to prevent hanging. */
			int init_timeout = (strcmp(options.provider->name, "geoclue2") == 0) ? 4000 : -1;
			int r = provider_get_location(
				options.provider, location_state, init_timeout, &loc);
			if (r <= 0) {
				if (strcmp(options.provider->name, "geoclue2") == 0) {
					char tz_name[128] = {0};
					if (location_timezone_resolve(&loc, tz_name, sizeof(tz_name)) == 0) {
						fprintf(stderr, _("GeoClue2 unavailable (no WiFi/GPS). Automatically resolved location from system timezone (%s): %.2f N, %.2f W\n"),
						        tz_name, loc.lat, -loc.lon);
						r = 1;
					}
				}
			}
			if (r <= 0) {
				fputs(_("Unable to get location"
					" from provider.\n"), stderr);
				exit(EXIT_FAILURE);
			}

			if (!location_is_valid(&loc)) {
				exit(EXIT_FAILURE);
			}

			print_location(&loc);
		}

		double now;
		r = systemtime_get_time(&now);
		if (r < 0) {
			fputs(_("Unable to read system time.\n"), stderr);
			options.method->free(method_state);
			exit(EXIT_FAILURE);
		}

		period_t period;
		double transition_prog;
		if (options.scheme.use_time) {
			int time_offset = get_seconds_since_midnight(now);
			period = get_period_from_time(scheme, time_offset);
			transition_prog = get_transition_progress_from_time(
				scheme, time_offset);
		} else {
			/* Current angular elevation of the sun */
			double elevation = solar_elevation(
				now, loc.lat, loc.lon);
			if (options.verbose) {
				/* TRANSLATORS: Append degree symbol if
				   possible. */
				printf(_("Solar elevation: %f\n"), elevation);
			}

			period = get_period_from_elevation(scheme, elevation);
			transition_prog =
				get_transition_progress_from_elevation(
					scheme, elevation);
		}

		/* Use transition progress to set color temperature */
		color_setting_t interp;
		interpolate_transition_scheme(
			scheme, transition_prog, &interp);

		if (options.verbose || options.mode == PROGRAM_MODE_PRINT) {
			print_period(period, transition_prog);
			printf(_("Color temperature: %uK\n"),
			       interp.temperature);
			printf(_("Brightness: %.2f\n"),
			       interp.brightness);
		}

		if (options.mode != PROGRAM_MODE_PRINT) {
			/* Adjust temperature */
			r = options.method->set_temperature(
				method_state, &interp, options.preserve_gamma);
			if (r < 0) {
				fputs(_("Temperature adjustment failed.\n"),
				      stderr);
				options.method->free(method_state);
				exit(EXIT_FAILURE);
			}

			/* In Quartz (macOS) the gamma adjustments will
			   automatically revert when the process exits.
			   Therefore, we have to loop until CTRL-C is received.
			   */
			if (strcmp(options.method->name, "quartz") == 0) {
				fputs(_("Press ctrl-c to stop...\n"), stderr);
				pause();
			}
		}
	}
	break;
	case PROGRAM_MODE_MANUAL:
	{
		if (options.verbose) {
			printf(_("Color temperature: %uK\n"),
			       options.temp_set);
		}

		/* Adjust temperature */
		color_setting_t manual = scheme->day;
		manual.temperature = options.temp_set;
		r = options.method->set_temperature(
			method_state, &manual, options.preserve_gamma);
		if (r < 0) {
			fputs(_("Temperature adjustment failed.\n"), stderr);
			options.method->free(method_state);
			exit(EXIT_FAILURE);
		}

		/* In Quartz (OSX) the gamma adjustments will automatically
		   revert when the process exits. Therefore, we have to loop
		   until CTRL-C is received. */
		if (strcmp(options.method->name, "quartz") == 0) {
			fputs(_("Press ctrl-c to stop...\n"), stderr);
			pause();
		}
	}
	break;
	case PROGRAM_MODE_RESET:
	{
		/* Reset screen */
		color_setting_t reset;
		color_setting_reset(&reset);

		r = options.method->set_temperature(method_state, &reset, 0);
		if (r < 0) {
			fputs(_("Temperature adjustment failed.\n"), stderr);
			options.method->free(method_state);
			exit(EXIT_FAILURE);
		}

		/* In Quartz (OSX) the gamma adjustments will automatically
		   revert when the process exits. Therefore, we have to loop
		   until CTRL-C is received. */
		if (strcmp(options.method->name, "quartz") == 0) {
			fputs(_("Press ctrl-c to stop...\n"), stderr);
			pause();
		}
	}
	break;
	case PROGRAM_MODE_CONTINUAL:
	{
		r = run_continual_mode(
			options.provider, location_state, scheme,
			options.method, method_state,
			options.use_fade, options.preserve_gamma,
			options.verbose);
		if (r < 0) exit(EXIT_FAILURE);
	}
	break;
	}

	/* Clean up gamma adjustment state */
	if (options.mode != PROGRAM_MODE_PRINT) {
		options.method->free(method_state);
	}

	/* Clean up location provider state */
	if (need_location) {
		options.provider->free(location_state);
	}

	return EXIT_SUCCESS;
}
