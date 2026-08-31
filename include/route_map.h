#ifndef ROUTE_MAP_H
#define ROUTE_MAP_H

#include <stdbool.h>

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"
#include "map_raster.h"

MapRasterBackend route_map_backend_for_layout(LayoutMode mode);
void route_map_visual_render(Frame *frame, const FlightState *flight,
                             const AnimationState *animation, const Layout *layout,
                             bool geography_enabled);

#endif
