/* gamma-randr.c -- X RANDR gamma adjustment source
   This file is part of Jarheart / Redshift.

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

   Copyright (c) 2010-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) s
#endif

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "gamma-randr.h"
#include "redshift.h"
#include "colorramp.h"

#define RANDR_VERSION_MAJOR  1
#define RANDR_VERSION_MINOR  3

typedef struct {
	RRCrtc crtc;
	int ramp_size;
	uint16_t *saved_r;
	uint16_t *saved_g;
	uint16_t *saved_b;
} randr_crtc_state_t;

typedef struct {
	Display *dpy;
	int screen_num;
	int crtc_num_count;
	int *crtc_num;
	int crtc_count;
	randr_crtc_state_t *crtcs;
	int event_base;
	Window root;
} randr_state_t;


static int
randr_init(randr_state_t **state)
{
	*state = calloc(1, sizeof(randr_state_t));
	if (*state == NULL) return -1;

	randr_state_t *s = *state;
	s->screen_num = -1;
	s->crtc_num = NULL;
	s->crtc_num_count = 0;
	s->crtc_count = 0;
	s->crtcs = NULL;

	/* Open X server connection */
	s->dpy = XOpenDisplay(NULL);
	if (s->dpy == NULL) {
		free(s);
		*state = NULL;
		return -1;
	}

	/* Query RandR version */
	int major = 0, minor = 0;
	if (!XRRQueryVersion(s->dpy, &major, &minor)) {
		fprintf(stderr, _("XRandR extension not supported by display `%s'.\n"),
			XDisplayName(NULL));
		XCloseDisplay(s->dpy);
		free(s);
		*state = NULL;
		return -1;
	}

	if (major < RANDR_VERSION_MAJOR ||
	    (major == RANDR_VERSION_MAJOR && minor < RANDR_VERSION_MINOR)) {
		fprintf(stderr, _("Unsupported RANDR version (%d.%d)\n"),
			major, minor);
		XCloseDisplay(s->dpy);
		free(s);
		*state = NULL;
		return -1;
	}

	return 0;
}

static int
randr_start(randr_state_t *state)
{
	int screen_num = (state->screen_num >= 0) ?
		state->screen_num : DefaultScreen(state->dpy);

	if (screen_num >= ScreenCount(state->dpy)) {
		fprintf(stderr, _("Screen %d does not exist.\n"), screen_num);
		return -1;
	}

	Window root = RootWindow(state->dpy, screen_num);
	XRRScreenResources *res = XRRGetScreenResourcesCurrent(state->dpy, root);
	if (res == NULL) {
		fprintf(stderr, _("Could not get screen resources for screen %d.\n"),
			screen_num);
		return -1;
	}

	state->crtc_count = res->ncrtc;
	state->crtcs = calloc(res->ncrtc, sizeof(randr_crtc_state_t));
	if (state->crtcs == NULL) {
		XRRFreeScreenResources(res);
		return -1;
	}

	/* Inspect and save current gamma for all active CRTCs */
	for (int i = 0; i < res->ncrtc; i++) {
		RRCrtc crtc = res->crtcs[i];
		state->crtcs[i].crtc = crtc;
		state->crtcs[i].ramp_size = 0;
		state->crtcs[i].saved_r = NULL;
		state->crtcs[i].saved_g = NULL;
		state->crtcs[i].saved_b = NULL;

		XRRCrtcInfo *ci = XRRGetCrtcInfo(state->dpy, res, crtc);
		if (ci == NULL) continue;

		int is_active = (ci->mode != None && ci->noutput > 0);
		XRRFreeCrtcInfo(ci);
		if (!is_active) continue;

		int size = XRRGetCrtcGammaSize(state->dpy, crtc);
		if (size <= 0) continue;

		XRRCrtcGamma *g = XRRGetCrtcGamma(state->dpy, crtc);
		if (g == NULL) continue;

		state->crtcs[i].ramp_size = size;
		state->crtcs[i].saved_r = malloc(size * sizeof(uint16_t));
		state->crtcs[i].saved_g = malloc(size * sizeof(uint16_t));
		state->crtcs[i].saved_b = malloc(size * sizeof(uint16_t));

		if (state->crtcs[i].saved_r &&
		    state->crtcs[i].saved_g &&
		    state->crtcs[i].saved_b) {
			memcpy(state->crtcs[i].saved_r, g->red, size * sizeof(uint16_t));
			memcpy(state->crtcs[i].saved_g, g->green, size * sizeof(uint16_t));
			memcpy(state->crtcs[i].saved_b, g->blue, size * sizeof(uint16_t));
		}

		XRRFreeGamma(g);
	}

	XRRFreeScreenResources(res);

	state->root = root;
	int event_base = 0, error_base = 0;
	if (XRRQueryExtension(state->dpy, &event_base, &error_base)) {
		state->event_base = event_base;
		XRRSelectInput(state->dpy, root,
			       RRScreenChangeNotifyMask |
			       RRCrtcChangeNotifyMask |
			       RROutputChangeNotifyMask);
	}

	return 0;
}

