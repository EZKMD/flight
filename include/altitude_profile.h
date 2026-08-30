#ifndef ALTITUDE_PROFILE_H
#define ALTITUDE_PROFILE_H

#include <stdbool.h>
#include <time.h>

#include "animation.h"
#include "flight_state.h"
#include "frame.h"
#include "layout.h"
#include "telemetry_history.h"

#define ALTITUDE_PROFILE_MAX_COLUMNS 104
#define ALTITUDE_PROFILE_MAX_ROWS 14
#define ALTITUDE_PROFILE_MIN_WIDTH 50
#define ALTITUDE_PROFILE_MIN_HEIGHT 14

typedef enum {
    ALTITUDE_CELL_EMPTY,
    ALTITUDE_CELL_HORIZONTAL,
    ALTITUDE_CELL_VERTICAL,
    ALTITUDE_CELL_CROSS,
    ALTITUDE_CELL_POINT,
    ALTITUDE_CELL_CURRENT
} AltitudeProfileCell;

typedef struct {
    int width;
    int height;
    int maximum_altitude_feet;
    time_t first_timestamp;
    time_t last_timestamp;
    size_t plotted_samples;
    AltitudeProfileCell cells[ALTITUDE_PROFILE_MAX_ROWS][ALTITUDE_PROFILE_MAX_COLUMNS];
} AltitudeProfilePlot;

int altitude_profile_map_time(time_t timestamp, time_t first, time_t last, int width);
int altitude_profile_map_altitude(int altitude_feet, int maximum_altitude_feet, int height);
bool altitude_profile_is_gap(time_t previous, time_t current,
                             unsigned int expected_interval_seconds);
bool altitude_profile_build(const TelemetryHistory *history, int width, int height,
                            AltitudeProfilePlot *plot);
void altitude_profile_visual_render(Frame *frame, const FlightState *flight,
                                    const TelemetryHistory *history,
                                    const AnimationState *animation, const Layout *layout);

#endif
