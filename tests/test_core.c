#include "altitude_profile.h"
#include "airport_reference.h"
#include "coastline_data.h"
#include "flight_candidate.h"
#include "flight_designator.h"
#include "geospatial_progress.h"
#include "geographic_map_poc.h"
#include "json.h"
#include "input.h"
#include "map_geometry.h"
#include "map_raster.h"
#include "normalizer.h"
#include "provider.h"
#include "route_map.h"
#include "runtime.h"
#include "subcell_canvas.h"
#include "telemetry_provider.h"
#include "telemetry_history.h"
#include "visual_viewport.h"
#include "version.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const time_t test_now = (time_t)2000000000;

static ResolvedFlight resolved_flight(const char *status, time_t departure, time_t arrival,
                                      const char *date, const char *origin, const char *destination)
{
    ResolvedFlight flight;
    memset(&flight, 0, sizeof(flight));
    (void)snprintf(flight.identity.flight_number, sizeof(flight.identity.flight_number), "QF1");
    (void)snprintf(flight.identity.airline_code, sizeof(flight.identity.airline_code), "QF");
    (void)snprintf(flight.selected_leg.origin.iata, sizeof(flight.selected_leg.origin.iata), "%s",
                   origin);
    (void)snprintf(flight.selected_leg.destination.iata,
                   sizeof(flight.selected_leg.destination.iata), "%s", destination);
    (void)airport_reference_normalize(&flight.selected_leg.origin);
    (void)airport_reference_normalize(&flight.selected_leg.destination);
    flight.selected_leg.timing.scheduled_departure = (OptionalTime){ true, departure };
    flight.selected_leg.timing.scheduled_arrival = (OptionalTime){ true, arrival };
    (void)snprintf(flight.selected_leg.departure_date,
                   sizeof(flight.selected_leg.departure_date), "%s", date);
    (void)snprintf(flight.selected_leg.provider_status,
                   sizeof(flight.selected_leg.provider_status), "%s", status);
    (void)snprintf(flight.selected_leg.leg_id, sizeof(flight.selected_leg.leg_id), "%s-%s-%ld",
                   origin, destination, (long)departure);
    flight.confidence = OCCURRENCE_CONFIRMED;
    return flight;
}

static FlightCandidate candidate(const char *status, time_t departure, time_t arrival,
                                 const char *date, const char *origin, const char *destination)
{
    FlightCandidate value;
    memset(&value, 0, sizeof(value));
    value.flight = resolved_flight(status, departure, arrival, date, origin, destination);
    value.airport_consistent = true;
    return value;
}

static TelemetrySnapshot telemetry(time_t source_time, bool on_ground, double altitude_m,
                                   double speed_mps, double vertical_mps)
{
    TelemetrySnapshot value;
    memset(&value, 0, sizeof(value));
    (void)snprintf(value.icao24, sizeof(value.icao24), "7c806c");
    (void)snprintf(value.callsign, sizeof(value.callsign), "QFA1");
    value.latitude = (OptionalDouble){ true, -10.0 };
    value.longitude = (OptionalDouble){ true, 130.0 };
    value.barometric_altitude_m = (OptionalDouble){ true, altitude_m };
    value.velocity_mps = (OptionalDouble){ true, speed_mps };
    value.heading_degrees = (OptionalDouble){ true, 302.0 };
    value.vertical_rate_mps = (OptionalDouble){ true, vertical_mps };
    value.on_ground = (OptionalBool){ true, on_ground };
    value.time_position = (OptionalTime){ true, source_time };
    value.received_at = (OptionalTime){ true, test_now };
    return value;
}

static void test_json_state_vector(void)
{
    const char *text =
        "{\"time\":1000,\"states\":[[\"7c806c\",\"QFA9   \",\"Australia\",990,"
        "999,76.4,22.2,11582.4,false,251.56,302.0,0.0,null,11600.0,null,false,0]]}";
    JsonToken tokens[128];
    JsonDocument document;
    int row;
    double value;
    assert(json_parse(&document, text, tokens, 128));
    row = json_array_get(&document,
                         json_object_get(&document, json_root(&document), "states"), 0);
    assert(row >= 0);
    assert(json_double(&document, json_array_get(&document, row, 7), &value));
    assert(fabs(value - 11582.4) < 0.01);
    assert(json_is_null(&document, json_array_get(&document, row, 12)));
}

static void test_airport_validation(void)
{
    AirportState airport;
    assert(airport_reference_pair_valid("SYD", "YSSY"));
    assert(airport_reference_pair_valid("SIN", "WSSS"));
    assert(airport_reference_pair_valid("LHR", "EGLL"));
    assert(!airport_reference_pair_valid("SYD", "WSSS"));
    assert(airport_reference_pair_valid("HND", "RJTT"));
    assert(airport_reference_pair_valid("CDG", "LFPG"));
    assert(airport_reference_pair_valid("JNB", "FAOR"));
    assert(airport_reference_pair_valid("GRU", "SBGR"));
    assert(airport_reference_pair_valid("AKL", "NZAA"));
    assert(!airport_reference_pair_valid("HND", "LFPG"));
    memset(&airport, 0, sizeof(airport));
    (void)snprintf(airport.iata, sizeof(airport.iata), "SYD");
    (void)snprintf(airport.icao, sizeof(airport.icao), "WSSS");
    assert(airport_reference_normalize(&airport) == AIRPORT_MISMATCH);
    assert(strcmp(airport.iata, "SYD") == 0);
    assert(airport.icao[0] == '\0');
}

static void test_designator_validation(void)
{
    assert(flight_designator_classify("QF1") == DESIGNATOR_IATA_SUPPORTED);
    assert(flight_designator_classify("BA281") == DESIGNATOR_IATA_SUPPORTED);
    assert(flight_designator_classify("U2250") == DESIGNATOR_IATA_SUPPORTED);
    assert(flight_designator_classify("TOM7795") == DESIGNATOR_PLAUSIBLE_UNSUPPORTED);
    assert(flight_designator_classify("QF") == DESIGNATOR_MALFORMED);
    assert(flight_designator_classify("QF-1") == DESIGNATOR_MALFORMED);
    assert(flight_designator_classify("1QF9") == DESIGNATOR_MALFORMED);
}

