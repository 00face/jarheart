/* gamma-wayland.h -- Wayland wlr-gamma-control adjustment header
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

#ifndef REDSHIFT_GAMMA_WAYLAND_H
#define REDSHIFT_GAMMA_WAYLAND_H

#include "redshift.h"

extern const gamma_method_t wayland_gamma_method;

/* Wayland multi-output independent control functions */
int wayland_set_output_enabled(int output_idx, int enabled);
int wayland_set_output_brightness(int output_idx, float brightness);
int wayland_set_output_calibration(int output_idx, float r, float g, float b);
int wayland_set_output_temp_offset(int output_idx, int temp_offset);
int wayland_reset_output(int output_idx);
int wayland_get_output_count(void);
int wayland_get_output_info(int output_idx, char *name_buf, size_t name_buf_size,
			    int *active, int *enabled, float *brightness,
			    float gamma_mult[3], int *temp_offset);

#endif /* ! REDSHIFT_GAMMA_WAYLAND_H */
