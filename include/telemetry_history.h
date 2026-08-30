#ifndef TELEMETRY_HISTORY_H
#define TELEMETRY_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "flight_state.h"

#define TELEMETRY_HISTORY_CAPACITY 256

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
} TelemetryHistory;

void telemetry_history_init(TelemetryHistory *history);
bool telemetry_sample_from_flight(const FlightState *flight, TelemetrySample *sample);
void telemetry_history_append(TelemetryHistory *history, const TelemetrySample *sample);
const TelemetrySample *telemetry_history_at(const TelemetryHistory *history, size_t index);

#endif
