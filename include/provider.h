#ifndef FLIGHT_PROVIDER_H
#define FLIGHT_PROVIDER_H

#include <stdint.h>
#include "flight_resolver.h"
#include "mock_provider.h"
#include "telemetry_provider.h"

typedef ProviderResult (*DataProviderLoadFunction)(void *context, FlightState *state, time_t now);
typedef ProviderResult (*DataProviderRefreshFunction)(void *context, FlightState *state,
                                                       time_t now);

typedef struct {
    void *context;
    DataProviderLoadFunction load;
    DataProviderRefreshFunction refresh;
    uint64_t refresh_interval_ms;
    const char *name;
} FlightDataProvider;

typedef struct {
    FixtureKind fixture;
    const char *flight_number;
} MockDataProviderContext;

typedef struct {
    FlightResolver resolver;
    TelemetryProvider telemetry;
    FlightResolveRequest request;
    ResolvedFlight resolved;
    bool have_resolved;
    bool resolved_from_stale_cache;
    bool metadata_cache_hit;
    bool fallback_cache_used;
    bool have_last_telemetry;
    bool telemetry_attempted;
    TelemetrySnapshot last_telemetry;
    ProviderResult resolver_result;
    ProviderResult telemetry_result;
} LiveDataProviderContext;

void provider_init_mock(FlightDataProvider *provider, MockDataProviderContext *context,
                        FixtureKind fixture, const char *flight_number);
void provider_init_live(FlightDataProvider *provider, LiveDataProviderContext *context,
                        FlightResolver resolver, TelemetryProvider telemetry,
                        const char *flight_number, const char *date);

#endif
