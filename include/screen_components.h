#ifndef SCREEN_COMPONENTS_H
#define SCREEN_COMPONENTS_H

#include <time.h>
#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"

void flight_header_render(Frame *frame, const FlightState *flight,
                          const AnimationState *animation);
void route_progress_render(Frame *frame, const FlightState *flight);
void flight_metrics_render(Frame *frame, const FlightState *flight,
                           const Layout *layout);
void flight_status_render(Frame *frame, const FlightState *flight);
void data_freshness_render(Frame *frame, const FlightState *flight,
                           const AnimationState *animation, time_t now);
void compact_summary_render(Frame *frame, const FlightState *flight,
                            const AnimationState *animation, time_t now);

#endif
