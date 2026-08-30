#include "altitude_profile.h"

#include "screen_components.h"

#include <stdio.h>
#include <string.h>

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

int altitude_profile_map_time(time_t timestamp, time_t first, time_t last, int width)
{
    double fraction;
    if (width <= 1 || last <= first) return 0;
    if (timestamp <= first) return 0;
    if (timestamp >= last) return width - 1;
    fraction = (double)(timestamp - first) / (double)(last - first);
    return clamp_int((int)(fraction * (double)(width - 1) + 0.5), 0, width - 1);
}

int altitude_profile_map_altitude(int altitude_feet, int maximum_altitude_feet, int height)
{
    double fraction;
    if (height <= 1 || maximum_altitude_feet <= 0) return 0;
    altitude_feet = clamp_int(altitude_feet, 0, maximum_altitude_feet);
    fraction = (double)altitude_feet / (double)maximum_altitude_feet;
    return clamp_int((int)((1.0 - fraction) * (double)(height - 1) + 0.5), 0, height - 1);
}

bool altitude_profile_is_gap(time_t previous, time_t current,
                             unsigned int expected_interval_seconds)
{
    time_t threshold;
    if (current <= previous) return true;
    if (expected_interval_seconds == 0U)
        expected_interval_seconds = TELEMETRY_HISTORY_DEFAULT_INTERVAL_SECONDS;
    threshold = (time_t)expected_interval_seconds * 2;
    return current - previous > threshold;
}

static void merge_cell(AltitudeProfilePlot *plot, int x, int y, AltitudeProfileCell cell)
{
    AltitudeProfileCell existing;
    if (x < 0 || x >= plot->width || y < 0 || y >= plot->height) return;
    existing = plot->cells[y][x];
    if (cell == ALTITUDE_CELL_CURRENT || existing == ALTITUDE_CELL_CURRENT) {
        plot->cells[y][x] = ALTITUDE_CELL_CURRENT;
    } else if (cell == ALTITUDE_CELL_POINT || existing == ALTITUDE_CELL_POINT) {
        plot->cells[y][x] = ALTITUDE_CELL_POINT;
    } else if (existing == ALTITUDE_CELL_EMPTY || existing == cell) {
        plot->cells[y][x] = cell;
    } else {
        plot->cells[y][x] = ALTITUDE_CELL_CROSS;
    }
}

static int scale_maximum(const TelemetryHistory *history)
{
    int maximum = history->maximum_altitude_feet > 40000 ?
                  history->maximum_altitude_feet : 40000;
    return ((maximum + 9999) / 10000) * 10000;
}

static void connect_points(AltitudeProfilePlot *plot, int from_x, int from_y,
                           int to_x, int to_y)
{
    int x;
    int previous_y = from_y;
    int width = to_x - from_x;
    if (width <= 0) {
        int y;
        int low = from_y < to_y ? from_y : to_y;
        int high = from_y > to_y ? from_y : to_y;
        for (y = low; y <= high; y++) merge_cell(plot, to_x, y, ALTITUDE_CELL_VERTICAL);
        return;
    }
    for (x = from_x; x <= to_x; x++) {
        int target_y = from_y + ((to_y - from_y) * (x - from_x)) / width;
        int low = previous_y < target_y ? previous_y : target_y;
        int high = previous_y > target_y ? previous_y : target_y;
        int y;
        merge_cell(plot, x, target_y, ALTITUDE_CELL_HORIZONTAL);
        for (y = low; y <= high; y++) {
            if (y != target_y) merge_cell(plot, x, y, ALTITUDE_CELL_VERTICAL);
        }
        previous_y = target_y;
    }
}

