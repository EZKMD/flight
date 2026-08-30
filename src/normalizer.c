#include "normalizer.h"
#include "geospatial_progress.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define METERS_TO_FEET 3.280839895
#define MPS_TO_KNOTS 1.943844492
#define MPS_TO_FPM 196.8503937
#define PRE_DEPARTURE_WINDOW_SECONDS (2 * 60 * 60)

static OptionalTime departure_time(const FlightTiming *timing, bool allow_schedule)
{
    if (timing->actual_departure.available) return timing->actual_departure;
    if (timing->estimated_departure.available) return timing->estimated_departure;
    return allow_schedule ? timing->scheduled_departure : (OptionalTime){ false, 0 };
}

static OptionalTime arrival_time(const FlightTiming *timing)
{
    if (timing->actual_arrival.available) return timing->actual_arrival;
    if (timing->estimated_arrival.available) return timing->estimated_arrival;
    return timing->scheduled_arrival;
}

static void heading_compass(char output[8], int heading)
{
    static const char *const points[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int normalized = heading % 360;
    int index;
    if (normalized < 0) normalized += 360;
    index = ((normalized + 22) / 45) % 8;
    (void)snprintf(output, 8, "%s", points[index]);
}

static void derive_duration(FlightState *state, time_t now)
{
    bool landed = state->status.phase == FLIGHT_PHASE_LANDED;
    OptionalTime departure = departure_time(&state->timing, landed);
    OptionalTime arrival = arrival_time(&state->timing);
    state->journey.airborne_minutes.available = false;
    state->journey.remaining_minutes.available = false;
    if (landed) {
        if (departure.available && arrival.available && arrival.value >= departure.value)
            state->journey.airborne_minutes = (OptionalInt){ true,
                (int)((arrival.value - departure.value) / 60) };
        state->journey.remaining_minutes = (OptionalInt){ true, 0 };
        return;
    }
    if (state->timing.actual_departure.available && now >= state->timing.actual_departure.value)
        state->journey.airborne_minutes = (OptionalInt){ true,
            (int)((now - state->timing.actual_departure.value) / 60) };
    if (arrival.available && arrival.value >= now)
        state->journey.remaining_minutes = (OptionalInt){ true,
            (int)((arrival.value - now) / 60) };
}

static void derive_progress(FlightState *state, time_t now, bool allow_live_position)
{
    OptionalTime departure = departure_time(&state->timing, true);
    OptionalTime arrival = arrival_time(&state->timing);
    state->journey.progress = 0.0;
    state->journey.progress_available = false;
    state->journey.progress_source = PROGRESS_UNAVAILABLE;
    state->journey.progress_fallback_reason = PROGRESS_FALLBACK_NONE;
    if (state->status.phase == FLIGHT_PHASE_LANDED) {
        state->journey.progress = 1.0;
        state->journey.progress_available = true;
        state->journey.progress_source = PROGRESS_LANDED;
        return;
    }
    if (allow_live_position && state->origin.latitude.available &&
        state->origin.longitude.available && state->destination.latitude.available &&
        state->destination.longitude.available && state->position.latitude.available &&
        state->position.longitude.available) {
        double live_progress;
        double cross_track_km;
        GeospatialProgressResult result =
            geospatial_progress(state->origin.latitude.value, state->origin.longitude.value,
                                state->destination.latitude.value,
                                state->destination.longitude.value,
                                state->position.latitude.value, state->position.longitude.value,
                                &live_progress, &cross_track_km);
        if (result == GEO_PROGRESS_VALID) {
            state->journey.progress = live_progress;
            state->journey.progress_available = true;
            state->journey.progress_source = PROGRESS_LIVE_GEOSPATIAL;
        } else if (result == GEO_PROGRESS_OFF_ROUTE)
            state->journey.progress_fallback_reason = PROGRESS_FALLBACK_OFF_ROUTE;
        else state->journey.progress_fallback_reason = PROGRESS_FALLBACK_ROUTE_TOO_SHORT;
    } else if (allow_live_position) {
        state->journey.progress_fallback_reason = PROGRESS_FALLBACK_MISSING_COORDINATES;
    }
    if (!state->journey.progress_available && departure.available && arrival.available &&
        arrival.value > departure.value && now >= departure.value) {
        state->journey.progress = (double)(now - departure.value) /
                                  (double)(arrival.value - departure.value);
        state->journey.progress_available = true;
        state->journey.progress_source = PROGRESS_SCHEDULE_TIME;
    }
    state->journey.progress = flight_progress_clamped(state);
}

static void phase_from_provider(FlightState *state, const char *provider_status, time_t now)
{
    OptionalTime departure = departure_time(&state->timing, true);
    if (strcmp(provider_status, "landed") == 0 || state->timing.actual_arrival.available) {
        state->status.phase = FLIGHT_PHASE_LANDED;
        state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
    } else if (state->status.delayed && !state->timing.actual_departure.available) {
        state->status.phase = FLIGHT_PHASE_DELAYED;
        state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
    } else if (strcmp(provider_status, "active") == 0 || strcmp(provider_status, "en-route") == 0 ||
               strcmp(provider_status, "en_route") == 0) {
        state->status.phase = FLIGHT_PHASE_AIRBORNE;
        state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
    } else if (departure.available && departure.value > now &&
               departure.value - now <= PRE_DEPARTURE_WINDOW_SECONDS) {
        state->status.phase = FLIGHT_PHASE_PRE_DEPARTURE;
        state->status.phase_source = PHASE_SCHEDULE_INFERRED;
    } else if (strcmp(provider_status, "scheduled") == 0 ||
               (departure.available && departure.value > now)) {
        state->status.phase = FLIGHT_PHASE_SCHEDULED;
        state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
    } else {
        state->status.phase = FLIGHT_PHASE_UNAVAILABLE;
        state->status.phase_source = PHASE_UNAVAILABLE;
    }
}

static void phase_from_fresh_telemetry(FlightState *state)
{
    FlightPhase previous = state->status.phase;
    if (state->position.on_ground.available && state->position.on_ground.value) {
        if (state->timing.actual_arrival.available || state->status.phase == FLIGHT_PHASE_LANDED)
            state->status.phase = FLIGHT_PHASE_LANDED;
        else if ((state->position.groundspeed_knots.available &&
                  state->position.groundspeed_knots.value > NORMALIZER_TAXI_ENTER_KNOTS) ||
                 (previous == FLIGHT_PHASE_TAXIING_DEPARTURE &&
                  state->position.groundspeed_knots.available &&
                  state->position.groundspeed_knots.value >= NORMALIZER_TAXI_EXIT_KNOTS) ||
                 state->timing.actual_departure.available)
            state->status.phase = FLIGHT_PHASE_TAXIING_DEPARTURE;
        else state->status.phase = FLIGHT_PHASE_PRE_DEPARTURE;
    } else if (state->position.on_ground.available && !state->position.on_ground.value) {
        if (state->position.vertical_rate_fpm.available &&
            (state->position.vertical_rate_fpm.value > NORMALIZER_CLIMB_ENTER_FPM ||
             (previous == FLIGHT_PHASE_CLIMBING &&
              state->position.vertical_rate_fpm.value >= NORMALIZER_CLIMB_EXIT_FPM)))
            state->status.phase = FLIGHT_PHASE_CLIMBING;
        else if (state->position.vertical_rate_fpm.available &&
                 (state->position.vertical_rate_fpm.value < NORMALIZER_DESCENT_ENTER_FPM ||
                  (previous == FLIGHT_PHASE_DESCENDING &&
                   state->position.vertical_rate_fpm.value <= NORMALIZER_DESCENT_EXIT_FPM)))
            state->status.phase = FLIGHT_PHASE_DESCENDING;
        else if (state->position.flight_level.available &&
                 state->position.vertical_rate_fpm.available)
            state->status.phase = FLIGHT_PHASE_CRUISING;
        else state->status.phase = FLIGHT_PHASE_AIRBORNE;
    } else state->status.phase = FLIGHT_PHASE_UNAVAILABLE;
    state->status.phase_source = PHASE_TELEMETRY_DERIVED;
}

static void remove_precise_motion_phase(FlightState *state)
{
    if (state->status.phase == FLIGHT_PHASE_CLIMBING ||
        state->status.phase == FLIGHT_PHASE_CRUISING ||
        state->status.phase == FLIGHT_PHASE_DESCENDING ||
        state->status.phase == FLIGHT_PHASE_TAXIING_DEPARTURE) {
        state->status.phase = FLIGHT_PHASE_AIRBORNE;
        state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
    }
}

void normalizer_from_resolved(FlightState *state, const ResolvedFlight *resolved,
                              const char *resolver_name, time_t now)
{
    const ResolvedFlightLeg *leg = &resolved->selected_leg;
    memset(state, 0, sizeof(*state));
    state->identity = resolved->identity;
    state->aircraft = leg->aircraft;
    state->origin = leg->origin;
    state->destination = leg->destination;
    state->timing = leg->timing;
    state->status.data_available = true;
    state->status.occurrence_confidence = resolved->confidence;
    state->status.delayed = leg->delay_minutes.available && leg->delay_minutes.value > 0;
    state->status.telemetry_state = TELEMETRY_UNAVAILABLE;
    phase_from_provider(state, leg->provider_status, now);
    state->status.display_state = state->status.phase == FLIGHT_PHASE_SCHEDULED ||
                                  state->status.phase == FLIGHT_PHASE_PRE_DEPARTURE ?
                                  DISPLAY_SCHEDULED : DISPLAY_TRACKING;
    state->metadata.last_updated = leg->updated;
    (void)snprintf(state->metadata.source, sizeof(state->metadata.source), "%s", resolver_name);
    derive_duration(state, now);
    derive_progress(state, now, false);
}

void normalizer_apply_telemetry(FlightState *state, const TelemetrySnapshot *snapshot,
                                const char *telemetry_name, time_t now)
{
    OptionalDouble altitude = snapshot->barometric_altitude_m.available ?
                              snapshot->barometric_altitude_m : snapshot->geometric_altitude_m;
    OptionalTime source_time = snapshot->time_position.available ? snapshot->time_position :
                               snapshot->last_contact;
    long age = normalizer_telemetry_age(snapshot, now);
    state->position.latitude = snapshot->latitude;
    state->position.longitude = snapshot->longitude;
    state->position.on_ground = snapshot->on_ground;
    state->position.last_position = snapshot->time_position;
    if (altitude.available) {
        int feet = (int)lround(altitude.value * METERS_TO_FEET);
        state->position.altitude_feet = (OptionalInt){ true, feet };
        state->position.flight_level = (OptionalInt){ true, (int)lround((double)feet / 100.0) };
    }
    if (snapshot->velocity_mps.available)
        state->position.groundspeed_knots = (OptionalInt){ true,
            (int)lround(snapshot->velocity_mps.value * MPS_TO_KNOTS) };
    if (snapshot->heading_degrees.available) {
        int heading = (int)lround(snapshot->heading_degrees.value);
        state->position.heading_degrees = (OptionalInt){ true, heading };
        heading_compass(state->position.heading_compass, heading);
    }
    if (snapshot->vertical_rate_mps.available)
        state->position.vertical_rate_fpm = (OptionalInt){ true,
            (int)lround(snapshot->vertical_rate_mps.value * MPS_TO_FPM) };
    state->metadata.telemetry_updated = source_time;
    state->metadata.telemetry_received = snapshot->received_at;
    state->metadata.last_updated = source_time;
    (void)snprintf(state->metadata.source, sizeof(state->metadata.source), "AIRLABS+%s",
                   telemetry_name);
    if (age >= 0 && age <= NORMALIZER_TELEMETRY_FRESH_SECONDS) {
        state->status.telemetry_state = TELEMETRY_FRESH;
        state->status.display_state = DISPLAY_LIVE;
        state->status.stale = false;
        phase_from_fresh_telemetry(state);
    } else {
        state->status.telemetry_state = TELEMETRY_STALE;
        state->status.display_state = DISPLAY_STALE;
        state->status.stale = true;
        remove_precise_motion_phase(state);
    }
    derive_duration(state, now);
    derive_progress(state, now, state->status.telemetry_state == TELEMETRY_FRESH);
}

long normalizer_telemetry_age(const TelemetrySnapshot *snapshot, time_t now)
{
    OptionalTime source_time = snapshot->time_position.available ? snapshot->time_position :
                               snapshot->last_contact;
    if (!source_time.available || now < source_time.value) return LONG_MAX;
    return (long)(now - source_time.value);
}

void normalizer_mark_telemetry_stale(FlightState *state, time_t now)
{
    state->status.telemetry_state = TELEMETRY_STALE;
    state->status.display_state = DISPLAY_STALE;
    state->status.stale = true;
    remove_precise_motion_phase(state);
    derive_duration(state, now);
    derive_progress(state, now, false);
}

void normalizer_mark_telemetry_missing(FlightState *state, time_t now)
{
    remove_precise_motion_phase(state);
    state->position.latitude.available = false;
    state->position.longitude.available = false;
    state->position.altitude_feet.available = false;
    state->position.flight_level.available = false;
    state->position.groundspeed_knots.available = false;
    state->position.heading_degrees.available = false;
    state->position.vertical_rate_fpm.available = false;
    state->position.on_ground.available = false;
    if (state->status.phase == FLIGHT_PHASE_SCHEDULED ||
        state->status.phase == FLIGHT_PHASE_PRE_DEPARTURE) {
        state->status.telemetry_state = TELEMETRY_NOT_EXPECTED;
        state->status.display_state = DISPLAY_SCHEDULED;
    } else {
        state->status.telemetry_state = TELEMETRY_UNAVAILABLE;
        state->status.display_state = state->metadata.stale_cache ? DISPLAY_STALE : DISPLAY_TRACKING;
    }
    state->status.stale = state->metadata.stale_cache;
    derive_duration(state, now);
    derive_progress(state, now, false);
}
