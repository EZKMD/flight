#ifndef ROUTE_MAP_SCENE_H
#define ROUTE_MAP_SCENE_H

#include <stdbool.h>

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"
#include "map_geometry.h"
#include "subcell_canvas.h"

typedef struct {
    GeoCoordinate origin;
    GeoCoordinate destination;
    int columns;
    int rows;
    MapViewport viewport;
    MapPoint projected_route[MAP_ROUTE_MAX_SAMPLES];
    size_t route_count;
    SubcellCanvas coastline;
    size_t coastline_segments_drawn;
    bool geography_available;
    bool valid;
} RouteMapScene;

bool route_map_scene_prepare(RouteMapScene *scene, GeoCoordinate origin,
                             GeoCoordinate destination, int columns, int rows,
                             bool include_geography);

#endif