static int
randr_refresh_crtcs(randr_state_t *state)
{
	if (state == NULL || state->dpy == NULL) return -1;

	XRRScreenResources *res = XRRGetScreenResourcesCurrent(state->dpy, state->root);
	if (res == NULL) return -1;

	randr_crtc_state_t *new_crtcs = calloc(res->ncrtc, sizeof(randr_crtc_state_t));
	if (new_crtcs == NULL) {
		XRRFreeScreenResources(res);
		return -1;
	}

	for (int i = 0; i < res->ncrtc; i++) {
		RRCrtc crtc = res->crtcs[i];
		new_crtcs[i].crtc = crtc;
		new_crtcs[i].ramp_size = 0;
		new_crtcs[i].saved_r = NULL;
		new_crtcs[i].saved_g = NULL;
		new_crtcs[i].saved_b = NULL;

		XRRCrtcInfo *ci = XRRGetCrtcInfo(state->dpy, res, crtc);
		if (ci == NULL) continue;
		int is_active = (ci->mode != None && ci->noutput > 0);
		XRRFreeCrtcInfo(ci);
		if (!is_active) continue;

		int size = XRRGetCrtcGammaSize(state->dpy, crtc);
		if (size <= 0) continue;

		int found = 0;
		for (int j = 0; j < state->crtc_count; j++) {
			if (state->crtcs && state->crtcs[j].crtc == crtc &&
			    state->crtcs[j].saved_r != NULL && state->crtcs[j].ramp_size == size) {
				new_crtcs[i].ramp_size = size;
				new_crtcs[i].saved_r = state->crtcs[j].saved_r;
				new_crtcs[i].saved_g = state->crtcs[j].saved_g;
				new_crtcs[i].saved_b = state->crtcs[j].saved_b;
				state->crtcs[j].saved_r = NULL;
				state->crtcs[j].saved_g = NULL;
				state->crtcs[j].saved_b = NULL;
				found = 1;
				break;
			}
		}

		if (!found) {
			XRRCrtcGamma *g = XRRGetCrtcGamma(state->dpy, crtc);
			if (g != NULL) {
				new_crtcs[i].ramp_size = size;
				new_crtcs[i].saved_r = malloc(size * sizeof(uint16_t));
				new_crtcs[i].saved_g = malloc(size * sizeof(uint16_t));
				new_crtcs[i].saved_b = malloc(size * sizeof(uint16_t));
				if (new_crtcs[i].saved_r && new_crtcs[i].saved_g && new_crtcs[i].saved_b) {
					memcpy(new_crtcs[i].saved_r, g->red, size * sizeof(uint16_t));
					memcpy(new_crtcs[i].saved_g, g->green, size * sizeof(uint16_t));
					memcpy(new_crtcs[i].saved_b, g->blue, size * sizeof(uint16_t));
				}
				XRRFreeGamma(g);
			}
		}
	}

	if (state->crtcs != NULL) {
		for (int j = 0; j < state->crtc_count; j++) {
			free(state->crtcs[j].saved_r);
			free(state->crtcs[j].saved_g);
			free(state->crtcs[j].saved_b);
		}
		free(state->crtcs);
	}

	state->crtcs = new_crtcs;
	state->crtc_count = res->ncrtc;
	XRRFreeScreenResources(res);
	return 0;
}