bool altitude_profile_build(const TelemetryHistory *history, int width, int height,
                            AltitudeProfilePlot *plot)
{
    const TelemetrySample *first = NULL;
    const TelemetrySample *last = NULL;
    const TelemetrySample *previous = NULL;
    int previous_x = 0;
    int previous_y = 0;
    int newest_x = 0;
    int newest_y = 0;
    bool have_newest = false;
    size_t index;
    memset(plot, 0, sizeof(*plot));
    if (history == NULL) return false;
    plot->width = clamp_int(width, 1, ALTITUDE_PROFILE_MAX_COLUMNS);
    plot->height = clamp_int(height, 1, ALTITUDE_PROFILE_MAX_ROWS);
    plot->maximum_altitude_feet = scale_maximum(history);
    for (index = 0; index < history->count; index++) {
        const TelemetrySample *sample = telemetry_history_at(history, index);
        if (sample == NULL || !sample->timestamp.available) continue;
        if (first == NULL) first = sample;
        last = sample;
    }
    if (first == NULL || last == NULL) return false;
    plot->first_timestamp = first->timestamp.value;
    plot->last_timestamp = last->timestamp.value;
    for (index = 0; index < history->count; index++) {
        const TelemetrySample *sample = telemetry_history_at(history, index);
        int x;
        int y;
        if (sample == NULL || !sample->timestamp.available) continue;
        if (!sample->altitude_feet.available) {
            previous = NULL;
            have_newest = false;
            continue;
        }
        x = altitude_profile_map_time(sample->timestamp.value, plot->first_timestamp,
                                      plot->last_timestamp, plot->width);
        y = altitude_profile_map_altitude(sample->altitude_feet.value,
                                          plot->maximum_altitude_feet, plot->height);
        if (previous != NULL &&
            !altitude_profile_is_gap(previous->timestamp.value, sample->timestamp.value,
                                     history->expected_interval_seconds))
            connect_points(plot, previous_x, previous_y, x, y);
        else merge_cell(plot, x, y, ALTITUDE_CELL_POINT);
        previous = sample;
        previous_x = x;
        previous_y = y;
        newest_x = x;
        newest_y = y;
        have_newest = true;
        plot->plotted_samples++;
    }
    if (have_newest) merge_cell(plot, newest_x, newest_y, ALTITUDE_CELL_CURRENT);
    return plot->plotted_samples > 0U;
}

static const char *cell_text(AltitudeProfileCell cell, bool show_aircraft)
{
    switch (cell) {
        case ALTITUDE_CELL_HORIZONTAL: return "─";
        case ALTITUDE_CELL_VERTICAL: return "│";
        case ALTITUDE_CELL_CROSS: return "┼";
        case ALTITUDE_CELL_POINT: return "·";
        case ALTITUDE_CELL_CURRENT: return show_aircraft ? "✈" : "─";
        case ALTITUDE_CELL_EMPTY: return " ";
    }
    return " ";
}

static void append_text(char *line, size_t capacity, const char *text)
{
    size_t used = strlen(line);
    if (used + 1U < capacity)
        (void)snprintf(line + used, capacity - used, "%s", text);
}

static void context_render(Frame *frame, const FlightState *flight)
{
    char route[64];
    char title[64];
    const char *origin = flight->origin.iata[0] != '\0' ? flight->origin.iata : "---";
    const char *destination = flight->destination.iata[0] != '\0' ?
                              flight->destination.iata : "---";
    (void)snprintf(title, sizeof(title), "%s  ALTITUDE PROFILE",
                   flight->identity.flight_number[0] != '\0' ?
                   flight->identity.flight_number : "--");
    frame_sides(frame, title, flight_display_label(flight->status.display_state));
    (void)snprintf(route, sizeof(route), "%s  →  %s", origin, destination);
    if (flight->position.flight_level.available) {
        char altitude[32];
        const char *label = flight->status.telemetry_state == TELEMETRY_FRESH ?
                            "CURRENT" : "LAST OBSERVED";
        (void)snprintf(altitude, sizeof(altitude), "%s FL%03d", label,
                       flight->position.flight_level.value);
        frame_sides(frame, route, altitude);
    } else frame_add(frame, route);
}

