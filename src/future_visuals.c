#include "future_visuals.h"

static void placeholder(Frame *frame, const char *title)
{
    frame_center(frame, title, 0);
    frame_center(frame, "COMING SOON", 0);
}

void radar_visual_render(Frame *frame, const FlightState *flight,
                         const TelemetryHistory *history,
                         const AnimationState *animation, const Layout *layout)
{
    (void)flight; (void)history; (void)animation; (void)layout;
    placeholder(frame, "RADAR");
}

void minimal_visual_render(Frame *frame, const FlightState *flight,
                           const TelemetryHistory *history,
                           const AnimationState *animation, const Layout *layout)
{
    (void)flight; (void)history; (void)animation; (void)layout;
    placeholder(frame, "MINIMAL");
}