static void
randr_restore(randr_state_t *state)
{
	if (state == NULL || state->dpy == NULL || state->crtcs == NULL) return;

	for (int i = 0; i < state->crtc_count; i++) {
		if (state->crtcs[i].ramp_size <= 0 || state->crtcs[i].saved_r == NULL) {
			continue;
		}

		int size = state->crtcs[i].ramp_size;
		XRRCrtcGamma *gamma = XRRAllocGamma(size);
		if (gamma == NULL) continue;

		memcpy(gamma->red, state->crtcs[i].saved_r, size * sizeof(uint16_t));
		memcpy(gamma->green, state->crtcs[i].saved_g, size * sizeof(uint16_t));
		memcpy(gamma->blue, state->crtcs[i].saved_b, size * sizeof(uint16_t));

		XRRSetCrtcGamma(state->dpy, state->crtcs[i].crtc, gamma);
		XRRFreeGamma(gamma);
	}

	XFlush(state->dpy);
}

static void
randr_free(randr_state_t *state)
{
	if (state == NULL) return;

	randr_restore(state);

	if (state->crtcs != NULL) {
		for (int i = 0; i < state->crtc_count; i++) {
			free(state->crtcs[i].saved_r);
			free(state->crtcs[i].saved_g);
			free(state->crtcs[i].saved_b);
		}
		free(state->crtcs);
	}

	free(state->crtc_num);

	if (state->dpy != NULL) {
		XCloseDisplay(state->dpy);
	}

	free(state);
}

static void
randr_print_help(FILE *f)
{
	fputs(_("Adjust gamma ramps with the X RANDR extension.\n"), f);
	fputs("\n", f);
	fputs(_("  screen=N\t\tX screen number\n"), f);
	fputs(_("  crtc=N\t\tX CRTC number to adjust (comma separated list)\n"), f);
	fputs("\n", f);
}

static int
randr_set_option(randr_state_t *state, const char *key, const char *value)
{
	if (strcasecmp(key, "screen") == 0) {
		char *tail;
		errno = 0;
		int parsed = strtol(value, &tail, 0);
		if (parsed == 0 && (errno != 0 || tail == value)) {
			return -1;
		}
		state->screen_num = parsed;
	} else if (strcasecmp(key, "crtc") == 0) {
		const char *local_value = value;
		char *tail;

		state->crtc_num_count = 0;
		while (1) {
			errno = 0;
			int parsed = strtol(local_value, &tail, 0);
			if (parsed == 0 && (errno != 0 || tail == local_value)) {
				return -1;
			}
			state->crtc_num_count += 1;
			local_value = tail;

			if (*local_value == ',') {
				local_value += 1;
			} else if (*local_value == '\0') {
				break;
			}
		}

		free(state->crtc_num);
		state->crtc_num = calloc(state->crtc_num_count, sizeof(int));
		if (state->crtc_num == NULL) return -1;

		local_value = value;
		for (int i = 0; i < state->crtc_num_count; i++) {
			errno = 0;
			int parsed = strtol(local_value, &tail, 0);
			if (parsed == 0 && (errno != 0 || tail == local_value)) {
				return -1;
			}
			state->crtc_num[i] = parsed;
			local_value = tail;

			if (*local_value == ',') {
				local_value += 1;
			} else if (*local_value == '\0') {
				break;
			}
		}
	} else if (strcasecmp(key, "preserve") == 0) {
		/* Kept for backward compatibility */
	} else {
		fprintf(stderr, _("Unknown method parameter: `%s'.\n"), key);
		return -1;
	}

	return 0;
}

