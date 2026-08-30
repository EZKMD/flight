#include "screen_components.h"

#include <stdio.h>

static const char *text_or(const char *text, const char *fallback)
{
    return text[0] != '\0' ? text : fallback;
}

static void format_minutes(char *buffer, size_t capacity, OptionalInt minutes)
{
    if (!minutes.available) {
        (void)snprintf(buffer, capacity, "--");
        return;
    }
    (void)snprintf(buffer, capacity, "%dH %02dM", minutes.value / 60,
                   minutes.value % 60);
}

static void format_airport(char *buffer, size_t capacity, const AirportState *airport)
{
    if (airport->iata[0] != '\0' && airport->icao[0] != '\0')
        (void)snprintf(buffer, capacity, "%s / %s", airport->iata, airport->icao);
    else if (airport->iata[0] != '\0')
        (void)snprintf(buffer, capacity, "%s", airport->iata);
    else if (airport->icao[0] != '\0')
        (void)snprintf(buffer, capacity, "%s", airport->icao);
    else (void)snprintf(buffer, capacity, "--");
}

void flight_header_render(Frame *frame, const FlightState *flight,
                          const AnimationState *animation)
{
    char left[128];
    char right[128];
    (void)snprintf(right, sizeof(right), "%s %s",
                   flight_display_label(flight->status.display_state), animation->heartbeat);
    frame_sides(frame, text_or(flight->identity.flight_number, "--"), right);
    frame_blank(frame);
    (void)snprintf(left, sizeof(left), "%s · %s",
                   text_or(flight->identity.airline_name, "UNKNOWN"),
                   text_or(flight->aircraft.model, "UNKNOWN"));
    frame_sides(frame, left, text_or(flight->aircraft.registration, "--"));
}

void route_progress_render(Frame *frame, const FlightState *flight)
{
    char left[64];
    char right[64];
    char bar[FRAME_LINE_CAPACITY];
    char progress_text[32];
    const char *departure = text_or(flight->timing.departure_display, "--:--");
    const char *arrival = text_or(flight->timing.arrival_display, "--:--");
    double progress = flight_progress_clamped(flight);
    int bar_width = frame->width - 13;
    int position;
    int used = 0;
    int index;

    format_airport(left, sizeof(left), &flight->origin);
    format_airport(right, sizeof(right), &flight->destination);
    frame_sides(frame, left, right);
    frame_blank(frame);

    if (bar_width < 8) bar_width = 8;
    if (bar_width > 72) bar_width = 72;
    position = (int)(progress * (double)(bar_width - 1));
    used += snprintf(bar + used, sizeof(bar) - (size_t)used, "%s ", departure);
    for (index = 0; index < bar_width; index++) {
        const char *symbol = index == position ? "✈" :
                             (index == bar_width - 1 ? "○" :
                             (index == 0 ? "●" : "─"));
        used += snprintf(bar + used, sizeof(bar) - (size_t)used, "%s", symbol);
        if (used >= (int)sizeof(bar)) break;
    }
    (void)snprintf(bar + used, sizeof(bar) - (size_t)used, " %s", arrival);
    frame_center(frame, bar, 0);
    if (flight->journey.progress_available)
        (void)snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress * 100.0);
    else (void)snprintf(progress_text, sizeof(progress_text), "--");
    frame_add(frame, progress_text);
}