static const char *empty_message(const FlightState *flight)
{
    if (flight->status.phase == FLIGHT_PHASE_LANDED) return "NO SESSION ALTITUDE HISTORY";
    if (flight->status.display_state == DISPLAY_SCHEDULED)
        return "NO LIVE ALTITUDE HISTORY";
    if (flight->status.display_state == DISPLAY_OFFLINE)
        return "NO LIVE ALTITUDE HISTORY";
    return "WAITING FOR LIVE TELEMETRY";
}

static void render_plot(Frame *frame, const AltitudeProfilePlot *plot,
                        const AnimationState *animation)
{
    bool show_aircraft = animation != NULL && animation->heartbeat != NULL &&
                         strcmp(animation->heartbeat, "•") == 0;
    int row;
    for (row = 0; row < plot->height; row++) {
        char line[FRAME_LINE_CAPACITY] = "";
        int altitude = plot->maximum_altitude_feet -
                       (plot->maximum_altitude_feet * row) / (plot->height - 1);
        int column;
        if (row == 0 || row == plot->height / 2 || row == plot->height - 1) {
            char label[16];
            (void)snprintf(label, sizeof(label), "FL%03d ", altitude / 100);
            append_text(line, sizeof(line), label);
        } else append_text(line, sizeof(line), "      ");
        append_text(line, sizeof(line), "│ ");
        for (column = 0; column < plot->width; column++)
            append_text(line, sizeof(line),
                        cell_text(plot->cells[row][column], show_aircraft));
        frame_add(frame, line);
    }
    frame_sides(frame, "        TRACKING START", "NOW");
}

void altitude_profile_visual_render(Frame *frame, const FlightState *flight,
                                    const TelemetryHistory *history,
                                    const AnimationState *animation, const Layout *layout)
{
    AltitudeProfilePlot plot;
    int graph_height;
    int graph_width;
    (void)animation;
    context_render(frame, flight);
    frame_blank(frame);
    if (layout->width < ALTITUDE_PROFILE_MIN_WIDTH ||
        layout->height < ALTITUDE_PROFILE_MIN_HEIGHT) {
        frame_center(frame, "ALTITUDE PROFILE REQUIRES MORE SPACE", 0);
        frame_center(frame, "minimum 50x14", 0);
        return;
    }
    if (history == NULL || history->count == 0U) {
        frame_center(frame, empty_message(flight), 0);
        frame_center(frame, "SESSION HISTORY BEGINS WITH OBSERVED TELEMETRY", 0);
        frame_blank(frame);
        data_freshness_render(frame, flight, animation, time(NULL));
        return;
    }
    if (history->count == 1U) {
        frame_center(frame, "TRACKING STARTED", 0);
        frame_center(frame, "INSUFFICIENT HISTORY FOR PROFILE", 0);
        frame_blank(frame);
        data_freshness_render(frame, flight, animation, time(NULL));
        return;
    }
    graph_height = layout->height - 9;
    if (layout->mode == LAYOUT_WIDE && graph_height > 12) graph_height = 12;
    else if (layout->mode == LAYOUT_MEDIUM && graph_height > 10) graph_height = 10;
    else if (graph_height > 7) graph_height = 7;
    if (graph_height < 4) graph_height = 4;
    graph_width = frame->width - 8;
    if (!altitude_profile_build(history, graph_width, graph_height, &plot) ||
        plot.plotted_samples < 2U) {
        frame_center(frame, "TRACKING STARTED", 0);
        frame_center(frame, "INSUFFICIENT ALTITUDE HISTORY FOR PROFILE", 0);
        frame_blank(frame);
        data_freshness_render(frame, flight, animation, time(NULL));
        return;
    }
    render_plot(frame, &plot, animation);
    frame_blank(frame);
    data_freshness_render(frame, flight, animation, time(NULL));
}
