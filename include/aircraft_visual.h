#ifndef AIRCRAFT_VISUAL_H
#define AIRCRAFT_VISUAL_H

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"

void aircraft_visual_render(Frame *frame, const FlightState *flight,
                            const AnimationState *animation, const Layout *layout);

#endif
