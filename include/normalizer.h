#ifndef NORMALIZER_H
#define NORMALIZER_H

#include <time.h>
#include "flight_resolver.h"
#include "telemetry_provider.h"

enum {
    NORMALIZER_CLIMB_ENTER_FPM = 500,
    NORMALIZER_CLIMB_EXIT_FPM = 300,
    NORMALIZER_DESCENT_ENTER_FPM = -500,
    NORMALIZER_DESCENT_EXIT_FPM = -300,
    NORMALIZER_TAXI_ENTER_KNOTS = 5,
    NORMALIZER_TAXI_EXIT_KNOTS = 3,
    NORMALIZER_TELEMETRY_FRESH_SECONDS = 30,
    NORMALIZER_TELEMETRY_RETAIN_SECONDS = 120
};

void normalizer_from_resolved(FlightState *state, const ResolvedFlight *resolved,
                              const char *resolver_name, time_t now);
void normalizer_apply_telemetry(FlightState *state, const TelemetrySnapshot *snapshot,
                                const char *telemetry_name, time_t now);
void normalizer_mark_telemetry_missing(FlightState *state, time_t now);
void normalizer_mark_telemetry_stale(FlightState *state, time_t now);
long normalizer_telemetry_age(const TelemetrySnapshot *snapshot, time_t now);

#endif
