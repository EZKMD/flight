#ifndef MAP_RASTER_H
#define MAP_RASTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "map_geometry.h"

#define MAP_RASTER_MAX_COLUMNS 104
#define MAP_RASTER_MAX_ROWS 16

typedef enum {
    MAP_RASTER_BRAILLE,
    MAP_RASTER_COMPAT
} MapRasterBackend;

typedef struct {
    int columns;
    int rows;
    uint32_t cells[MAP_RASTER_MAX_ROWS][MAP_RASTER_MAX_COLUMNS];
    int marker_column;
    int marker_row;
    bool marker_available;
} MapRaster;

bool map_raster_render(MapRaster *raster, MapRasterBackend backend,
                       const MapPoint *route, size_t route_count,
                       const MapPoint *marker, int columns, int rows);
bool map_raster_row_utf8(const MapRaster *raster, int row, char *output, size_t capacity);

#endif
