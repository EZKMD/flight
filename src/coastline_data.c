#include "coastline_data.h"

#include <stdint.h>

typedef struct {
    int16_t longitude_hundredths;
    int16_t latitude_hundredths;
} CoastlinePoint;

#include "../data/coastline_110m_generated.inc"

size_t coastline_data_point_count(void) { return COASTLINE_POINT_COUNT; }
size_t coastline_data_segment_count(void) { return COASTLINE_SEGMENT_COUNT; }

bool coastline_data_segment_bounds(size_t index, size_t *first_point,
                                   size_t *point_count)
{
    size_t start;
    size_t end;
    if (first_point == NULL || point_count == NULL ||
        index >= COASTLINE_SEGMENT_COUNT) return false;
    start = coastline_segment_offsets[index];
    end = coastline_segment_offsets[index + 1U];
    if (end <= start) return false;
    *first_point = start;
    *point_count = end - start;
    return true;
}

bool coastline_data_point(size_t index, GeoCoordinate *point)
{
    if (point == NULL || index >= COASTLINE_POINT_COUNT) return false;
    point->longitude = (double)coastline_points[index].longitude_hundredths / 100.0;
    point->latitude = (double)coastline_points[index].latitude_hundredths / 100.0;
    return true;
}
