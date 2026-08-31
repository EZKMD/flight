#ifndef ROUTE_MAP_COMPAT_H
#define ROUTE_MAP_COMPAT_H

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"

void route_map_compat_render(Frame *frame, const FlightState *flight,
                             const AnimationState *animation, const Layout *layout);

#endif
