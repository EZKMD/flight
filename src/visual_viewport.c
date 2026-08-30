#include "visual_viewport.h"

#include "aircraft_visual.h"
#include "future_visuals.h"

void visual_viewport_init(VisualViewport *viewport, VisualMode mode,
                          const TelemetryHistory *history)
{
    viewport->mode = mode;
    viewport->history = history;
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
            route_map_visual_render(frame, flight, viewport->history, animation, layout);
            break;
        case VISUAL_RADAR:
            radar_visual_render(frame, flight, viewport->history, animation, layout);
            break;
        case VISUAL_MINIMAL:
            minimal_visual_render(frame, flight, viewport->history, animation, layout);
            break;
    }
}