void flight_metrics_render(Frame *frame, const FlightState *flight,
                           const Layout *layout)
{
    char altitude[32];
    char speed[32];
    char heading[32];
    char telemetry[FRAME_LINE_CAPACITY];
    char elapsed[32];
    char remaining[32];
    int gap;

    if (flight->position.flight_level.available)
        (void)snprintf(altitude, sizeof(altitude), "FL%d", flight->position.flight_level.value);
    else (void)snprintf(altitude, sizeof(altitude), "FL---");
    if (flight->position.groundspeed_knots.available)
        (void)snprintf(speed, sizeof(speed), "%d KT", flight->position.groundspeed_knots.value);
    else (void)snprintf(speed, sizeof(speed), "-- KT");
    if (flight->position.heading_degrees.available)
        (void)snprintf(heading, sizeof(heading), "%d° %s",
                       flight->position.heading_degrees.value,
                       text_or(flight->position.heading_compass, "--"));
    else (void)snprintf(heading, sizeof(heading), "---° --");

    gap = (frame->width - frame_text_width(altitude) - frame_text_width(speed) -
           frame_text_width(heading)) / 2;
    if (gap < 1) gap = 1;
    (void)snprintf(telemetry, sizeof(telemetry), "%s%*s%s%*s%s",
                   altitude, gap, "", speed, gap, "", heading);
    frame_center(frame, telemetry, 0);

    if (!layout->show_times) return;
    frame_blank(frame);
    format_minutes(elapsed, sizeof(elapsed), flight->journey.airborne_minutes);
    format_minutes(remaining, sizeof(remaining), flight->journey.remaining_minutes);
    frame_sides(frame, elapsed, remaining);
    if (layout->show_telemetry_labels) frame_sides(frame, "AIRBORNE", "REMAINING");
}

void flight_status_render(Frame *frame, const FlightState *flight)
{
    frame_center(frame, flight_phase_label(flight->status.phase), 0);
}

void data_freshness_render(Frame *frame, const FlightState *flight,
                           const AnimationState *animation, time_t now)
{
    char text[128];
    long age;
    if (!flight->status.data_available) {
        (void)snprintf(text, sizeof(text), "DATA UNAVAILABLE · %s", animation->clock_text);
    } else if (flight->status.telemetry_state == TELEMETRY_STALE) {
        (void)snprintf(text, sizeof(text), "DATA STALE · %s", animation->clock_text);
    } else if (flight->status.telemetry_state == TELEMETRY_UNAVAILABLE) {
        (void)snprintf(text, sizeof(text), "NO TELEMETRY · %s", animation->clock_text);
    } else if (flight->status.telemetry_state == TELEMETRY_NOT_EXPECTED) {
        (void)snprintf(text, sizeof(text), "SCHEDULE DATA · %s", animation->clock_text);
    } else if (!flight->metadata.telemetry_updated.available) {
        (void)snprintf(text, sizeof(text), "DATA UNAVAILABLE · %s", animation->clock_text);
    } else {
        age = (long)(now - flight->metadata.telemetry_updated.value);
        if (age < 0) age = 0;
        (void)snprintf(text, sizeof(text), "DATA %lds AGO · %s", age,
                       animation->clock_text);
    }
    frame_center(frame, text, 0);
}

void compact_summary_render(Frame *frame, const FlightState *flight,
                            const AnimationState *animation, time_t now)
{
    char right[64];
    char text[128];
    (void)snprintf(right, sizeof(right), "%s %s",
                   flight_display_label(flight->status.display_state), animation->heartbeat);
    frame_sides(frame, text_or(flight->identity.flight_number, "--"), right);
    frame_blank(frame);
    (void)snprintf(text, sizeof(text), "%s → %s", text_or(flight->origin.iata, "--"),
                   text_or(flight->destination.iata, "--"));
    frame_center(frame, text, 0);
    if (flight->journey.progress_available && flight->position.flight_level.available)
        (void)snprintf(text, sizeof(text), "%.1f%%  FL%d",
                       flight_progress_clamped(flight) * 100.0,
                       flight->position.flight_level.value);
    else if (flight->journey.progress_available)
        (void)snprintf(text, sizeof(text), "%.1f%%  FL---",
                        flight_progress_clamped(flight) * 100.0);
    else (void)snprintf(text, sizeof(text), "--  FL---");
    frame_center(frame, text, 0);
    frame_blank(frame);
    flight_status_render(frame, flight);
    if (flight->position.groundspeed_knots.available &&
        flight->position.heading_degrees.available)
        (void)snprintf(text, sizeof(text), "%d KT · %d° %s",
                       flight->position.groundspeed_knots.value,
                       flight->position.heading_degrees.value,
                       text_or(flight->position.heading_compass, "--"));
    else (void)snprintf(text, sizeof(text), "-- KT · ---° --");
    frame_center(frame, text, 0);
    frame_blank(frame);
    data_freshness_render(frame, flight, animation, now);
}