static void test_occurrence_selection(void)
{
    FlightCandidateSet set;
    FlightResolveRequest request = { "QF1", "", test_now };
    ResolvedFlight selected;
    ProviderResult result;
    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("landed", test_now - 7200, test_now - 3600,
                                       "2033-05-18", "SYD", "SIN");
    set.items[set.count++] = candidate("active", test_now - 1800, test_now + 1800,
                                       "2033-05-18", "SYD", "SIN");
    result = flight_candidate_select(&set, &request, &selected);
    assert(result.status == PROVIDER_OK);
    assert(strcmp(selected.selected_leg.provider_status, "active") == 0);

    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("landed", test_now - 86400, test_now - 80000,
                                       "2033-05-17", "SYD", "SIN");
    set.items[set.count++] = candidate("scheduled", test_now + 3600, test_now + 7200,
                                       "2033-05-18", "SYD", "SIN");
    assert(flight_candidate_select(&set, &request, &selected).status == PROVIDER_OK);
    assert(strcmp(selected.selected_leg.provider_status, "scheduled") == 0);

    request.date = "2033-05-19";
    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("active", test_now - 300, test_now + 3000,
                                       "2033-05-18", "SYD", "SIN");
    set.items[set.count++] = candidate("scheduled", test_now + 4000, test_now + 8000,
                                       "2033-05-19", "SYD", "SIN");
    assert(flight_candidate_select(&set, &request, &selected).status == PROVIDER_OK);
    assert(strcmp(selected.selected_leg.departure_date, "2033-05-19") == 0);

    /* Date strings, not local wall-clock assumptions, constrain a UTC boundary. */
    request.date = "2033-05-20";
    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("scheduled", test_now + 60, test_now + 3600,
                                       "2033-05-19", "SYD", "SIN");
    set.items[set.count++] = candidate("scheduled", test_now + 120, test_now + 3700,
                                       "2033-05-20", "SYD", "SIN");
    assert(flight_candidate_select(&set, &request, &selected).status == PROVIDER_OK);
    assert(strcmp(selected.selected_leg.departure_date, "2033-05-20") == 0);

    request.date = "";
    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("landed", test_now - 3600, test_now - 300,
                                       "2033-05-18", "SYD", "SIN");
    set.items[set.count++] = candidate("scheduled", test_now + 900, test_now + 4500,
                                       "2033-05-18", "SYD", "SIN");
    assert(flight_candidate_select(&set, &request, &selected).status == PROVIDER_OK);
    assert(strcmp(selected.selected_leg.provider_status, "scheduled") == 0);

    memset(&set, 0, sizeof(set));
    set.items[set.count++] = candidate("scheduled", test_now + 900, test_now + 4500,
                                       "2033-05-18", "SYD", "SIN");
    set.items[set.count++] = candidate("scheduled", test_now + 900, test_now + 4500,
                                       "2033-05-18", "SYD", "LHR");
    assert(flight_candidate_select(&set, &request, &selected).status ==
           PROVIDER_OCCURRENCE_AMBIGUOUS);
}

static void test_multileg_isolation(void)
{
    ResolvedFlight first = resolved_flight("scheduled", test_now + 100,
                                           test_now + 1000, "2033-05-18", "SYD", "SIN");
    ResolvedFlight second = resolved_flight("scheduled", test_now + 2000,
                                            test_now + 5000, "2033-05-18", "SIN", "LHR");
    assert(strcmp(first.selected_leg.origin.icao, "YSSY") == 0);
    assert(strcmp(first.selected_leg.destination.icao, "WSSS") == 0);
    assert(strcmp(second.selected_leg.origin.icao, "WSSS") == 0);
    assert(strcmp(second.selected_leg.destination.icao, "EGLL") == 0);
}

static FlightState normalized(const char *status, time_t departure, time_t arrival)
{
    ResolvedFlight resolved = resolved_flight(status, departure, arrival, "2033-05-18",
                                              "SYD", "SIN");
    FlightState state;
    normalizer_from_resolved(&state, &resolved, "TEST", test_now);
    return state;
}

static void test_phase_inference(void)
{
    FlightState state = normalized("scheduled", test_now + 10800, test_now + 20000);
    TelemetrySnapshot sample;
    assert(state.status.phase == FLIGHT_PHASE_SCHEDULED);
    state = normalized("scheduled", test_now + 3600, test_now + 10000);
    assert(state.status.phase == FLIGHT_PHASE_PRE_DEPARTURE);

    state = normalized("active", test_now - 1000, test_now + 10000);
    state.timing.actual_departure = (OptionalTime){ true, test_now - 1000 };
    sample = telemetry(test_now - 5, true, 0.0, 10.0, 0.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_TAXIING_DEPARTURE);

    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 5, false, 3000.0, 200.0, 4.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CLIMBING);
    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 5, false, 11582.4, 251.56, 0.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CRUISING);
    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 5, false, 3000.0, 200.0, -4.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_DESCENDING);
    state = normalized("landed", test_now - 5000, test_now - 1000);
    assert(state.status.phase == FLIGHT_PHASE_LANDED);
    state = normalized("unknown", test_now - 5000, test_now - 1000);
    assert(state.status.phase == FLIGHT_PHASE_UNAVAILABLE);
}

static void test_phase_boundaries_and_hysteresis(void)
{
    FlightState state = normalized("active", test_now - 1000, test_now + 10000);
    TelemetrySnapshot sample = telemetry(test_now - 1, false, 3000.0, 200.0, 2.55);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CLIMBING);
    sample.vertical_rate_mps.value = 2.05;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CLIMBING);
    sample.vertical_rate_mps.value = 1.50;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CRUISING);

    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 1, false, 3000.0, 200.0, -2.55);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_DESCENDING);
    sample.vertical_rate_mps.value = -2.05;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_DESCENDING);
    sample.vertical_rate_mps.value = -1.50;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_CRUISING);

    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 1, true, 0.0, 3.1, 0.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_TAXIING_DEPARTURE);
    sample.velocity_mps.value = 2.1;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_TAXIING_DEPARTURE);
    sample.velocity_mps.value = 1.0;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_PRE_DEPARTURE);

    state = normalized("active", test_now - 1000, test_now + 10000);
    sample = telemetry(test_now - 31, false, 3000.0, 200.0, 4.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.phase == FLIGHT_PHASE_AIRBORNE);
    assert(state.status.phase_source == PHASE_PROVIDER_CONFIRMED);
}

static void test_geospatial_progress(void)
{
    double progress = -1.0;
    double cross_track = -1.0;
    assert(geospatial_progress(0.0, 0.0, 0.0, 10.0, 0.0, 5.0,
                               &progress, &cross_track) == GEO_PROGRESS_VALID);
    assert(fabs(progress - 0.5) < 0.01);
    assert(geospatial_progress(0.0, 0.0, 0.0, 10.0, 0.0, -1.0,
                               &progress, &cross_track) == GEO_PROGRESS_VALID);
    assert(progress == 0.0);
    assert(geospatial_progress(0.0, 0.0, 0.0, 10.0, 0.0, 11.0,
                               &progress, &cross_track) == GEO_PROGRESS_VALID);
    assert(progress == 1.0);
    assert(geospatial_progress(0.0, 170.0, 0.0, -170.0, 0.0, 180.0,
                               &progress, &cross_track) == GEO_PROGRESS_VALID);
    assert(fabs(progress - 0.5) < 0.01);
    assert(geospatial_progress(0.0, 0.0, 0.0, 0.01, 0.0, 0.005,
                               &progress, &cross_track) == GEO_PROGRESS_ROUTE_TOO_SHORT);
    assert(geospatial_progress(0.0, 0.0, 0.0, 10.0, 30.0, 5.0,
                               &progress, &cross_track) == GEO_PROGRESS_OFF_ROUTE);
}

