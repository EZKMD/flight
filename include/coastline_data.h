#ifndef COASTLINE_DATA_H
#define COASTLINE_DATA_H

#include <stdbool.h>
#include <stddef.h>

#include "map_geometry.h"

size_t coastline_data_point_count(void);
size_t coastline_data_segment_count(void);
bool coastline_data_segment_bounds(size_t index, size_t *first_point,
                                   size_t *point_count);
bool coastline_data_point(size_t index, GeoCoordinate *point);

#endif
