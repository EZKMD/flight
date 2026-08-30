#ifndef TELEMETRY_HISTORY_H
#define TELEMETRY_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "flight_state.h"

#define TELEMETRY_HISTORY_CAPACITY 4096
#define TELEMETRY_HISTORY_ID_CAPACITY 192
#define TELEMETRY_HISTORY_DEFAULT_INTERVAL_SECONDS 300U

typedef struct {
    OptionalTime timestamp;
    OptionalDouble latitude;
    OptionalDouble longitude;
    OptionalInt altitude_feet;
    OptionalInt groundspeed_knots;
    OptionalInt heading_degrees;
    OptionalInt vertical_rate_fpm;
    OptionalBool on_ground;
    TelemetryState freshness;
} TelemetrySample;

typedef struct {
    TelemetrySample samples[TELEMETRY_HISTORY_CAPACITY];
    size_t start;
    size_t count;
    unsigned int expected_interval_seconds;
    int maximum_altitude_feet;
    char occurrence_id[TELEMETRY_HISTORY_ID_CAPACITY];
} TelemetryHistory;

void telemetry_history_init(TelemetryHistory *history);
void telemetry_history_set_interval(TelemetryHistory *history, unsigned int seconds);
bool telemetry_history_set_occurrence(TelemetryHistory *history, const char *occurrence_id);
bool telemetry_sample_from_flight(const FlightState *flight, TelemetrySample *sample);
bool telemetry_history_append(TelemetryHistory *history, const TelemetrySample *sample);
bool telemetry_history_observe(TelemetryHistory *history, const char *occurrence_id,
                               const FlightState *flight);
const TelemetrySample *telemetry_history_at(const TelemetryHistory *history, size_t index);

#endif