static void test_progress_provenance(void)
{
    FlightState state = normalized("active", test_now - 3600, test_now + 3600);
    TelemetrySnapshot sample = telemetry(test_now - 5, false, 11582.4, 250.0, 0.0);
    assert(state.journey.progress_source == PROGRESS_SCHEDULE_TIME);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.journey.progress_source == PROGRESS_LIVE_GEOSPATIAL);
    state = normalized("active", test_now - 3600, test_now + 3600);
    sample.latitude.value = 60.0;
    sample.longitude.value = 0.0;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.display_state == DISPLAY_LIVE);
    assert(state.journey.progress_source == PROGRESS_SCHEDULE_TIME);
    assert(state.journey.progress_fallback_reason == PROGRESS_FALLBACK_OFF_ROUTE);
    state = normalized("active", test_now - 3600, test_now + 3600);
    state.destination.latitude.available = false;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.journey.progress_source == PROGRESS_SCHEDULE_TIME);
    assert(state.journey.progress_fallback_reason == PROGRESS_FALLBACK_MISSING_COORDINATES);
    state = normalized("landed", test_now - 7200, test_now - 3600);
    assert(state.journey.progress_source == PROGRESS_LANDED);
    state = normalized("unknown", 0, 0);
    state.timing.scheduled_departure.available = false;
    state.timing.scheduled_arrival.available = false;
    normalizer_mark_telemetry_missing(&state, test_now);
    assert(state.journey.progress_source == PROGRESS_UNAVAILABLE);
}

static void test_duration_rules(void)
{
    FlightState state;
    ResolvedFlight resolved = resolved_flight("active", test_now - 3600,
                                              test_now + 3600, "2033-05-18", "SYD", "SIN");
    resolved.selected_leg.timing.actual_departure = (OptionalTime){ true, test_now - 3600 };
    normalizer_from_resolved(&state, &resolved, "TEST", test_now);
    assert(state.journey.airborne_minutes.value == 60);

    resolved = resolved_flight("landed", test_now - 7200, test_now - 3600,
                               "2033-05-18", "SYD", "SIN");
    resolved.selected_leg.timing.actual_departure = (OptionalTime){ true, test_now - 7200 };
    resolved.selected_leg.timing.actual_arrival = (OptionalTime){ true, test_now - 3600 };
    normalizer_from_resolved(&state, &resolved, "TEST", test_now);
    assert(state.journey.airborne_minutes.value == 60);

    resolved.selected_leg.timing.actual_arrival.available = false;
    resolved.selected_leg.timing.estimated_arrival = (OptionalTime){ true, test_now - 3000 };
    normalizer_from_resolved(&state, &resolved, "TEST", test_now);
    assert(state.journey.airborne_minutes.value == 70);

    resolved.selected_leg.timing.actual_departure.available = false;
    resolved.selected_leg.timing.estimated_arrival.available = false;
    normalizer_from_resolved(&state, &resolved, "TEST", test_now);
    assert(state.journey.airborne_minutes.value == 60);
}

static void test_telemetry_semantics(void)
{
    TelemetryRequest request;
    TelemetrySnapshot sample;
    FlightState state = normalized("active", test_now - 1000, test_now + 5000);
    memset(&request, 0, sizeof(request));
    (void)snprintf(request.icao24, sizeof(request.icao24), "7c806c");
    sample = telemetry(test_now - 5, false, 11582.4, 251.56, 0.0);
    assert(telemetry_snapshot_matches(&request, &sample));
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.telemetry_state == TELEMETRY_FRESH);
    assert(state.status.display_state == DISPLAY_LIVE);

    normalizer_apply_telemetry(&state, &sample, "TEST", test_now + 31);
    assert(state.status.telemetry_state == TELEMETRY_STALE);
    assert(state.status.display_state == DISPLAY_STALE);
    assert(state.status.phase == FLIGHT_PHASE_AIRBORNE);
    normalizer_mark_telemetry_missing(&state, test_now + 121);
    assert(state.status.telemetry_state == TELEMETRY_UNAVAILABLE);
    assert(state.status.display_state == DISPLAY_TRACKING);
    sample.time_position.value = test_now + 121;
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now + 122);
    assert(state.status.telemetry_state == TELEMETRY_FRESH);
    assert(state.status.display_state == DISPLAY_LIVE);

    state = normalized("active", test_now - 1000, test_now + 5000);
    sample = telemetry(test_now - 180, false, 11582.4, 251.56, 0.0);
    normalizer_apply_telemetry(&state, &sample, "TEST", test_now);
    assert(state.status.telemetry_state == TELEMETRY_STALE);
    assert(state.status.display_state == DISPLAY_STALE);

    state = normalized("active", test_now - 1000, test_now + 5000);
    normalizer_mark_telemetry_missing(&state, test_now);
    assert(state.status.telemetry_state == TELEMETRY_UNAVAILABLE);
    assert(state.status.display_state == DISPLAY_TRACKING);
    (void)snprintf(sample.icao24, sizeof(sample.icao24), "abcdef");
    assert(!telemetry_snapshot_matches(&request, &sample));
}

static void test_diagnostics(void)
{
    assert(strcmp(provider_status_name(PROVIDER_AUTH_ERROR), "authentication failed") == 0);
    assert(strcmp(provider_status_name(PROVIDER_RATE_LIMITED), "rate limited") == 0);
    assert(strcmp(provider_status_name(PROVIDER_TIMEOUT), "timeout") == 0);
    assert(strcmp(provider_status_name(PROVIDER_NOT_FOUND), "not found") == 0);
    assert(strcmp(provider_status_name(PROVIDER_PARTIAL_SUCCESS), "partial success") == 0);
    assert(strcmp(provider_status_name(PROVIDER_UNSUPPORTED_DESIGNATOR),
                  "unsupported designator format") == 0);
    assert(strcmp(provider_status_name(PROVIDER_REJECTED_DESIGNATOR),
                  "provider rejected commercial designator") == 0);
}

static void test_all_mock_fixtures(void)
{
    int fixture;
    for (fixture = 0; fixture < (int)FIXTURE_COUNT; fixture++) {
        FlightDataProvider provider;
        MockDataProviderContext context;
        FlightState state;
        ProviderResult result;
        provider_init_mock(&provider, &context, (FixtureKind)fixture, "QF9");
        result = provider.load(provider.context, &state, test_now);
        assert(result.status == PROVIDER_OK);
        assert(strcmp(state.identity.flight_number, "QF9") == 0);
        if ((FixtureKind)fixture == FIXTURE_STALE) {
            assert(state.status.phase == FLIGHT_PHASE_AIRBORNE);
            assert(state.status.phase_source == PHASE_PROVIDER_CONFIRMED);
        }
    }
}

