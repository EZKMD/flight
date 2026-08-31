#include "visual_viewport.h"

#include "aircraft_visual.h"
#include "altitude_profile.h"
#include "future_visuals.h"
#include "geographic_map_poc.h"
#include "route_map.h"

#include <string.h>

void visual_viewport_init(VisualViewport *viewport, VisualMode mode,
                          const TelemetryHistory *history)
{
    viewport->mode = mode;
    viewport->history = history;
}

bool visual_mode_parse(const char *name, VisualMode *mode)
{
    if (strcmp(name, "aircraft") == 0) *mode = VISUAL_AIRCRAFT;
    else if (strcmp(name, "altitude") == 0) *mode = VISUAL_ALTITUDE_PROFILE;
    else if (strcmp(name, "route") == 0) *mode = VISUAL_ROUTE_MAP;
    else if (strcmp(name, "geo") == 0) *mode = VISUAL_GEOGRAPHIC_MAP_POC;
    else return false;
    return true;
}

const char *visual_mode_name(VisualMode mode)
{
    if (mode == VISUAL_ALTITUDE_PROFILE) return "altitude";
    if (mode == VISUAL_ROUTE_MAP) return "route";
    if (mode == VISUAL_GEOGRAPHIC_MAP_POC) return "geo";
    return "aircraft";
}

void visual_viewport_toggle(VisualViewport *viewport)
{
    if (viewport->mode == VISUAL_AIRCRAFT) viewport->mode = VISUAL_ALTITUDE_PROFILE;
    else if (viewport->mode == VISUAL_ALTITUDE_PROFILE) viewport->mode = VISUAL_ROUTE_MAP;
    else viewport->mode = VISUAL_AIRCRAFT;
}

void visual_viewport_render(const VisualViewport *viewport, Frame *frame,
                            const FlightState *flight, const AnimationState *animation,
                            const Layout *layout)
{
    switch (viewport->mode) {
        case VISUAL_AIRCRAFT:
            aircraft_visual_render(frame, flight, animation, layout);
            break;
        case VISUAL_ALTITUDE_PROFILE:
            altitude_profile_visual_render(frame, flight, viewport->history, animation, layout);
            break;
        case VISUAL_ROUTE_MAP:
            route_map_visual_render(frame, flight, animation, layout);
            break;
        case VISUAL_GEOGRAPHIC_MAP_POC:
            geographic_map_poc_render(frame, flight, animation, layout);
            break;
        case VISUAL_RADAR:
            radar_visual_render(frame, flight, viewport->history, animation, layout);
            break;
        case VISUAL_MINIMAL:
            minimal_visual_render(frame, flight, viewport->history, animation, layout);
            break;
    }
}
