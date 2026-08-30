#ifndef RENDERER_H
#define RENDERER_H

#include "animation.h"
#include "flight_state.h"
#include "layout.h"
#include "visual_viewport.h"

void renderer_draw(const FlightState *flight, const AnimationState *animation,
                   const Layout *layout, const VisualViewport *viewport);

#endif