static void test_telemetry_history_model(void)
{
    TelemetryHistory history;
    TelemetrySample sample;
    AltitudeProfilePlot plot;
    FlightState state;
    size_t index;
    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    telemetry_history_init(&history);
    telemetry_history_set_interval(&history, 15U);
    assert(history.expected_interval_seconds == 15U);
    assert(telemetry_sample_from_flight(&state, &sample));
    assert(sample.timestamp.value == test_now - 3);
    assert(sample.altitude_feet.available && sample.altitude_feet.value == 38000);
    for (index = 0; index < TELEMETRY_HISTORY_CAPACITY + 2U; index++) {
        sample.timestamp.value = (time_t)index;
        assert(telemetry_history_append(&history, &sample));
    }
    assert(history.count == TELEMETRY_HISTORY_CAPACITY);
    assert(telemetry_history_at(&history, 0)->timestamp.value == 2);
    assert(telemetry_history_at(&history, TELEMETRY_HISTORY_CAPACITY - 1U)->timestamp.value ==
           (time_t)(TELEMETRY_HISTORY_CAPACITY + 1U));
    assert(telemetry_history_at(&history, TELEMETRY_HISTORY_CAPACITY) == NULL);
    assert(altitude_profile_build(&history, 80, 10, &plot));
    assert(plot.plotted_samples == TELEMETRY_HISTORY_CAPACITY);
    assert(plot.cells[0][plot.width - 1] == ALTITUDE_CELL_CURRENT ||
           plot.cells[1][plot.width - 1] == ALTITUDE_CELL_CURRENT);
    state.metadata.telemetry_updated.available = false;
    assert(!telemetry_sample_from_flight(&state, &sample));
}

static TelemetrySample altitude_sample(time_t timestamp, int altitude)
{
    TelemetrySample sample;
    memset(&sample, 0, sizeof(sample));
    sample.timestamp = (OptionalTime){ true, timestamp };
    sample.altitude_feet = (OptionalInt){ true, altitude };
    sample.freshness = TELEMETRY_FRESH;
    return sample;
}

static bool frame_contains(const Frame *frame, const char *text)
{
    int index;
    for (index = 0; index < frame->count; index++) {
        if (strstr(frame->lines[index], text) != NULL) return true;
    }
    return false;
}

static void test_telemetry_history_acceptance_and_lifecycle(void)
{
    TelemetryHistory history;
    TelemetrySample sample = altitude_sample(100, 10000);
    FlightState state;
    telemetry_history_init(&history);
    assert(telemetry_history_set_occurrence(&history, "occurrence-a|leg-a"));
    assert(!telemetry_history_set_occurrence(&history, "occurrence-a|leg-a"));
    assert(telemetry_history_append(&history, &sample));
    assert(history.maximum_altitude_feet == 10000);
    assert(!telemetry_history_append(&history, &sample));
    sample.timestamp.value = 99;
    assert(!telemetry_history_append(&history, &sample));
    sample.timestamp.value = 101;
    assert(telemetry_history_append(&history, &sample));
    sample.timestamp.value = 102;
    sample.freshness = TELEMETRY_STALE;
    assert(!telemetry_history_append(&history, &sample));
    assert(history.count == 2U);

    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    telemetry_history_init(&history);
    assert(telemetry_history_observe(&history, "occurrence-a|leg-a", &state));
    assert(!telemetry_history_observe(&history, "occurrence-a|leg-a", &state));
    state.metadata.telemetry_updated.value++;
    state.status.telemetry_state = TELEMETRY_STALE;
    assert(!telemetry_history_observe(&history, "occurrence-a|leg-a", &state));
    assert(history.count == 1U);
    assert(!telemetry_history_observe(&history, "occurrence-b|leg-b", &state));
    assert(history.count == 0U);
    assert(history.maximum_altitude_feet == 0);
    assert(strcmp(history.occurrence_id, "occurrence-b|leg-b") == 0);
    state.status.telemetry_state = TELEMETRY_FRESH;
    assert(telemetry_history_observe(&history, "occurrence-b|leg-b", &state));
    assert(history.count == 1U);
}

static void test_altitude_profile_math(void)
{
    assert(altitude_profile_map_time(100, 100, 200, 11) == 0);
    assert(altitude_profile_map_time(150, 100, 200, 11) == 5);
    assert(altitude_profile_map_time(200, 100, 200, 11) == 10);
    assert(altitude_profile_map_time(100, 100, 100, 11) == 0);
    assert(altitude_profile_map_altitude(40000, 40000, 9) == 0);
    assert(altitude_profile_map_altitude(20000, 40000, 9) == 4);
    assert(altitude_profile_map_altitude(0, 40000, 9) == 8);
    assert(altitude_profile_map_altitude(-100, 40000, 9) == 8);
    assert(altitude_profile_is_gap(100, 131, 15U));
    assert(!altitude_profile_is_gap(100, 130, 15U));
    assert(altitude_profile_is_gap(100, 100, 15U));
}

static void test_altitude_profile_geometry(void)
{
    TelemetryHistory history;
    TelemetrySample sample;
    AltitudeProfilePlot plot;
    int row;
    int column;
    bool middle_has_line = false;
    telemetry_history_init(&history);
    telemetry_history_set_interval(&history, 15U);
    assert(!altitude_profile_build(&history, 30, 8, &plot));

    sample = altitude_sample(100, 0);
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 30, 8, &plot));
    assert(plot.plotted_samples == 1U);
    assert(plot.cells[7][0] == ALTITUDE_CELL_CURRENT);

    sample = altitude_sample(115, 20000);
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(130, 40000);
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 30, 8, &plot));
    assert(plot.width == 30 && plot.height == 8);
    assert(plot.cells[0][29] == ALTITUDE_CELL_CURRENT);
    for (row = 0; row < plot.height; row++) {
        if (plot.cells[row][plot.width / 2] != ALTITUDE_CELL_EMPTY) middle_has_line = true;
    }
    assert(middle_has_line);

    telemetry_history_init(&history);
    telemetry_history_set_interval(&history, 15U);
    sample = altitude_sample(100, 10000);
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(115, 0);
    sample.altitude_feet.available = false;
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(130, 30000);
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 21, 8, &plot));
    for (row = 0; row < plot.height; row++)
        assert(plot.cells[row][plot.width / 2] == ALTITUDE_CELL_EMPTY);

    telemetry_history_init(&history);
    telemetry_history_set_interval(&history, 15U);
    sample = altitude_sample(100, 10000);
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(131, 30000);
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 21, 8, &plot));
    for (column = 1; column < plot.width - 1; column++) {
        for (row = 0; row < plot.height; row++)
            assert(plot.cells[row][column] == ALTITUDE_CELL_EMPTY);
    }

    telemetry_history_init(&history);
    sample = altitude_sample(100, 10000);
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(101, 20000);
    assert(telemetry_history_append(&history, &sample));
    sample = altitude_sample(102, 30000);
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 2, 8, &plot));
    assert(plot.width == 2 && plot.plotted_samples == 3U);

    sample = altitude_sample(103, 0);
    sample.altitude_feet.available = false;
    assert(telemetry_history_append(&history, &sample));
    assert(altitude_profile_build(&history, 2, 8, &plot));
    for (row = 0; row < plot.height; row++) {
        for (column = 0; column < plot.width; column++)
            assert(plot.cells[row][column] != ALTITUDE_CELL_CURRENT);
    }
}

