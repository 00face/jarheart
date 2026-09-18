/* gamma-wayland.c -- Wayland wlr-gamma-control adjustment source
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

   Copyright (c) 2026  Jarheart Authors
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) s
#endif

#include <wayland-client.h>
#include "wlr-gamma-control-unstable-v1-client-protocol.h"

#include "gamma-wayland.h"
#include "redshift.h"
#include "colorramp.h"

#include <math.h>

typedef struct wayland_output {
	struct wl_output *wl_output;
	uint32_t id;
	char name[32];
	int enabled;           /* 1 = adjustments applied, 0 = bypassed (linear identity) */
	float brightness_mult; /* default 1.0f */
	float gamma_mult[3];   /* default 1.0f */
	int temp_offset;       /* default 0 */
	struct zwlr_gamma_control_v1 *gamma_control;
	uint32_t ramp_size;
	int failed;
	uint16_t *current_r;
	uint16_t *current_g;
	uint16_t *current_b;
	struct wayland_output *next;
} wayland_output_t;

typedef struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct zwlr_gamma_control_manager_v1 *manager;
	wayland_output_t *outputs;
} wayland_state_t;

static wayland_state_t *g_wayland_state = NULL;

static wayland_output_t *
get_output_by_index(int idx)
{
	if (g_wayland_state == NULL || idx < 0) return NULL;
	int cur = 0;
	for (wayland_output_t *o = g_wayland_state->outputs; o != NULL; o = o->next) {
		if (cur == idx) return o;
		cur++;
	}
	return NULL;
}

int
wayland_set_output_enabled(int output_idx, int enabled)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	o->enabled = enabled ? 1 : 0;
	return 0;
}

int
wayland_set_output_brightness(int output_idx, float brightness)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	if (brightness < 0.1f) brightness = 0.1f;
	if (brightness > 2.0f) brightness = 2.0f;
	o->brightness_mult = brightness;
	return 0;
}

int
wayland_set_output_calibration(int output_idx, float r, float g, float b)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	if (r < 0.1f) r = 0.1f;
	if (r > 2.0f) r = 2.0f;
	if (g < 0.1f) g = 0.1f;
	if (g > 2.0f) g = 2.0f;
	if (b < 0.1f) b = 0.1f;
	if (b > 2.0f) b = 2.0f;
	o->gamma_mult[0] = r;
	o->gamma_mult[1] = g;
	o->gamma_mult[2] = b;
	return 0;
}

int
wayland_set_output_temp_offset(int output_idx, int temp_offset)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	if (temp_offset < -5000) temp_offset = -5000;
	if (temp_offset > 5000) temp_offset = 5000;
	o->temp_offset = temp_offset;
	return 0;
}

int
wayland_reset_output(int output_idx)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	o->enabled = 1;
	o->brightness_mult = 1.0f;
	o->gamma_mult[0] = 1.0f;
	o->gamma_mult[1] = 1.0f;
	o->gamma_mult[2] = 1.0f;
	o->temp_offset = 0;
	return 0;
}

int
wayland_get_output_count(void)
{
	if (g_wayland_state == NULL) return 0;
	int count = 0;
	for (wayland_output_t *o = g_wayland_state->outputs; o != NULL; o = o->next) {
		count++;
	}
	return count;
}

int
wayland_get_output_info(int output_idx, char *name_buf, size_t name_buf_size,
			int *active, int *enabled, float *brightness,
			float gamma_mult[3], int *temp_offset)
{
	wayland_output_t *o = get_output_by_index(output_idx);
	if (!o) return -1;
	if (name_buf && name_buf_size > 0) {
		snprintf(name_buf, name_buf_size, "%s", o->name[0] ? o->name : "WL-Output");
	}
	if (active) *active = !o->failed && o->ramp_size > 0;
	if (enabled) *enabled = o->enabled;
	if (brightness) *brightness = o->brightness_mult;
	if (gamma_mult) {
		gamma_mult[0] = o->gamma_mult[0];
		gamma_mult[1] = o->gamma_mult[1];
		gamma_mult[2] = o->gamma_mult[2];
	}
	if (temp_offset) *temp_offset = o->temp_offset;
	return 0;
}


