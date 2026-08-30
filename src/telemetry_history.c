#include "telemetry_history.h"

#include <stdio.h>
#include <string.h>

void telemetry_history_init(TelemetryHistory *history)
{
    memset(history, 0, sizeof(*history));
    history->expected_interval_seconds = TELEMETRY_HISTORY_DEFAULT_INTERVAL_SECONDS;
}

void telemetry_history_set_interval(TelemetryHistory *history, unsigned int seconds)
{
    history->expected_interval_seconds = seconds > 0U ? seconds :
                                         TELEMETRY_HISTORY_DEFAULT_INTERVAL_SECONDS;
}

bool telemetry_history_set_occurrence(TelemetryHistory *history, const char *occurrence_id)
{
    unsigned int interval = history->expected_interval_seconds;
    if (occurrence_id == NULL || occurrence_id[0] == '\0') return false;
    if (strcmp(history->occurrence_id, occurrence_id) == 0) return false;
    telemetry_history_init(history);
    telemetry_history_set_interval(history, interval);
    (void)snprintf(history->occurrence_id, sizeof(history->occurrence_id), "%s", occurrence_id);
    return true;
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

bool telemetry_history_append(TelemetryHistory *history, const TelemetrySample *sample)
{
    const TelemetrySample *newest;
    size_t destination;
    if (sample == NULL || !sample->timestamp.available || sample->freshness != TELEMETRY_FRESH)
        return false;
    newest = history->count > 0U ? telemetry_history_at(history, history->count - 1U) : NULL;
    if (newest != NULL && sample->timestamp.value <= newest->timestamp.value) return false;
    if (history->count < TELEMETRY_HISTORY_CAPACITY) {
        destination = (history->start + history->count) % TELEMETRY_HISTORY_CAPACITY;
        history->count++;
    } else {
        destination = history->start;
        history->start = (history->start + 1) % TELEMETRY_HISTORY_CAPACITY;
    }
    history->samples[destination] = *sample;
    if (sample->altitude_feet.available &&
        sample->altitude_feet.value > history->maximum_altitude_feet)
        history->maximum_altitude_feet = sample->altitude_feet.value;
    return true;
}

bool telemetry_history_observe(TelemetryHistory *history, const char *occurrence_id,
                               const FlightState *flight)
{
    TelemetrySample sample;
    (void)telemetry_history_set_occurrence(history, occurrence_id);
    if (flight == NULL || flight->status.telemetry_state != TELEMETRY_FRESH) return false;
    if (!telemetry_sample_from_flight(flight, &sample)) return false;
    return telemetry_history_append(history, &sample);
}

const TelemetrySample *telemetry_history_at(const TelemetryHistory *history, size_t index)
{
    if (index >= history->count) return NULL;
    return &history->samples[(history->start + index) % TELEMETRY_HISTORY_CAPACITY];
}