static void test_altitude_profile_render_states(void)
{
    TelemetryHistory history;
    TelemetrySample sample;
    FlightState state;
    AnimationState animation;
    Layout layout;
    Frame frame;
    bool found_marker = false;
    int index;
    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    memset(&animation, 0, sizeof(animation));
    animation.heartbeat = "•";
    layout = layout_select((TerminalSize){ 100, 28 });
    telemetry_history_init(&history);
    frame_init(&frame, layout.content_width);
    altitude_profile_visual_render(&frame, &state, &history, &animation, &layout);
    assert(frame_contains(&frame, "WAITING FOR LIVE TELEMETRY"));
    sample = altitude_sample(100, 10000);
    assert(telemetry_history_append(&history, &sample));
    frame_init(&frame, layout.content_width);
    altitude_profile_visual_render(&frame, &state, &history, &animation, &layout);
    assert(frame_contains(&frame, "TRACKING STARTED"));
    sample = altitude_sample(115, 20000);
    assert(telemetry_history_append(&history, &sample));
    frame_init(&frame, layout.content_width);
    altitude_profile_visual_render(&frame, &state, &history, &animation, &layout);
    for (index = 0; index < frame.count; index++) {
        if (strstr(frame.lines[index], "✈") != NULL) found_marker = true;
    }
    assert(found_marker);
    animation.heartbeat = "·";
    frame_init(&frame, layout.content_width);
    altitude_profile_visual_render(&frame, &state, &history, &animation, &layout);
    assert(!frame_contains(&frame, "✈"));
    layout = layout_select((TerminalSize){ 45, 20 });
    frame_init(&frame, layout.content_width);
    altitude_profile_visual_render(&frame, &state, &history, &animation, &layout);
    assert(frame_contains(&frame, "REQUIRES MORE SPACE"));
}

static void test_map_geometry(void)
{
    const GeoCoordinate melbourne = { -37.6733, 144.8433 };
    const GeoCoordinate london = { 51.4700, -0.4543 };
    GeoCoordinate point;
    MapRoute route;
    MapViewport viewport;
    MapPoint projected;
    size_t index;
    assert(map_great_circle_position(melbourne, london, 0.0, &point));
    assert(fabs(point.latitude - melbourne.latitude) < 0.001);
    assert(map_great_circle_position(melbourne, london, 0.5, &point));
    assert(point.latitude > 0.0);
    assert(map_great_circle_position(melbourne, london, 1.0, &point));
    assert(fabs(point.latitude - london.latitude) < 0.001);

    assert(map_great_circle_position((GeoCoordinate){ 0.0, 170.0 },
                                     (GeoCoordinate){ 0.0, -170.0 }, 0.5, &point));
    assert(fabs(fabs(point.longitude) - 180.0) < 0.001);
    assert(map_great_circle_position((GeoCoordinate){ 75.0, -40.0 },
                                     (GeoCoordinate){ 75.0, 80.0 }, 0.5, &point));
    assert(point.latitude > 75.0);
    assert(map_route_sample((GeoCoordinate){ 0.0, 0.0 },
                            (GeoCoordinate){ 0.001, 0.001 }, 1U, &route));
    assert(route.count == 2U);
    assert(map_route_sample((GeoCoordinate){ -40.0, 20.0 },
                            (GeoCoordinate){ 40.0, 20.0 }, 64U, &route));
    assert(map_viewport_fit(&viewport, &route, 80, 20, 0.05));
    for (index = 0U; index < route.count; index++) {
        assert(map_viewport_project(&viewport, route.points[index], &projected));
        assert(projected.x >= 0.0 && projected.x <= 1.0);
        assert(projected.y >= 0.0 && projected.y <= 1.0);
    }
    assert(map_route_sample((GeoCoordinate){ 10.0, -60.0 },
                            (GeoCoordinate){ 10.0, 60.0 }, 64U, &route));
    assert(map_viewport_fit(&viewport, &route, 80, 20, 0.05));
    assert(!map_viewport_fit(&viewport, &route, 0, 20, 0.05));
    assert(!geo_coordinate_valid((GeoCoordinate){ 91.0, 0.0 }));

    {
        MapPoint first = { -0.5, 0.5 };
        MapPoint second = { 1.5, 0.5 };
        assert(map_clip_normalized_line(&first, &second));
        assert(fabs(first.x) < 0.0001);
        assert(fabs(second.x - 1.0) < 0.0001);
        first = (MapPoint){ -0.5, -0.5 };
        second = (MapPoint){ -0.1, -0.1 };
        assert(!map_clip_normalized_line(&first, &second));
    }
}

static void test_subcell_canvas(void)
{
    static const int coordinates[8][2] = {
        { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 },
        { 1, 1 }, { 1, 2 }, { 0, 3 }, { 1, 3 }
    };
    static const uint32_t expected[8] = {
        UINT32_C(0x2801), UINT32_C(0x2802), UINT32_C(0x2804), UINT32_C(0x2808),
        UINT32_C(0x2810), UINT32_C(0x2820), UINT32_C(0x2840), UINT32_C(0x2880)
    };
    SubcellCanvas canvas;
    int index;
    assert(!subcell_canvas_init(&canvas, 0, 1));
    for (index = 0; index < 8; index++) {
        assert(subcell_canvas_init(&canvas, 1, 1));
        assert(subcell_canvas_set(&canvas, coordinates[index][0], coordinates[index][1]));
        assert(subcell_canvas_codepoint(&canvas, 0, 0) == expected[index]);
    }
    assert(subcell_canvas_init(&canvas, 1, 1));
    for (index = 0; index < 8; index++)
        assert(subcell_canvas_set(&canvas, coordinates[index][0], coordinates[index][1]));
    assert(subcell_canvas_codepoint(&canvas, 0, 0) == UINT32_C(0x28ff));
    assert(!subcell_canvas_set(&canvas, -1, 0));
    assert(!subcell_canvas_set(&canvas, 2, 0));
    assert(subcell_canvas_init(&canvas, 4, 2));
    subcell_canvas_line(&canvas, 0, 0, 7, 7);
    assert(subcell_canvas_codepoint(&canvas, 0, 0) != (uint32_t)' ');
    assert(subcell_canvas_codepoint(&canvas, 3, 1) != (uint32_t)' ');
}

