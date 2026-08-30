#ifndef TELEMETRY_PROVIDER_H
#define TELEMETRY_PROVIDER_H

#include "flight_state.h"
#include "provider_result.h"

typedef struct {
    char icao24[8];
    char callsign[16];
} TelemetryRequest;

typedef struct {
    char icao24[8];
    char callsign[16];
    OptionalDouble latitude;
    OptionalDouble longitude;
    OptionalDouble barometric_altitude_m;
    OptionalDouble geometric_altitude_m;
    OptionalDouble velocity_mps;
    OptionalDouble heading_degrees;
    OptionalDouble vertical_rate_mps;
    OptionalBool on_ground;
    OptionalTime time_position;
    OptionalTime last_contact;
    OptionalTime received_at;
} TelemetrySnapshot;

bool telemetry_snapshot_matches(const TelemetryRequest *request,
                                const TelemetrySnapshot *snapshot);

typedef ProviderResult (*TelemetryFetchFunction)(void *context,
    const TelemetryRequest *request, TelemetrySnapshot *snapshot);

typedef struct {
    void *context;
    TelemetryFetchFunction fetch;
    const char *name;
} TelemetryProvider;

#endif
