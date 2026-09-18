/* gamma-randr.h -- X RANDR gamma adjustment header
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

   Copyright (c) 2010-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifndef REDSHIFT_GAMMA_RANDR_H
#define REDSHIFT_GAMMA_RANDR_H

#include "redshift.h"

extern const gamma_method_t randr_gamma_method;

/* Multi-monitor independent control functions */
int randr_set_crtc_calibration(int crtc_num, float r, float g, float b);
int randr_set_crtc_enabled(int crtc_num, int enabled);
int randr_set_crtc_brightness(int crtc_num, float brightness);
int randr_set_crtc_temp_offset(int crtc_num, int temp_offset);
int randr_reset_crtc(int crtc_num);
int randr_get_crtc_count(void);
int randr_get_crtc_info(int crtc_num, char *name_buf, size_t name_buf_size,
			int *active, int *enabled, float *brightness,
			float gamma_mult[3], int *temp_offset);

#endif /* ! REDSHIFT_GAMMA_RANDR_H */
