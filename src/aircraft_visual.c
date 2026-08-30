#include "aircraft_visual.h"

#include "artwork.h"

#include <stdio.h>

void aircraft_visual_render(Frame *frame, const FlightState *flight,
                            const AnimationState *animation, const Layout *layout)
{
    const AircraftArtwork *art;
    char suffix[32];
    int origin;
    int index;
    (void)flight;
    if (!layout->show_artwork) return;

    art = layout->use_small_artwork ? artwork_small() : artwork_normal();
    origin = (frame->width - art->width) / 2 + animation->drift;
    for (index = 0; index < art->line_count; index++) {
        if (index == art->line_count - 1) {
            (void)snprintf(suffix, sizeof(suffix), "  %s", animation->propulsion);
            frame_at(frame, origin - 1, art->lines[index], suffix);
        } else if (art == artwork_normal() && index == 0) {
            frame_at(frame, origin + 1, art->lines[index], "");
        } else {
            frame_at(frame, origin, art->lines[index], "");
        }
    }
}
