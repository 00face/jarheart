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

typedef struct wayland_output {
	struct wl_output *wl_output;
	uint32_t id;
	struct zwlr_gamma_control_v1 *gamma_control;
	uint32_t ramp_size;
	int failed;
	struct wayland_output *next;
} wayland_output_t;

typedef struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct zwlr_gamma_control_manager_v1 *manager;
	wayland_output_t *outputs;
} wayland_state_t;


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

		int fd = create_shm_file((off_t)total_bytes);
		if (fd < 0) {
			perror("create_shm_file");
			return -1;
		}

		uint16_t *table = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE,
				       MAP_SHARED, fd, 0);
		if (table == MAP_FAILED) {
			perror("mmap");
			close(fd);
			return -1;
		}

		uint16_t *r = &table[0 * o->ramp_size];
		uint16_t *g = &table[1 * o->ramp_size];
		uint16_t *b = &table[2 * o->ramp_size];

		for (uint32_t i = 0; i < o->ramp_size; i++) {
			uint16_t val = (uint16_t)(((double)i / (o->ramp_size - 1)) * UINT16_MAX);
			r[i] = val;
			g[i] = val;
			b[i] = val;
		}

		colorramp_fill(r, g, b, (int)o->ramp_size, setting);

		munmap(table, total_bytes);

		zwlr_gamma_control_v1_set_gamma(o->gamma_control, fd);
		close(fd);
	}

	wl_display_flush(state->display);
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
	(gamma_method_set_temperature_func *)wayland_set_temperature
};
