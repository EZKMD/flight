#include "provider.h"

#include "cache.h"
#include "normalizer.h"

#include <stdio.h>
#include <string.h>

#define RESOLVER_CACHE_TTL_SECONDS (6L * 60L * 60L)
#define STALE_CACHE_TTL_SECONDS (30L * 24L * 60L * 60L)

static ProviderResult result(ProviderStatus status, const char *message)
{
    ProviderResult value = { .status = status, .retry_after_seconds = 0 };
    (void)snprintf(value.message, sizeof(value.message), "%s", message);
    return value;
}

static ProviderResult mock_load(void *opaque, FlightState *state, time_t now)
{
    MockDataProviderContext *context = opaque;
    mock_provider_load(state, context->fixture, context->flight_number, now);
    return result(PROVIDER_OK, "");
}

static ProviderResult mock_refresh(void *opaque, FlightState *state, time_t now)
{
    MockDataProviderContext *context = opaque;
    mock_provider_refresh(state, context->fixture, now);
    return result(PROVIDER_OK, "");
}

static void unavailable_state(FlightState *state, const char *flight_number,
                              const char *source)
{
    memset(state, 0, sizeof(*state));
    (void)snprintf(state->identity.flight_number, sizeof(state->identity.flight_number), "%s",
                   flight_number);
    state->status.phase = FLIGHT_PHASE_UNAVAILABLE;
    state->status.phase_source = PHASE_UNAVAILABLE;
    state->status.data_available = false;
    state->status.occurrence_confidence = OCCURRENCE_UNAVAILABLE;
    state->status.telemetry_state = TELEMETRY_UNAVAILABLE;
    state->status.display_state = DISPLAY_OFFLINE;
    state->journey.progress_source = PROGRESS_UNAVAILABLE;
    (void)snprintf(state->metadata.source, sizeof(state->metadata.source), "%s", source);
}

static ProviderResult fetch_telemetry(LiveDataProviderContext *context, FlightState *state,
                                      time_t now)
{
    TelemetryRequest request;
    TelemetrySnapshot snapshot;
    ProviderResult fetch_result;
    memset(&request, 0, sizeof(request));
    (void)snprintf(request.icao24, sizeof(request.icao24), "%s", state->aircraft.icao24);
    (void)snprintf(request.callsign, sizeof(request.callsign), "%s", state->aircraft.callsign);
    context->telemetry_attempted = true;
    fetch_result = context->telemetry.fetch(context->telemetry.context, &request, &snapshot);
    if (fetch_result.status == PROVIDER_OK) {
        context->last_telemetry = snapshot;
        context->have_last_telemetry = true;
        normalizer_apply_telemetry(state, &snapshot, context->telemetry.name, now);
        if (state->status.telemetry_state == TELEMETRY_STALE)
            fetch_result = result(PROVIDER_TELEMETRY_STALE,
                                  "matching telemetry exists but its source timestamp is stale");
    } else {
        if (context->have_last_telemetry &&
            normalizer_telemetry_age(&context->last_telemetry, now) <=
            NORMALIZER_TELEMETRY_RETAIN_SECONDS) {
            normalizer_apply_telemetry(state, &context->last_telemetry,
                                       context->telemetry.name, now);
            normalizer_mark_telemetry_stale(state, now);
            fetch_result = result(PROVIDER_TELEMETRY_STALE,
                                  "telemetry refresh failed; retaining recent stale telemetry");
        } else normalizer_mark_telemetry_missing(state, now);
        if (fetch_result.status == PROVIDER_NOT_FOUND)
            fetch_result = result(PROVIDER_TELEMETRY_UNAVAILABLE,
                                  "schedule resolved but no matching live telemetry exists");
    }
    context->telemetry_result = fetch_result;
    return fetch_result;
}

static ProviderResult live_load(void *opaque, FlightState *state, time_t now)
{
    LiveDataProviderContext *context = opaque;
    ProviderResult load_result;
    bool cached;
    context->request.now = now;
    context->metadata_cache_hit = false;
    context->fallback_cache_used = false;
    context->resolved_from_stale_cache = false;
    cached = cache_load_resolved(context->request.flight_number, context->request.date, now,
                                 RESOLVER_CACHE_TTL_SECONDS, &context->resolved);
    if (!cached) {
        context->resolver_result = context->resolver.resolve(context->resolver.context,
                                                              &context->request,
                                                              &context->resolved);
        if (context->resolver_result.status != PROVIDER_OK) {
            if (!cache_load_resolved(context->request.flight_number, context->request.date, now,
                                     STALE_CACHE_TTL_SECONDS, &context->resolved)) {
                unavailable_state(state, context->request.flight_number, context->resolver.name);
                return context->resolver_result;
            }
            context->resolved_from_stale_cache = true;
            context->fallback_cache_used = true;
        } else {
            (void)cache_store_resolved(context->request.flight_number, context->request.date,
                                       &context->resolved, now);
        }
    } else {
        context->metadata_cache_hit = true;
        context->resolver_result = result(PROVIDER_OK, "resolved occurrence metadata available");
    }
    context->have_resolved = true;
    normalizer_from_resolved(state, &context->resolved, context->resolver.name, now);
    state->metadata.stale_cache = context->resolved_from_stale_cache;
    load_result = fetch_telemetry(context, state, now);
    if (context->resolved_from_stale_cache && load_result.status != PROVIDER_OK) {
        state->status.stale = true;
        state->status.display_state = DISPLAY_STALE;
        return result(PROVIDER_STALE_CACHE, "using stale cached occurrence metadata");
    }
    if (load_result.status == PROVIDER_TELEMETRY_UNAVAILABLE ||
        load_result.status == PROVIDER_TELEMETRY_STALE)
        return result(PROVIDER_PARTIAL_SUCCESS, load_result.message);
    return load_result;
}

static ProviderResult live_refresh(void *opaque, FlightState *state, time_t now)
{
    LiveDataProviderContext *context = opaque;
    if (!context->have_resolved) return live_load(opaque, state, now);
    return fetch_telemetry(context, state, now);
}

void provider_init_mock(FlightDataProvider *provider, MockDataProviderContext *context,
                        FixtureKind fixture, const char *flight_number)
{
    context->fixture = fixture;
    context->flight_number = flight_number;
    provider->context = context;
    provider->load = mock_load;
    provider->refresh = mock_refresh;
    provider->refresh_interval_ms = UINT64_C(15000);
    provider->name = "MOCK";
}

void provider_init_live(FlightDataProvider *provider, LiveDataProviderContext *context,
                        FlightResolver resolver, TelemetryProvider telemetry,
                        const char *flight_number, const char *date)
{
    memset(context, 0, sizeof(*context));
    context->resolver = resolver;
    context->telemetry = telemetry;
    context->request.flight_number = flight_number;
    context->request.date = date;
    provider->context = context;
    provider->load = live_load;
    provider->refresh = live_refresh;
    provider->refresh_interval_ms = UINT64_C(300000);
    provider->name = "LIVE";
}
