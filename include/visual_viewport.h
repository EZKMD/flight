#ifndef VISUAL_VIEWPORT_H
#define VISUAL_VIEWPORT_H

#include <stdbool.h>
#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"
#include "telemetry_history.h"

typedef enum {
    VISUAL_AIRCRAFT,
    VISUAL_ALTITUDE_PROFILE,
    VISUAL_ROUTE_MAP,
    VISUAL_RADAR,
    VISUAL_MINIMAL
} VisualMode;

typedef struct {
    VisualMode mode;
    const TelemetryHistory *history;
    bool geography_enabled;
} VisualViewport;

void visual_viewport_init(VisualViewport *viewport, VisualMode mode,
                          const TelemetryHistory *history);
bool visual_mode_parse(const char *name, VisualMode *mode);
const char *visual_mode_name(VisualMode mode);
void visual_viewport_toggle(VisualViewport *viewport);
bool visual_viewport_toggle_geography(VisualViewport *viewport);
void visual_viewport_render(const VisualViewport *viewport, Frame *frame,
                            const FlightState *flight, const AnimationState *animation,
                            const Layout *layout);

#endif
