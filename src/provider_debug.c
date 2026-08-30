#include "provider_debug.h"

#include <stdio.h>
#include <time.h>

static const char *telemetry_name(TelemetryState state)
{
    switch (state) {
        case TELEMETRY_FRESH: return "fresh";
        case TELEMETRY_STALE: return "stale";
        case TELEMETRY_UNAVAILABLE: return "unavailable";
        case TELEMETRY_NOT_EXPECTED: return "not expected";
    }
    return "unknown";
}

static const char *progress_name(ProgressSource source)
{
    switch (source) {
        case PROGRESS_LIVE_GEOSPATIAL: return "live geospatial";
        case PROGRESS_PROVIDER_REPORTED: return "provider reported";
        case PROGRESS_SCHEDULE_TIME: return "schedule time";
        case PROGRESS_LANDED: return "landed";
        case PROGRESS_UNAVAILABLE: return "unavailable";
    }
    return "unknown";
}

static const char *phase_source_name(PhaseSource source)
{
    switch (source) {
        case PHASE_TELEMETRY_DERIVED: return "telemetry derived";
        case PHASE_PROVIDER_CONFIRMED: return "provider confirmed";
        case PHASE_SCHEDULE_INFERRED: return "schedule inferred";
        case PHASE_UNAVAILABLE: return "unavailable";
    }
    return "unknown";
}

static const char *progress_fallback_name(ProgressFallbackReason reason)
{
    switch (reason) {
        case PROGRESS_FALLBACK_NONE: return "none";
        case PROGRESS_FALLBACK_MISSING_COORDINATES: return "missing coordinates";
        case PROGRESS_FALLBACK_ROUTE_TOO_SHORT: return "route too short";
        case PROGRESS_FALLBACK_OFF_ROUTE: return "telemetry position off expected route";
    }
    return "unknown";
}

void provider_debug_print(FILE *output, const LiveDataProviderContext *context,
                          const FlightState *state, ProviderResult final_result)
{
    const ResolvedFlight *resolved = &context->resolved;
    (void)fprintf(output, "provider result: %s%s%s\n",
                  provider_status_name(final_result.status),
                  final_result.message[0] != '\0' ? " — " : "",
                  final_result.message);
    (void)fprintf(output, "resolver result: %s%s%s\n",
                  provider_status_name(context->resolver_result.status),
                  context->resolver_result.message[0] != '\0' ? " — " : "",
                  context->resolver_result.message);
    (void)fprintf(output, "candidates: %d\nselected occurrence: %s\nselected leg: %s\n",
                  resolved->candidate_count,
                  resolved->occurrence_id[0] != '\0' ? resolved->occurrence_id : "unavailable",
                  resolved->selected_leg.leg_id[0] != '\0' ?
                      resolved->selected_leg.leg_id : "unavailable");
    if (state->origin.iata[0] != '\0' || state->destination.iata[0] != '\0')
        (void)fprintf(output, "route: %s / %s -> %s / %s\n",
                      state->origin.iata, state->origin.icao,
                      state->destination.iata, state->destination.icao);
    else (void)fputs("route: unavailable\n", output);
    (void)fprintf(output, "ICAO24: %s\n",
                  state->aircraft.icao24[0] != '\0' ? state->aircraft.icao24 : "unavailable");
    if (resolved->selection_reason[0] != '\0')
        (void)fprintf(output, "score: %d (%s)\n", resolved->selection_score,
                      resolved->selection_reason);
    else (void)fputs("score: unavailable\n", output);
    if (context->telemetry_attempted)
        (void)fprintf(output, "telemetry result: %s%s%s\n",
                      provider_status_name(context->telemetry_result.status),
                      context->telemetry_result.message[0] != '\0' ? " — " : "",
                      context->telemetry_result.message);
    else (void)fputs("telemetry result: not attempted\n", output);
    if (state->metadata.telemetry_updated.available) {
        long age = (long)(time(NULL) - state->metadata.telemetry_updated.value);
        (void)fprintf(output, "telemetry source timestamp: %ld\ntelemetry age: %lds\n",
                      (long)state->metadata.telemetry_updated.value, age);
    } else {
        (void)fputs("telemetry source timestamp: unavailable\n"
                    "telemetry age: unavailable\n", output);
    }
    (void)fprintf(output,
                  "display: %s\ntelemetry: %s\nphase: %s\nphase source: %s\n",
                  flight_display_label(state->status.display_state),
                  telemetry_name(state->status.telemetry_state),
                  flight_phase_label(state->status.phase),
                  phase_source_name(state->status.phase_source));
    if (state->journey.progress_available)
        (void)fprintf(output, "progress: %.1f%%\n", flight_progress_clamped(state) * 100.0);
    else (void)fputs("progress: unavailable\n", output);
    (void)fprintf(output, "progress source: %s\n",
                  progress_name(state->journey.progress_source));
    (void)fprintf(output, "progress fallback reason: %s\n",
                  progress_fallback_name(state->journey.progress_fallback_reason));
    (void)fprintf(output,
                  "metadata cache: %s\nresolved metadata source: %s\n"
                  "stale metadata fallback: %s\nflight-state fallback cache: not present\n",
                  context->metadata_cache_hit ? "hit" : "miss",
                  context->metadata_cache_hit ? "fresh metadata cache" :
                  context->fallback_cache_used ? "stale metadata cache" :
                  context->have_resolved ? "provider response" : "unavailable",
                  context->fallback_cache_used || state->metadata.stale_cache ? "used" : "none");
}
