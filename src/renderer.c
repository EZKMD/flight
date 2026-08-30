#include "renderer.h"

#include "frame.h"
#include "screen_components.h"

#include <stdio.h>
#include <time.h>

static void build_full(Frame *frame, const FlightState *flight,
                       const AnimationState *animation, const Layout *layout,
                       const VisualViewport *viewport, time_t now)
{
    flight_header_render(frame, flight, animation);
    frame_blank(frame);
    visual_viewport_render(viewport, frame, flight, animation, layout);
    if (layout->show_artwork) frame_blank(frame);
    route_progress_render(frame, flight);
    frame_blank(frame);
    flight_metrics_render(frame, flight, layout);
    frame_blank(frame);
    flight_status_render(frame, flight);
    frame_blank(frame);
    data_freshness_render(frame, flight, animation, now);
}

static void build_too_small(Frame *frame)
{
    char requirement[64];
    frame_center(frame, "terminal too small", 0);
    (void)snprintf(requirement, sizeof(requirement), "minimum %dx%d",
                   LAYOUT_MIN_WIDTH, LAYOUT_MIN_HEIGHT);
    frame_center(frame, requirement, 0);
}

void renderer_draw(const FlightState *flight, const AnimationState *animation,
                   const Layout *layout, const VisualViewport *viewport)
{
    Frame frame;
    int top;
    int row;
    int index;
    int visible;
    time_t now = time(NULL);
    frame_init(&frame, layout->content_width);

    if (layout->terminal_too_small) build_too_small(&frame);
    else if (viewport->mode == VISUAL_ALTITUDE_PROFILE)
        visual_viewport_render(viewport, &frame, flight, animation, layout);
    else if (layout->mode == LAYOUT_TINY || layout->height < 13)
        compact_summary_render(&frame, flight, animation, now);
    else build_full(&frame, flight, animation, layout, viewport, now);

    visible = frame.count < layout->height ? frame.count : layout->height;
    top = (layout->height - visible) / 2;
    (void)fputs("\x1b[H", stdout);
    for (row = 0; row < layout->height; row++) {
        (void)fputs("\x1b[2K", stdout);
        index = row - top;
        if (index >= 0 && index < visible) {
            int left = (layout->width - frame.width) / 2;
            if (left < 0) left = 0;
            (void)fprintf(stdout, "%*s%s", left, "", frame.lines[index]);
        }
        if (row + 1 < layout->height) (void)fputc('\n', stdout);
    }
    (void)fflush(stdout);
}