static bool raster_contains(const MapRaster *raster, uint32_t codepoint)
{
    int row;
    int column;
    for (row = 0; row < raster->rows; row++)
        for (column = 0; column < raster->columns; column++)
            if (raster->cells[row][column] == codepoint) return true;
    return false;
}

static void test_map_raster_backends(void)
{
    MapPoint horizontal[] = { { 0.0, 0.5 }, { 0.5, 0.5 }, { 1.0, 0.5 } };
    MapPoint rising[] = { { 0.0, 1.0 }, { 1.0, 0.0 } };
    MapPoint falling[] = { { 0.0, 0.0 }, { 1.0, 1.0 } };
    MapPoint corner[] = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 } };
    MapPoint clipped[] = { { -1.0, 0.5 }, { 2.0, 0.5 } };
    MapPoint marker = { 0.5, 0.5 };
    MapRaster raster;
    char row[FRAME_LINE_CAPACITY];
    assert(!map_raster_render(&raster, MAP_RASTER_COMPAT, horizontal, 3U,
                              NULL, 1, 4));
    assert(map_raster_render(&raster, MAP_RASTER_COMPAT, horizontal, 3U,
                             &marker, 20, 5));
    assert(raster_contains(&raster, UINT32_C(0x2500)));
    assert(raster_contains(&raster, UINT32_C(0x25cf)));
    assert(raster_contains(&raster, UINT32_C(0x25c6)));
    assert(map_raster_row_utf8(&raster, raster.marker_row, row, sizeof(row)));
    assert(strstr(row, "◆") != NULL);
    assert(map_raster_render(&raster, MAP_RASTER_COMPAT, rising, 2U,
                             NULL, 5, 5));
    assert(raster_contains(&raster, UINT32_C(0x2571)));
    assert(map_raster_render(&raster, MAP_RASTER_COMPAT, falling, 2U,
                             NULL, 5, 5));
    assert(raster_contains(&raster, UINT32_C(0x2572)));
    assert(map_raster_render(&raster, MAP_RASTER_COMPAT, corner, 3U,
                             NULL, 5, 5));
    assert(raster_contains(&raster, UINT32_C(0x256e)));
    assert(map_raster_render(&raster, MAP_RASTER_COMPAT, clipped, 2U,
                             NULL, MAP_RASTER_MAX_COLUMNS, MAP_RASTER_MAX_ROWS));
    assert(raster.cells[MAP_RASTER_MAX_ROWS / 2][0] != (uint32_t)' ');
    assert(map_raster_render(&raster, MAP_RASTER_BRAILLE, rising, 2U,
                             &marker, 20, 5));
    assert(raster_contains(&raster, UINT32_C(0x25c6)));
    assert(raster.cells[4][0] == UINT32_C(0x25cf));
    assert(raster.cells[0][19] == UINT32_C(0x25cf));
    assert(!map_raster_row_utf8(&raster, 0, row, 2U));
}

static void test_route_map_visual_states(void)
{
    FlightState state;
    AnimationState animation;
    Layout layout;
    Frame frame;
    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    memset(&animation, 0, sizeof(animation));
    animation.heartbeat = "•";
    layout = layout_select((TerminalSize){ 130, 30 });
    assert(route_map_backend_for_layout(layout.mode) == MAP_RASTER_BRAILLE);
    assert(route_map_backend_for_layout(LAYOUT_MEDIUM) == MAP_RASTER_BRAILLE);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame_contains(&frame, "ROUTE MAP"));
    assert(frame_contains(&frame, "◆"));
    assert(frame_contains(&frame, "✈"));
    assert(frame_contains(&frame, "58.2%"));

    mock_provider_load(&state, FIXTURE_SCHEDULED, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(!frame_contains(&frame, "◆"));
    assert(!frame_contains(&frame, "✈"));
    assert(frame_contains(&frame, "SCHEDULE PROGRESS"));

    mock_provider_load(&state, FIXTURE_UNAVAILABLE, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame_contains(&frame, "NO LIVE POSITION"));

    mock_provider_load(&state, FIXTURE_STALE, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(!frame_contains(&frame, "◆"));
    assert(!frame_contains(&frame, "✈"));

    state.origin.latitude.available = false;
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame_contains(&frame, "ROUTE GEOMETRY UNAVAILABLE"));

    mock_provider_load(&state, FIXTURE_LANDED, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame_contains(&frame, "100.0% · LANDED"));
    assert(frame_contains(&frame, "◆"));

    layout = layout_select((TerminalSize){ 60, 18 });
    assert(route_map_backend_for_layout(layout.mode) == MAP_RASTER_COMPAT);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame.count <= FRAME_MAX_LINES);
    layout = layout_select((TerminalSize){ 40, 12 });
    assert(route_map_backend_for_layout(layout.mode) == MAP_RASTER_COMPAT);
    frame_init(&frame, layout.content_width);
    route_map_visual_render(&frame, &state, &animation, &layout);
    assert(frame.count <= layout.height);
}

static unsigned long coastline_fingerprint(const SubcellCanvas *canvas)
{
    unsigned long hash = 2166136261UL;
    int row;
    int column;
    for (row = 0; row < canvas->rows; row++)
        for (column = 0; column < canvas->columns; column++) {
            hash ^= (unsigned long)canvas->cells[row][column];
            hash *= 16777619UL;
        }
    return hash;
}

