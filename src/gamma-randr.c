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

#include <math.h>

#include "gamma-randr.h"
#include "redshift.h"
#include "colorramp.h"

#define RANDR_VERSION_MAJOR  1
#define RANDR_VERSION_MINOR  3

typedef struct {
	RRCrtc crtc;
	char name[32];
	int active;
	int enabled;           /* 1 = Jarheart adjustments applied, 0 = bypassed (linear identity 6500K) */
	float brightness_mult; /* per-CRTC brightness multiplier (default 1.0f) */
	float gamma_mult[3];   /* per-CRTC R, G, B calibration multipliers (default 1.0f) */
	int temp_offset;       /* per-CRTC Kelvin offset (default 0) */
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


static randr_state_t *g_randr_state = NULL;
static float g_pending_crtc_mults[8][3];
static int g_pending_crtc_enabled[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static float g_pending_crtc_brightness[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static int g_pending_crtc_temp_offset[8];
static int g_has_pending_crtc = 0;

int
randr_set_crtc_calibration(int crtc_num, float r, float g, float b)
{
	if (crtc_num < 0 || crtc_num >= 8) return -1;
	if (r < 0.1f) r = 0.1f;
	if (r > 2.0f) r = 2.0f;
	if (g < 0.1f) g = 0.1f;
	if (g > 2.0f) g = 2.0f;
	if (b < 0.1f) b = 0.1f;
	if (b > 2.0f) b = 2.0f;

	g_pending_crtc_mults[crtc_num][0] = r;
	g_pending_crtc_mults[crtc_num][1] = g;
	g_pending_crtc_mults[crtc_num][2] = b;
	g_has_pending_crtc = 1;

	if (g_randr_state != NULL && crtc_num < g_randr_state->crtc_count) {
		g_randr_state->crtcs[crtc_num].gamma_mult[0] = r;
		g_randr_state->crtcs[crtc_num].gamma_mult[1] = g;
		g_randr_state->crtcs[crtc_num].gamma_mult[2] = b;
	}
	return 0;
}

int
randr_set_crtc_enabled(int crtc_num, int enabled)
{
	if (crtc_num < 0 || crtc_num >= 8) return -1;
	g_pending_crtc_enabled[crtc_num] = enabled ? 1 : 0;
	g_has_pending_crtc = 1;

	if (g_randr_state != NULL && crtc_num < g_randr_state->crtc_count) {
		g_randr_state->crtcs[crtc_num].enabled = enabled ? 1 : 0;
	}
	return 0;
}

int
randr_set_crtc_brightness(int crtc_num, float brightness)
{
	if (crtc_num < 0 || crtc_num >= 8) return -1;
	if (brightness < 0.1f) brightness = 0.1f;
	if (brightness > 2.0f) brightness = 2.0f;

	g_pending_crtc_brightness[crtc_num] = brightness;
	g_has_pending_crtc = 1;

	if (g_randr_state != NULL && crtc_num < g_randr_state->crtc_count) {
		g_randr_state->crtcs[crtc_num].brightness_mult = brightness;
	}
	return 0;
}

int
randr_set_crtc_temp_offset(int crtc_num, int temp_offset)
{
	if (crtc_num < 0 || crtc_num >= 8) return -1;
	if (temp_offset < -5000) temp_offset = -5000;
	if (temp_offset > 5000) temp_offset = 5000;

	g_pending_crtc_temp_offset[crtc_num] = temp_offset;
	g_has_pending_crtc = 1;

	if (g_randr_state != NULL && crtc_num < g_randr_state->crtc_count) {
		g_randr_state->crtcs[crtc_num].temp_offset = temp_offset;
	}
	return 0;
}

int
randr_reset_crtc(int crtc_num)
{
	if (crtc_num < 0 || crtc_num >= 8) return -1;
	g_pending_crtc_mults[crtc_num][0] = 1.0f;
	g_pending_crtc_mults[crtc_num][1] = 1.0f;
	g_pending_crtc_mults[crtc_num][2] = 1.0f;
	g_pending_crtc_enabled[crtc_num] = 1;
	g_pending_crtc_brightness[crtc_num] = 1.0f;
	g_pending_crtc_temp_offset[crtc_num] = 0;

	if (g_randr_state != NULL && crtc_num < g_randr_state->crtc_count) {
		g_randr_state->crtcs[crtc_num].gamma_mult[0] = 1.0f;
		g_randr_state->crtcs[crtc_num].gamma_mult[1] = 1.0f;
		g_randr_state->crtcs[crtc_num].gamma_mult[2] = 1.0f;
		g_randr_state->crtcs[crtc_num].enabled = 1;
		g_randr_state->crtcs[crtc_num].brightness_mult = 1.0f;
		g_randr_state->crtcs[crtc_num].temp_offset = 0;
	}
	return 0;
}

int
randr_get_crtc_count(void)
{
	if (g_randr_state == NULL) return 0;
	return g_randr_state->crtc_count;
}

int
randr_get_crtc_info(int crtc_num, char *name_buf, size_t name_buf_size,
		    int *active, int *enabled, float *brightness,
		    float gamma_mult[3], int *temp_offset)
{
	if (g_randr_state == NULL || crtc_num < 0 || crtc_num >= g_randr_state->crtc_count) {
		return -1;
	}
	randr_crtc_state_t *c = &g_randr_state->crtcs[crtc_num];
	if (name_buf && name_buf_size > 0) {
		snprintf(name_buf, name_buf_size, "%s", c->name[0] ? c->name : "CRTC");
	}
	if (active) *active = c->active;
	if (enabled) *enabled = c->enabled;
	if (brightness) *brightness = c->brightness_mult;
	if (gamma_mult) {
		gamma_mult[0] = c->gamma_mult[0];
		gamma_mult[1] = c->gamma_mult[1];
		gamma_mult[2] = c->gamma_mult[2];
	}
	if (temp_offset) *temp_offset = c->temp_offset;
	return 0;
}

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

/* Check if a gamma ramp is noticeably warm/tinted or distorted,
   which indicates a prior redshift run or blue light filter was active. */
static int
randr_ramp_is_tinted(const XRRCrtcGamma *g, int size)
{
	if (g == NULL || size < 2) return 0;
	uint16_t max_r = g->red[size - 1];
	uint16_t max_g = g->green[size - 1];
	uint16_t max_b = g->blue[size - 1];

	if (max_r > 1000) {
		/* If blue is < 85% of red or green is < 70% of red at full white */
		if ((uint32_t)max_b * 100 < (uint32_t)max_r * 85 ||
		    (uint32_t)max_g * 100 < (uint32_t)max_r * 70) {
			return 1;
		}
	}
	return 0;
}

static void
randr_fill_linear(uint16_t *r, uint16_t *g, uint16_t *b, int size)
{
	for (int i = 0; i < size; i++) {
		uint16_t val = (uint16_t)(((double)i / (size > 1 ? (size - 1) : 1)) * UINT16_MAX);
		r[i] = val;
		g[i] = val;
		b[i] = val;
	}
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
	g_randr_state = state;

	/* Inspect and save current gamma for all active CRTCs */
	for (int i = 0; i < res->ncrtc; i++) {
		RRCrtc crtc = res->crtcs[i];
		state->crtcs[i].crtc = crtc;
		state->crtcs[i].ramp_size = 0;
		state->crtcs[i].saved_r = NULL;
		state->crtcs[i].saved_g = NULL;
		state->crtcs[i].saved_b = NULL;
		state->crtcs[i].gamma_mult[0] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][0] > 0.01f) ? g_pending_crtc_mults[i][0] : 1.0f;
		state->crtcs[i].gamma_mult[1] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][1] > 0.01f) ? g_pending_crtc_mults[i][1] : 1.0f;
		state->crtcs[i].gamma_mult[2] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][2] > 0.01f) ? g_pending_crtc_mults[i][2] : 1.0f;
		state->crtcs[i].enabled = (i < 8 && g_has_pending_crtc) ? g_pending_crtc_enabled[i] : 1;
		state->crtcs[i].brightness_mult = (i < 8 && g_has_pending_crtc && g_pending_crtc_brightness[i] > 0.05f) ? g_pending_crtc_brightness[i] : 1.0f;
		state->crtcs[i].temp_offset = (i < 8 && g_has_pending_crtc) ? g_pending_crtc_temp_offset[i] : 0;
		snprintf(state->crtcs[i].name, sizeof(state->crtcs[i].name), "CRTC-%d", i);

		XRRCrtcInfo *ci = XRRGetCrtcInfo(state->dpy, res, crtc);
		if (ci == NULL) continue;

		int is_active = (ci->mode != None && ci->noutput > 0);
		state->crtcs[i].active = is_active;
		if (ci->noutput > 0) {
			XRROutputInfo *oi = XRRGetOutputInfo(state->dpy, res, ci->outputs[0]);
			if (oi != NULL) {
				snprintf(state->crtcs[i].name, sizeof(state->crtcs[i].name), "%s", oi->name);
				XRRFreeOutputInfo(oi);
			}
		}
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
			if (randr_ramp_is_tinted(g, size)) {
				randr_fill_linear(state->crtcs[i].saved_r,
						  state->crtcs[i].saved_g,
						  state->crtcs[i].saved_b, size);
			} else {
				memcpy(state->crtcs[i].saved_r, g->red, size * sizeof(uint16_t));
				memcpy(state->crtcs[i].saved_g, g->green, size * sizeof(uint16_t));
				memcpy(state->crtcs[i].saved_b, g->blue, size * sizeof(uint16_t));
			}
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
		new_crtcs[i].active = is_active;
		snprintf(new_crtcs[i].name, sizeof(new_crtcs[i].name), "CRTC-%d", i);
		if (ci->noutput > 0) {
			XRROutputInfo *oi = XRRGetOutputInfo(state->dpy, res, ci->outputs[0]);
			if (oi != NULL) {
				snprintf(new_crtcs[i].name, sizeof(new_crtcs[i].name), "%s", oi->name);
				XRRFreeOutputInfo(oi);
			}
		}
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
				new_crtcs[i].gamma_mult[0] = state->crtcs[j].gamma_mult[0];
				new_crtcs[i].gamma_mult[1] = state->crtcs[j].gamma_mult[1];
				new_crtcs[i].gamma_mult[2] = state->crtcs[j].gamma_mult[2];
				new_crtcs[i].enabled = state->crtcs[j].enabled;
				new_crtcs[i].brightness_mult = state->crtcs[j].brightness_mult;
				new_crtcs[i].temp_offset = state->crtcs[j].temp_offset;
				found = 1;
				break;
			}
		}

		if (!found) {
			new_crtcs[i].gamma_mult[0] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][0] > 0.01f) ? g_pending_crtc_mults[i][0] : 1.0f;
			new_crtcs[i].gamma_mult[1] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][1] > 0.01f) ? g_pending_crtc_mults[i][1] : 1.0f;
			new_crtcs[i].gamma_mult[2] = (i < 8 && g_has_pending_crtc && g_pending_crtc_mults[i][2] > 0.01f) ? g_pending_crtc_mults[i][2] : 1.0f;
			new_crtcs[i].enabled = (i < 8 && g_has_pending_crtc) ? g_pending_crtc_enabled[i] : 1;
			new_crtcs[i].brightness_mult = (i < 8 && g_has_pending_crtc && g_pending_crtc_brightness[i] > 0.05f) ? g_pending_crtc_brightness[i] : 1.0f;
			new_crtcs[i].temp_offset = (i < 8 && g_has_pending_crtc) ? g_pending_crtc_temp_offset[i] : 0;
			XRRCrtcGamma *g = XRRGetCrtcGamma(state->dpy, crtc);
			if (g != NULL) {
				new_crtcs[i].ramp_size = size;
				new_crtcs[i].saved_r = malloc(size * sizeof(uint16_t));
				new_crtcs[i].saved_g = malloc(size * sizeof(uint16_t));
				new_crtcs[i].saved_b = malloc(size * sizeof(uint16_t));
				if (new_crtcs[i].saved_r && new_crtcs[i].saved_g && new_crtcs[i].saved_b) {
					if (randr_ramp_is_tinted(g, size)) {
						randr_fill_linear(new_crtcs[i].saved_r,
								  new_crtcs[i].saved_g,
								  new_crtcs[i].saved_b, size);
					} else {
						memcpy(new_crtcs[i].saved_r, g->red, size * sizeof(uint16_t));
						memcpy(new_crtcs[i].saved_g, g->green, size * sizeof(uint16_t));
						memcpy(new_crtcs[i].saved_b, g->blue, size * sizeof(uint16_t));
					}
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
		if (state->crtcs[i].ramp_size <= 0) {
			continue;
		}

		int size = state->crtcs[i].ramp_size;
		XRRCrtcGamma *gamma = XRRAllocGamma(size);
		if (gamma == NULL) continue;

		if (state->crtcs[i].saved_r != NULL) {
			memcpy(gamma->red, state->crtcs[i].saved_r, size * sizeof(uint16_t));
			memcpy(gamma->green, state->crtcs[i].saved_g, size * sizeof(uint16_t));
			memcpy(gamma->blue, state->crtcs[i].saved_b, size * sizeof(uint16_t));
		} else {
			randr_fill_linear(gamma->red, gamma->green, gamma->blue, size);
		}

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

	if (g_randr_state == state) {
		g_randr_state = NULL;
	}

	free(state);
}

static void
randr_print_help(FILE *f)
{
	fputs(_("Adjust gamma ramps with the X RANDR extension.\n"), f);
	fputs("\n", f);
	fputs(_("  screen=N\t\tX screen number\n"), f);
	fputs(_("  crtc=N\t\tCRTC index to adjust\n"), f);
	fputs(_("  crtc-gamma=ID:R:G:B\tPer-CRTC RGB white point calibration multiplier (e.g. 0:1.0:0.95:1.0)\n"), f);
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
	} else if (strcasecmp(key, "crtc-gamma") == 0 || strcasecmp(key, "crtc-calibration") == 0) {
		int crtc_idx = -1;
		float r_m = 1.0f, g_m = 1.0f, b_m = 1.0f;
		if (sscanf(value, "%d:%f:%f:%f", &crtc_idx, &r_m, &g_m, &b_m) == 4) {
			randr_set_crtc_calibration(crtc_idx, r_m, g_m, b_m);
		} else {
			return -1;
		}
	} else if (strncasecmp(key, "crtc", 4) == 0 && strstr(key, "-gamma") != NULL) {
		int crtc_idx = -1;
		if (sscanf(key + 4, "%d-gamma", &crtc_idx) == 1) {
			float r_m = 1.0f, g_m = 1.0f, b_m = 1.0f;
			if (sscanf(value, "%f:%f:%f", &r_m, &g_m, &b_m) == 3) {
				randr_set_crtc_calibration(crtc_idx, r_m, g_m, b_m);
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	} else if (strcasecmp(key, "crtc-enabled") == 0) {
		int crtc_idx = -1, en = 1;
		if (sscanf(value, "%d:%d", &crtc_idx, &en) == 2) {
			randr_set_crtc_enabled(crtc_idx, en);
		} else {
			return -1;
		}
	} else if (strncasecmp(key, "crtc", 4) == 0 && strstr(key, "-enabled") != NULL) {
		int crtc_idx = -1;
		if (sscanf(key + 4, "%d-enabled", &crtc_idx) == 1) {
			int en = (strcasecmp(value, "true") == 0 || strcasecmp(value, "1") == 0 || strcasecmp(value, "on") == 0);
			randr_set_crtc_enabled(crtc_idx, en);
		} else {
			return -1;
		}
	} else if (strcasecmp(key, "crtc-brightness") == 0) {
		int crtc_idx = -1;
		float b = 1.0f;
		if (sscanf(value, "%d:%f", &crtc_idx, &b) == 2) {
			randr_set_crtc_brightness(crtc_idx, b);
		} else {
			return -1;
		}
	} else if (strncasecmp(key, "crtc", 4) == 0 && strstr(key, "-brightness") != NULL) {
		int crtc_idx = -1;
		if (sscanf(key + 4, "%d-brightness", &crtc_idx) == 1) {
			float b = (float)atof(value);
			randr_set_crtc_brightness(crtc_idx, b);
		} else {
			return -1;
		}
	} else if (strcasecmp(key, "crtc-offset") == 0) {
		int crtc_idx = -1, off = 0;
		if (sscanf(value, "%d:%d", &crtc_idx, &off) == 2) {
			randr_set_crtc_temp_offset(crtc_idx, off);
		} else {
			return -1;
		}
	} else if (strncasecmp(key, "crtc", 4) == 0 && strstr(key, "-offset") != NULL) {
		int crtc_idx = -1;
		if (sscanf(key + 4, "%d-offset", &crtc_idx) == 1) {
			int off = atoi(value);
			randr_set_crtc_temp_offset(crtc_idx, off);
		} else {
			return -1;
		}
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

	/* If monitor is bypassed (disabled from adjustments), restore neutral linear identity */
	if (!state->crtcs[crtc_num].enabled) {
		if (preserve && state->crtcs[crtc_num].saved_r != NULL) {
			memcpy(gamma->red, state->crtcs[crtc_num].saved_r,
			       ramp_size * sizeof(uint16_t));
			memcpy(gamma->green, state->crtcs[crtc_num].saved_g,
			       ramp_size * sizeof(uint16_t));
			memcpy(gamma->blue, state->crtcs[crtc_num].saved_b,
			       ramp_size * sizeof(uint16_t));
		} else {
			randr_fill_linear(gamma->red, gamma->green, gamma->blue, ramp_size);
		}
		XRRSetCrtcGamma(state->dpy, state->crtcs[crtc_num].crtc, gamma);
		XRRFreeGamma(gamma);
		return 0;
	}

	color_setting_t crtc_setting = *setting;
	if (state->crtcs[crtc_num].temp_offset != 0) {
		int target = (int)crtc_setting.temperature + state->crtcs[crtc_num].temp_offset;
		if (target < 1000) target = 1000;
		if (target > 25000) target = 25000;
		crtc_setting.temperature = (unsigned int)target;
	}
	if (state->crtcs[crtc_num].brightness_mult != 1.0f) {
		float b = crtc_setting.brightness * state->crtcs[crtc_num].brightness_mult;
		if (b < 0.1f) b = 0.1f;
		if (b > 2.0f) b = 2.0f;
		crtc_setting.brightness = b;
	}

	int is_neutral = (crtc_setting.temperature == NEUTRAL_TEMP &&
			  crtc_setting.brightness >= 0.999f && crtc_setting.brightness <= 1.001f &&
			  crtc_setting.gamma[0] >= 0.999f && crtc_setting.gamma[0] <= 1.001f &&
			  crtc_setting.gamma[1] >= 0.999f && crtc_setting.gamma[1] <= 1.001f &&
			  crtc_setting.gamma[2] >= 0.999f && crtc_setting.gamma[2] <= 1.001f &&
			  !crtc_setting.darkroom && !crtc_setting.movie_mode && crtc_setting.cvd_mode == CVD_NONE);

	if (is_neutral &&
	    state->crtcs[crtc_num].gamma_mult[0] == 1.0f &&
	    state->crtcs[crtc_num].gamma_mult[1] == 1.0f &&
	    state->crtcs[crtc_num].gamma_mult[2] == 1.0f) {
		/* Directly write a clean linear identity ramp to revert to hardware defaults */
		randr_fill_linear(gamma->red, gamma->green, gamma->blue, ramp_size);
		XRRSetCrtcGamma(state->dpy, state->crtcs[crtc_num].crtc, gamma);
		XRRFreeGamma(gamma);
		return 0;
	}

	if (preserve && state->crtcs[crtc_num].saved_r != NULL) {
		memcpy(gamma->red, state->crtcs[crtc_num].saved_r,
		       ramp_size * sizeof(uint16_t));
		memcpy(gamma->green, state->crtcs[crtc_num].saved_g,
		       ramp_size * sizeof(uint16_t));
		memcpy(gamma->blue, state->crtcs[crtc_num].saved_b,
		       ramp_size * sizeof(uint16_t));
	} else {
		randr_fill_linear(gamma->red, gamma->green, gamma->blue, ramp_size);
	}

	colorramp_fill(gamma->red, gamma->green, gamma->blue, ramp_size, &crtc_setting);

	/* Apply per-CRTC independent calibration (WO-013) */
	if (state->crtcs[crtc_num].gamma_mult[0] != 1.0f ||
	    state->crtcs[crtc_num].gamma_mult[1] != 1.0f ||
	    state->crtcs[crtc_num].gamma_mult[2] != 1.0f) {
		for (int k = 0; k < ramp_size; k++) {
			double nr = (double)gamma->red[k] * state->crtcs[crtc_num].gamma_mult[0];
			double ng = (double)gamma->green[k] * state->crtcs[crtc_num].gamma_mult[1];
			double nb = (double)gamma->blue[k] * state->crtcs[crtc_num].gamma_mult[2];
			gamma->red[k] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, nr));
			gamma->green[k] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, ng));
			gamma->blue[k] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, nb));
		}
	}

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
