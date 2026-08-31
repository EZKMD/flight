#ifndef GEOGRAPHIC_MAP_POC_H
#define GEOGRAPHIC_MAP_POC_H

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
} GeographicMapPocScene;

bool geographic_map_poc_prepare(GeographicMapPocScene *scene,
                                GeoCoordinate origin, GeoCoordinate destination,
                                int columns, int rows, bool include_geography);
void geographic_map_poc_render(Frame *frame, const FlightState *flight,
                               const AnimationState *animation,
                               const Layout *layout, bool geography_enabled);

#endif