static void test_geographic_map_poc_geometry(void)
{
    static const GeoCoordinate routes[][2] = {
        { { -37.6733, 144.8433 }, { 51.4700, -0.4543 } },
        { { -37.6733, 144.8433 }, { 25.2731, 51.6081 } },
        { { 51.4700, -0.4543 }, { 33.9416, -118.4085 } },
        { { -33.9399, 151.1753 }, { 1.3644, 103.9915 } },
        { { -17.7554, 177.4434 }, { 21.3187, -157.9225 } },
        { { 64.1300, -21.9406 }, { 61.1743, -149.9985 } }
    };
    GeographicMapPocScene scene;
    GeographicMapPocScene repeated;
    MapPoint projected_origin;
    size_t index;
    assert(coastline_data_point_count() == 5127U);
    assert(coastline_data_segment_count() == 134U);
    for (index = 0U; index < sizeof(routes) / sizeof(routes[0]); index++) {
        assert(geographic_map_poc_prepare(&scene, routes[index][0], routes[index][1],
                                          100, 12, true));
        assert(scene.valid);
        assert(scene.geography_available);
        assert(scene.coastline_segments_drawn > 0U);
        assert(map_viewport_project(&scene.viewport, routes[index][0],
                                    &projected_origin));
        assert(fabs(projected_origin.x - scene.projected_route[0].x) < 0.000001);
        assert(fabs(projected_origin.y - scene.projected_route[0].y) < 0.000001);
    }
    assert(geographic_map_poc_prepare(&scene, routes[0][0], routes[0][1],
                                      100, 12, true));
    assert(geographic_map_poc_prepare(&repeated, routes[0][0], routes[0][1],
                                      100, 12, true));
    assert(coastline_fingerprint(&scene.coastline) ==
           coastline_fingerprint(&repeated.coastline));
    assert(coastline_fingerprint(&scene.coastline) != 0UL);
    assert(geographic_map_poc_prepare(&scene, routes[0][0], routes[0][1],
                                      100, 12, false));
    assert(!scene.geography_available);
    assert(scene.coastline_segments_drawn == 0U);
}

static void test_geographic_map_poc_states(void)
{
    FlightState state;
    AnimationState animation;
    Layout layout = layout_select((TerminalSize){ 130, 30 });
    Frame frame;
    (void)memset(&animation, 0, sizeof(animation));
    animation.heartbeat = "•";
    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, true);
    assert(frame_contains(&frame, "GEOGRAPHIC MAP POC"));
    assert(frame_contains(&frame, "GEO ON"));
    assert(frame_contains(&frame, "◆"));
    assert(frame_contains(&frame, "✈"));

    layout = layout_select((TerminalSize){ 100, 24 });
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, true);
    assert(frame_contains(&frame, "GEO ON"));
    assert(frame_contains(&frame, "◆"));

    mock_provider_load(&state, FIXTURE_SCHEDULED, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, true);
    assert(!frame_contains(&frame, "◆"));
    assert(!frame_contains(&frame, "✈"));
    assert(frame_contains(&frame, "NO LIVE POSITION"));

    mock_provider_load(&state, FIXTURE_LANDED, "QF9", test_now);
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, true);
    assert(frame_contains(&frame, "100.0% · LANDED"));
    assert(frame_contains(&frame, "◆"));

    layout = layout_select((TerminalSize){ 70, 20 });
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, true);
    assert(frame_contains(&frame, "GEOGRAPHIC MAP REQUIRES MORE SPACE"));
    frame_init(&frame, layout.content_width);
    geographic_map_poc_render(&frame, &state, &animation, &layout, false);
    assert(frame_contains(&frame, "ROUTE MAP"));
}

static int frame_style_count(const Frame *frame, FrameStyle style)
{
    int count = 0;
    int row;
    int column;
    for (row = 0; row < frame->count; row++)
        for (column = 0; column < frame->width; column++)
            if (frame->styles[row][column] == style) count++;
    return count;
}

static uint32_t frame_codepoint_at(const Frame *frame, int row, int wanted_column)
{
    const unsigned char *cursor = (const unsigned char *)frame->lines[row];
    int column = 0;
    while (*cursor != '\0' && column < wanted_column) {
        if ((*cursor & 0x80U) == 0U) cursor += 1;
        else if ((*cursor & 0xe0U) == 0xc0U) cursor += 2;
        else if ((*cursor & 0xf0U) == 0xe0U) cursor += 3;
        else cursor += 4;
        column++;
    }
    if (column != wanted_column || *cursor == '\0') return 0U;
    if ((*cursor & 0x80U) == 0U) return (uint32_t)*cursor;
    if ((*cursor & 0xe0U) == 0xc0U)
        return ((uint32_t)(cursor[0] & 0x1fU) << 6) |
               (uint32_t)(cursor[1] & 0x3fU);
    return ((uint32_t)(cursor[0] & 0x0fU) << 12) |
           ((uint32_t)(cursor[1] & 0x3fU) << 6) |
           (uint32_t)(cursor[2] & 0x3fU);
}

static void test_geographic_map_toggle_and_styles(void)
{
    FlightState state;
    FlightState before_state;
    AnimationState animation;
    TelemetryHistory history;
    RuntimeSchedule schedule;
    RuntimeSchedule before_schedule;
    VisualViewport viewport;
    Layout layout = layout_select((TerminalSize){ 130, 30 });
    Frame geography_on;
    Frame geography_off;
    int row;
    int column;
    bool saw_styled_reset = false;
    (void)memset(&animation, 0, sizeof(animation));
    animation.heartbeat = "•";
    telemetry_history_init(&history);
    runtime_schedule_init(&schedule, 1000U);
    mock_provider_load(&state, FIXTURE_QF9_CRUISING, "QF9", test_now);
    before_state = state;
    before_schedule = schedule;
    visual_viewport_init(&viewport, VISUAL_GEOGRAPHIC_MAP_POC, &history);
    assert(viewport.geography_enabled);
    assert(input_action_for_key('g') == INPUT_TOGGLE_GEOGRAPHY);
    assert(INPUT_TOGGLE_GEOGRAPHY != INPUT_REFRESH);
    assert(visual_viewport_toggle_geography(&viewport));
    assert(!viewport.geography_enabled);
    assert(viewport.mode == VISUAL_GEOGRAPHIC_MAP_POC);
    assert(viewport.history == &history);
    assert(memcmp(&state, &before_state, sizeof(state)) == 0);
    assert(memcmp(&schedule, &before_schedule, sizeof(schedule)) == 0);
    assert(visual_viewport_toggle_geography(&viewport));
    assert(viewport.geography_enabled);

    frame_init(&geography_on, layout.content_width);
    geographic_map_poc_render(&geography_on, &state, &animation, &layout, true);
    frame_init(&geography_off, layout.content_width);
    geographic_map_poc_render(&geography_off, &state, &animation, &layout, false);
    assert(frame_contains(&geography_on, "GEO ON"));
    assert(frame_contains(&geography_off, "GEO OFF"));
    assert(frame_style_count(&geography_on, FRAME_STYLE_DIM) > 0);
    assert(frame_style_count(&geography_off, FRAME_STYLE_DIM) == 0);
    assert(frame_style_count(&geography_on, FRAME_STYLE_ACCENT) >= 2);
    assert(frame_style_count(&geography_off, FRAME_STYLE_ACCENT) >= 2);
    for (row = 3; row < 15; row++) {
        char styled[FRAME_RENDERED_LINE_CAPACITY];
        char monochrome[FRAME_RENDERED_LINE_CAPACITY];
        assert(frame_render_line(&geography_on, row, true, styled, sizeof(styled)));
        assert(frame_render_line(&geography_on, row, false, monochrome,
                                 sizeof(monochrome)));
        assert(strchr(monochrome, '\x1b') == NULL);
        assert(strcmp(monochrome, geography_on.lines[row]) == 0);
        if (strstr(styled, "\x1b[2m") != NULL &&
            strstr(styled, "\x1b[0m") != NULL) saw_styled_reset = true;
        for (column = 0; column < geography_off.width; column++) {
            uint32_t off_glyph = frame_codepoint_at(&geography_off, row, column);
            if (off_glyph != 0U && off_glyph != (uint32_t)' ') {
                assert(frame_codepoint_at(&geography_on, row, column) == off_glyph);
                assert(geography_on.styles[row][column] != FRAME_STYLE_DIM);
                if (off_glyph == UINT32_C(0x25c6))
                    assert(geography_on.styles[row][column] == FRAME_STYLE_ACCENT);
            }
        }
    }
    assert(saw_styled_reset);

    layout = layout_select((TerminalSize){ 100, 24 });
    frame_init(&geography_on, layout.content_width);
    geographic_map_poc_render(&geography_on, &state, &animation, &layout, true);
    frame_init(&geography_off, layout.content_width);
    geographic_map_poc_render(&geography_off, &state, &animation, &layout, false);
    assert(frame_contains(&geography_on, "GEO ON"));
    assert(frame_contains(&geography_off, "GEO OFF"));

    viewport.mode = VISUAL_AIRCRAFT;
    assert(!visual_viewport_toggle_geography(&viewport));
    assert(viewport.mode == VISUAL_AIRCRAFT);
}

