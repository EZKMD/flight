#include "telemetry_history.h"

#include <string.h>

void telemetry_history_init(TelemetryHistory *history)
{
    memset(history, 0, sizeof(*history));
}

bool telemetry_sample_from_flight(const FlightState *flight, TelemetrySample *sample)
{
    memset(sample, 0, sizeof(*sample));
    if (!flight->metadata.telemetry_updated.available) return false;
    sample->timestamp = flight->metadata.telemetry_updated;
    sample->latitude = flight->position.latitude;
    sample->longitude = flight->position.longitude;
    sample->altitude_feet = flight->position.altitude_feet;
    sample->groundspeed_knots = flight->position.groundspeed_knots;
    sample->heading_degrees = flight->position.heading_degrees;
    sample->vertical_rate_fpm = flight->position.vertical_rate_fpm;
    sample->on_ground = flight->position.on_ground;
    sample->freshness = flight->status.telemetry_state;
    return true;
}

void telemetry_history_append(TelemetryHistory *history, const TelemetrySample *sample)
{
    size_t destination;
    if (history->count < TELEMETRY_HISTORY_CAPACITY) {
        destination = (history->start + history->count) % TELEMETRY_HISTORY_CAPACITY;
        history->count++;
    } else {
        destination = history->start;
        history->start = (history->start + 1) % TELEMETRY_HISTORY_CAPACITY;
    }
    history->samples[destination] = *sample;
}

const TelemetrySample *telemetry_history_at(const TelemetryHistory *history, size_t index)
{
    if (index >= history->count) return NULL;
    return &history->samples[(history->start + index) % TELEMETRY_HISTORY_CAPACITY];
}
