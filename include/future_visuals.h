#ifndef FUTURE_VISUALS_H
#define FUTURE_VISUALS_H

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"
#include "telemetry_history.h"

void radar_visual_render(Frame *frame, const FlightState *flight,
                         const TelemetryHistory *history,
                         const AnimationState *animation, const Layout *layout);
void minimal_visual_render(Frame *frame, const FlightState *flight,
                           const TelemetryHistory *history,
                           const AnimationState *animation, const Layout *layout);

#endif