static void test_visual_mode_contract(void)
{
    TelemetryHistory history;
    VisualViewport viewport;
    FlightState state;
    AnimationState animation;
    Layout layout;
    Frame frame;
    telemetry_history_init(&history);
    visual_viewport_init(&viewport, VISUAL_AIRCRAFT, &history);
    assert(viewport.mode == VISUAL_AIRCRAFT);
    assert(viewport.history == &history);
    assert(VISUAL_ALTITUDE_PROFILE != VISUAL_ROUTE_MAP);
    assert(VISUAL_RADAR != VISUAL_MINIMAL);
    assert(visual_mode_parse("aircraft", &viewport.mode));
    assert(viewport.mode == VISUAL_AIRCRAFT);
    assert(visual_mode_parse("altitude", &viewport.mode));
    assert(viewport.mode == VISUAL_ALTITUDE_PROFILE);
    assert(visual_mode_parse("route", &viewport.mode));
    assert(viewport.mode == VISUAL_ROUTE_MAP);
    assert(visual_mode_parse("geo", &viewport.mode));
    assert(viewport.mode == VISUAL_GEOGRAPHIC_MAP_POC);
    assert(!visual_mode_parse("radar", &viewport.mode));
    assert(!visual_mode_parse("minimal", &viewport.mode));
    assert(strcmp(visual_mode_name(VISUAL_ALTITUDE_PROFILE), "altitude") == 0);
    assert(strcmp(visual_mode_name(VISUAL_ROUTE_MAP), "route") == 0);
    assert(strcmp(visual_mode_name(VISUAL_GEOGRAPHIC_MAP_POC), "geo") == 0);
    viewport.mode = VISUAL_AIRCRAFT;
    visual_viewport_toggle(&viewport);
    assert(viewport.mode == VISUAL_ALTITUDE_PROFILE);
    visual_viewport_toggle(&viewport);
    assert(viewport.mode == VISUAL_ROUTE_MAP);
    visual_viewport_toggle(&viewport);
    assert(viewport.mode == VISUAL_AIRCRAFT);
    viewport.mode = VISUAL_GEOGRAPHIC_MAP_POC;
    visual_viewport_toggle(&viewport);
    assert(viewport.mode == VISUAL_AIRCRAFT);
    assert(viewport.history == &history);
    assert(input_action_for_key('q') == INPUT_QUIT);
    assert(input_action_for_key('r') == INPUT_REFRESH);
    assert(input_action_for_key('f') == INPUT_NEXT_FIXTURE);
    assert(input_action_for_key('v') == INPUT_NEXT_VISUAL);
    assert(input_action_for_key('g') == INPUT_TOGGLE_GEOGRAPHY);
    memset(&state, 0, sizeof(state));
    memset(&animation, 0, sizeof(animation));
    memset(&layout, 0, sizeof(layout));
    frame_init(&frame, 80);
    visual_viewport_init(&viewport, VISUAL_ALTITUDE_PROFILE, NULL);
    visual_viewport_render(&viewport, &frame, &state, &animation, &layout);
    assert(frame.count >= 2);
    assert(strstr(frame.lines[0], "ALTITUDE PROFILE") != NULL);
    frame_init(&frame, 80);
    visual_viewport_init(&viewport, VISUAL_ROUTE_MAP, NULL);
    visual_viewport_render(&viewport, &frame, &state, &animation, &layout);
    assert(strstr(frame.lines[0], "ROUTE MAP") != NULL);
    frame_init(&frame, 80);
    layout = layout_select((TerminalSize){ 100, 24 });
    visual_viewport_init(&viewport, VISUAL_GEOGRAPHIC_MAP_POC, NULL);
    visual_viewport_render(&viewport, &frame, &state, &animation, &layout);
    assert(strstr(frame.lines[0], "GEOGRAPHIC MAP POC") != NULL);
    frame_init(&frame, 80);
    visual_viewport_init(&viewport, VISUAL_RADAR, NULL);
    visual_viewport_render(&viewport, &frame, &state, &animation, &layout);
    assert(strstr(frame.lines[0], "RADAR") != NULL);
    frame_init(&frame, 80);
    visual_viewport_init(&viewport, VISUAL_MINIMAL, NULL);
    visual_viewport_render(&viewport, &frame, &state, &animation, &layout);
    assert(strstr(frame.lines[0], "MINIMAL") != NULL);
}

int main(void)
{
    assert(strcmp(FLIGHT_VERSION, "0.1.0") == 0);
    test_json_state_vector();
    test_airport_validation();
    test_designator_validation();
    test_occurrence_selection();
    test_multileg_isolation();
    test_phase_inference();
    test_phase_boundaries_and_hysteresis();
    test_geospatial_progress();
    test_progress_provenance();
    test_duration_rules();
    test_telemetry_semantics();
    test_diagnostics();
    test_all_mock_fixtures();
    test_telemetry_history_model();
    test_telemetry_history_acceptance_and_lifecycle();
    test_altitude_profile_math();
    test_altitude_profile_geometry();
    test_altitude_profile_render_states();
    test_map_geometry();
    test_subcell_canvas();
    test_map_raster_backends();
    test_route_map_visual_states();
    test_geographic_map_poc_geometry();
    test_geographic_map_poc_states();
    test_geographic_map_toggle_and_styles();
    test_visual_mode_contract();
    (void)puts("data semantics tests passed");
    return 0;
}