static int
randr_set_temperature_for_crtc(
	randr_state_t *state, int crtc_num, const color_setting_t *setting,
	int preserve)
{
	if (crtc_num >= state->crtc_count || crtc_num < 0) {
		return -1;
	}

	int ramp_size = state->crtcs[crtc_num].ramp_size;
	if (ramp_size <= 0) {
		/* Inactive CRTC */
		return 0;
	}

	XRRCrtcGamma *gamma = XRRAllocGamma(ramp_size);
	if (gamma == NULL) {
		perror("XRRAllocGamma");
		return -1;
	}

	if (preserve && state->crtcs[crtc_num].saved_r != NULL) {
		memcpy(gamma->red, state->crtcs[crtc_num].saved_r,
		       ramp_size * sizeof(uint16_t));
		memcpy(gamma->green, state->crtcs[crtc_num].saved_g,
		       ramp_size * sizeof(uint16_t));
		memcpy(gamma->blue, state->crtcs[crtc_num].saved_b,
		       ramp_size * sizeof(uint16_t));
	} else {
		for (int i = 0; i < ramp_size; i++) {
			uint16_t value = (uint16_t)(((double)i / (ramp_size - 1)) * UINT16_MAX);
			gamma->red[i] = value;
			gamma->green[i] = value;
			gamma->blue[i] = value;
		}
	}

	colorramp_fill(gamma->red, gamma->green, gamma->blue, ramp_size, setting);

	XRRSetCrtcGamma(state->dpy, state->crtcs[crtc_num].crtc, gamma);
	XRRFreeGamma(gamma);

	return 0;
}

static int
randr_set_temperature(
	randr_state_t *state, const color_setting_t *setting, int preserve)
{
	if (state->crtc_num_count == 0) {
		for (int i = 0; i < state->crtc_count; i++) {
			int r = randr_set_temperature_for_crtc(
				state, i, setting, preserve);
			if (r < 0) return -1;
		}
	} else {
		for (int i = 0; i < state->crtc_num_count; i++) {
			int r = randr_set_temperature_for_crtc(
				state, state->crtc_num[i], setting, preserve);
			if (r < 0) return -1;
		}
	}

	XFlush(state->dpy);
	return 0;
}

static int
randr_get_fd(randr_state_t *state)
{
	return (state && state->dpy) ? ConnectionNumber(state->dpy) : -1;
}

static int
randr_handle(randr_state_t *state)
{
	if (state == NULL || state->dpy == NULL) return 0;

	int reconfigured = 0;
	while (XPending(state->dpy) > 0) {
		XEvent ev;
		XNextEvent(state->dpy, &ev);
		if (state->event_base > 0) {
			if (ev.type == state->event_base + RRScreenChangeNotify) {
				XRRUpdateConfiguration(&ev);
				reconfigured = 1;
			} else if (ev.type == state->event_base + RRNotify) {
				reconfigured = 1;
			}
		}
	}

	if (reconfigured) {
		randr_refresh_crtcs(state);
		return 1;
	}

	return 0;
}

const gamma_method_t randr_gamma_method = {
	"randr", 1,
	(gamma_method_init_func *)randr_init,
	(gamma_method_start_func *)randr_start,
	(gamma_method_free_func *)randr_free,
	(gamma_method_print_help_func *)randr_print_help,
	(gamma_method_set_option_func *)randr_set_option,
	(gamma_method_restore_func *)randr_restore,
	(gamma_method_set_temperature_func *)randr_set_temperature,
	(gamma_method_get_fd_func *)randr_get_fd,
	(gamma_method_handle_func *)randr_handle
};