#ifndef MFD_CLOEXEC
# define MFD_CLOEXEC 0x0001U
#endif

static int
create_shm_file(off_t size)
{
	int fd = -1;
#if defined(__linux__) && defined(__NR_memfd_create)
	fd = (int)syscall(__NR_memfd_create, "jarheart-gamma", MFD_CLOEXEC);
	if (fd >= 0) {
		if (ftruncate(fd, size) < 0) {
			close(fd);
			return -1;
		}
		return fd;
	}
#endif
	char template[] = "/tmp/jarheart-shm-XXXXXX";
	fd = mkstemp(template);
	if (fd < 0) return -1;
	unlink(template);
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void
gamma_control_handle_gamma_size(void *data, struct zwlr_gamma_control_v1 *control, uint32_t ramp_size)
{
	(void)control;
	wayland_output_t *output = data;
	output->ramp_size = ramp_size;
}

static void
gamma_control_handle_failed(void *data, struct zwlr_gamma_control_v1 *control)
{
	(void)control;
	wayland_output_t *output = data;
	output->failed = 1;
	fprintf(stderr, _("Wayland gamma control failed on output %u.\n"), output->id);
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
	.gamma_size = gamma_control_handle_gamma_size,
	.failed = gamma_control_handle_failed,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	(void)version;
	wayland_state_t *state = data;
	if (strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) == 0) {
		state->manager = wl_registry_bind(registry, id,
						  &zwlr_gamma_control_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		wayland_output_t *output = calloc(1, sizeof(wayland_output_t));
		if (output != NULL) {
			output->id = id;
			output->enabled = 1;
			output->brightness_mult = 1.0f;
			output->gamma_mult[0] = 1.0f;
			output->gamma_mult[1] = 1.0f;
			output->gamma_mult[2] = 1.0f;
			output->temp_offset = 0;
			snprintf(output->name, sizeof(output->name), "WL-%u", id);
			output->wl_output = wl_registry_bind(registry, id, &wl_output_interface, 1);
			output->next = state->outputs;
			state->outputs = output;
		}
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t id)
{
	(void)registry;
	wayland_state_t *state = data;
	wayland_output_t **curr = &state->outputs;
	while (*curr != NULL) {
		if ((*curr)->id == id) {
			wayland_output_t *to_remove = *curr;
			*curr = to_remove->next;
			if (to_remove->gamma_control != NULL) {
				zwlr_gamma_control_v1_destroy(to_remove->gamma_control);
			}
			if (to_remove->wl_output != NULL) {
				wl_output_destroy(to_remove->wl_output);
			}
			free(to_remove->current_r);
			free(to_remove->current_g);
			free(to_remove->current_b);
			free(to_remove);
			break;
		}
		curr = &(*curr)->next;
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};


static int
wayland_init(wayland_state_t **state)
{
	*state = calloc(1, sizeof(wayland_state_t));
	if (*state == NULL) return -1;

	wayland_state_t *s = *state;

	/* Connect to Wayland display */
	s->display = wl_display_connect(NULL);
	if (s->display == NULL) {
		free(s);
		*state = NULL;
		return -1;
	}

	s->registry = wl_display_get_registry(s->display);
	if (s->registry == NULL) {
		wl_display_disconnect(s->display);
		free(s);
		*state = NULL;
		return -1;
	}

	wl_registry_add_listener(s->registry, &registry_listener, s);
	wl_display_roundtrip(s->display);

	if (s->manager == NULL) {
		/* Compositor does not support wlr-gamma-control */
		wl_registry_destroy(s->registry);
		wl_display_disconnect(s->display);
		free(s);
		*state = NULL;
		return -1;
	}

	return 0;
}

static int
wayland_start(wayland_state_t *state)
{
	g_wayland_state = state;
	for (wayland_output_t *o = state->outputs; o != NULL; o = o->next) {
		o->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(
			state->manager, o->wl_output);
		if (o->gamma_control != NULL) {
			zwlr_gamma_control_v1_add_listener(o->gamma_control,
							   &gamma_control_listener, o);
		}
	}

	wl_display_roundtrip(state->display);
	return 0;
}

static void
wayland_restore(wayland_state_t *state)
{
	if (state == NULL || state->display == NULL) return;

	for (wayland_output_t *o = state->outputs; o != NULL; o = o->next) {
		if (o->gamma_control != NULL) {
			zwlr_gamma_control_v1_destroy(o->gamma_control);
			o->gamma_control = NULL;
		}
	}

	wl_display_flush(state->display);
}

static void
wayland_free(wayland_state_t *state)
{
	if (state == NULL) return;

	wayland_restore(state);

	wayland_output_t *curr = state->outputs;
	while (curr != NULL) {
		wayland_output_t *next = curr->next;
		if (curr->wl_output != NULL) {
			wl_output_destroy(curr->wl_output);
		}
		free(curr->current_r);
		free(curr->current_g);
		free(curr->current_b);
		free(curr);
		curr = next;
	}

	if (state->manager != NULL) {
		zwlr_gamma_control_manager_v1_destroy(state->manager);
	}
	if (state->registry != NULL) {
		wl_registry_destroy(state->registry);
	}
	if (state->display != NULL) {
		wl_display_disconnect(state->display);
	}

	if (g_wayland_state == state) {
		g_wayland_state = NULL;
	}

	free(state);
}

static void
wayland_print_help(FILE *f)
{
	fputs(_("Adjust gamma ramps with Wayland wlr-gamma-control protocol.\n"), f);
	fputs("\n", f);
}

static int
wayland_set_option(wayland_state_t *state, const char *key, const char *value)
{
	(void)state;
	(void)value;
	fprintf(stderr, _("Unknown method parameter: `%s'.\n"), key);
	return -1;
}

static int
wayland_set_temperature(
	wayland_state_t *state, const color_setting_t *setting, int preserve)
{
	(void)preserve;
	for (wayland_output_t *o = state->outputs; o != NULL; o = o->next) {
		if (o->failed || o->ramp_size == 0 || o->gamma_control == NULL) {
			continue;
		}

		size_t ramp_bytes = o->ramp_size * sizeof(uint16_t);
		size_t total_bytes = 3 * ramp_bytes;

		uint16_t *target_r = malloc(ramp_bytes);
		uint16_t *target_g = malloc(ramp_bytes);
		uint16_t *target_b = malloc(ramp_bytes);
		if (!target_r || !target_g || !target_b) {
			free(target_r); free(target_g); free(target_b);
			return -1;
		}

		/* If output is bypassed, write clean linear identity ramp */
		if (!o->enabled) {
			int fd = create_shm_file((off_t)total_bytes);
			if (fd >= 0) {
				uint16_t *table = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE,
						       MAP_SHARED, fd, 0);
				if (table != MAP_FAILED) {
					for (uint32_t i = 0; i < o->ramp_size; i++) {
						uint16_t val = (uint16_t)(((double)i / (o->ramp_size > 1 ? (o->ramp_size - 1) : 1)) * UINT16_MAX);
						table[0 * o->ramp_size + i] = val;
						table[1 * o->ramp_size + i] = val;
						table[2 * o->ramp_size + i] = val;
					}
					munmap(table, total_bytes);
					zwlr_gamma_control_v1_set_gamma(o->gamma_control, fd);
				}
				close(fd);
			}
			free(target_r); free(target_g); free(target_b);
			continue;
		}

		color_setting_t output_setting = *setting;
		if (o->temp_offset != 0) {
			int target = (int)output_setting.temperature + o->temp_offset;
			if (target < 1000) target = 1000;
			if (target > 25000) target = 25000;
			output_setting.temperature = (unsigned int)target;
		}
		if (o->brightness_mult != 1.0f) {
			float b = output_setting.brightness * o->brightness_mult;
			if (b < 0.1f) b = 0.1f;
			if (b > 2.0f) b = 2.0f;
			output_setting.brightness = b;
		}

		for (uint32_t i = 0; i < o->ramp_size; i++) {
			uint16_t val = (uint16_t)(((double)i / (o->ramp_size > 1 ? (o->ramp_size - 1) : 1)) * UINT16_MAX);
			target_r[i] = val;
			target_g[i] = val;
			target_b[i] = val;
		}

		colorramp_fill(target_r, target_g, target_b, (int)o->ramp_size, &output_setting);

		if (o->gamma_mult[0] != 1.0f || o->gamma_mult[1] != 1.0f || o->gamma_mult[2] != 1.0f) {
			for (uint32_t i = 0; i < o->ramp_size; i++) {
				double nr = (double)target_r[i] * o->gamma_mult[0];
				double ng = (double)target_g[i] * o->gamma_mult[1];
				double nb = (double)target_b[i] * o->gamma_mult[2];
				target_r[i] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, nr));
				target_g[i] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, ng));
				target_b[i] = (uint16_t)fmin(UINT16_MAX, fmax(0.0, nb));
			}
		}

		/* WO-017: If a prior ramp is active and differs, smoothly blend across sub-frames */
		int blend_steps = (o->current_r != NULL) ? 4 : 1;
		for (int step = 1; step <= blend_steps; step++) {
			double alpha = (double)step / blend_steps;

			int fd = create_shm_file((off_t)total_bytes);
			if (fd < 0) {
				perror("create_shm_file");
				free(target_r); free(target_g); free(target_b);
				return -1;
			}

			uint16_t *table = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE,
					       MAP_SHARED, fd, 0);
			if (table == MAP_FAILED) {
				perror("mmap");
				close(fd);
				free(target_r); free(target_g); free(target_b);
				return -1;
			}

			uint16_t *r = &table[0 * o->ramp_size];
			uint16_t *g = &table[1 * o->ramp_size];
			uint16_t *b = &table[2 * o->ramp_size];

			if (blend_steps > 1 && o->current_r != NULL) {
				for (uint32_t i = 0; i < o->ramp_size; i++) {
					r[i] = (uint16_t)((1.0 - alpha) * o->current_r[i] + alpha * target_r[i]);
					g[i] = (uint16_t)((1.0 - alpha) * o->current_g[i] + alpha * target_g[i]);
					b[i] = (uint16_t)((1.0 - alpha) * o->current_b[i] + alpha * target_b[i]);
				}
			} else {
				memcpy(r, target_r, ramp_bytes);
				memcpy(g, target_g, ramp_bytes);
				memcpy(b, target_b, ramp_bytes);
			}

			munmap(table, total_bytes);
			zwlr_gamma_control_v1_set_gamma(o->gamma_control, fd);
			close(fd);

			if (step < blend_steps) {
				wl_display_flush(state->display);
				struct timespec ts = { .tv_sec = 0, .tv_nsec = 16000000 }; /* 16ms blend tick */
				nanosleep(&ts, NULL);
			}
		}

		if (o->current_r == NULL) {
			o->current_r = malloc(ramp_bytes);
			o->current_g = malloc(ramp_bytes);
			o->current_b = malloc(ramp_bytes);
		}
		if (o->current_r && o->current_g && o->current_b) {
			memcpy(o->current_r, target_r, ramp_bytes);
			memcpy(o->current_g, target_g, ramp_bytes);
			memcpy(o->current_b, target_b, ramp_bytes);
		}

		free(target_r);
		free(target_g);
		free(target_b);
	}

	wl_display_flush(state->display);
	return 0;
}

static int
wayland_get_fd(wayland_state_t *state)
{
	return (state && state->display) ? wl_display_get_fd(state->display) : -1;
}

static int
wayland_handle(wayland_state_t *state)
{
	if (state == NULL || state->display == NULL) return 0;
	if (wl_display_dispatch_pending(state->display) < 0) {
		return -1;
	}
	return 0;
}

const gamma_method_t wayland_gamma_method = {
	"wayland", 1,
	(gamma_method_init_func *)wayland_init,
	(gamma_method_start_func *)wayland_start,
	(gamma_method_free_func *)wayland_free,
	(gamma_method_print_help_func *)wayland_print_help,
	(gamma_method_set_option_func *)wayland_set_option,
	(gamma_method_restore_func *)wayland_restore,
	(gamma_method_set_temperature_func *)wayland_set_temperature,
	(gamma_method_get_fd_func *)wayland_get_fd,
	(gamma_method_handle_func *)wayland_handle
};
