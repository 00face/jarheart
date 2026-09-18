/* location-timezone.h -- System timezone location provider header
   This file is part of Jarheart.

   Jarheart is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/

#ifndef JARHEART_LOCATION_TIMEZONE_H
#define JARHEART_LOCATION_TIMEZONE_H

#include "redshift.h"

extern const location_provider_t timezone_location_provider;

int location_timezone_resolve(location_t *loc, char *tz_name, size_t tz_name_len);

#endif /* JARHEART_LOCATION_TIMEZONE_H */
