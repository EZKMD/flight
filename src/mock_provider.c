#include "mock_provider.h"

#include <stdio.h>
#include <string.h>

static OptionalDouble number(double value) { return (OptionalDouble){ true, value }; }
static OptionalInt integer(int value) { return (OptionalInt){ true, value }; }
static OptionalTime moment(time_t value) { return (OptionalTime){ true, value }; }

static void load_qf9_base(FlightState *state, const char *flight_number, time_t now)
{
    memset(state, 0, sizeof(*state));
    (void)snprintf(state->identity.flight_number, sizeof(state->identity.flight_number), "%s",
                   flight_number != NULL ? flight_number : "QF9");
    (void)snprintf(state->identity.airline_name, sizeof(state->identity.airline_name), "QANTAS");
    (void)snprintf(state->identity.airline_code, sizeof(state->identity.airline_code), "QF");
    (void)snprintf(state->aircraft.model, sizeof(state->aircraft.model), "BOEING 787-9");
    (void)snprintf(state->aircraft.registration, sizeof(state->aircraft.registration), "VH-ZNA");
    (void)snprintf(state->origin.name, sizeof(state->origin.name), "MELBOURNE");
    (void)snprintf(state->origin.iata, sizeof(state->origin.iata), "MEL");
    (void)snprintf(state->origin.icao, sizeof(state->origin.icao), "YMML");
    state->origin.latitude = number(-37.6733);
    state->origin.longitude = number(144.8433);
    (void)snprintf(state->origin.timezone, sizeof(state->origin.timezone), "Australia/Melbourne");
    (void)snprintf(state->destination.name, sizeof(state->destination.name), "LONDON HEATHROW");
    (void)snprintf(state->destination.iata, sizeof(state->destination.iata), "LHR");
    (void)snprintf(state->destination.icao, sizeof(state->destination.icao), "EGLL");
    state->destination.latitude = number(51.4700);
    state->destination.longitude = number(-0.4543);
    (void)snprintf(state->destination.timezone, sizeof(state->destination.timezone), "Europe/London");
    state->timing.scheduled_departure = moment(now - (time_t)(470 * 60));
    state->timing.actual_departure = state->timing.scheduled_departure;
    state->timing.scheduled_arrival = moment(now + (time_t)(331 * 60));
    state->timing.estimated_arrival = state->timing.scheduled_arrival;
    (void)snprintf(state->timing.departure_display, sizeof(state->timing.departure_display), "21:07");
    (void)snprintf(state->timing.arrival_display, sizeof(state->timing.arrival_display), "06:29");
    state->position.latitude = number(22.2);
    state->position.longitude = number(76.4);
    state->position.altitude_feet = integer(38000);
    state->position.flight_level = integer(380);
    state->position.groundspeed_knots = integer(489);
    state->position.heading_degrees = integer(302);
    (void)snprintf(state->position.heading_compass, sizeof(state->position.heading_compass), "NW");
    state->journey.progress = 0.582;
    state->journey.progress_available = true;
    state->journey.progress_source = PROGRESS_LIVE_GEOSPATIAL;
    state->journey.airborne_minutes = integer(470);
    state->journey.remaining_minutes = integer(331);
    state->status.phase = FLIGHT_PHASE_CRUISING;
    state->status.data_available = true;
    state->status.occurrence_confidence = OCCURRENCE_CONFIRMED;
    state->status.telemetry_state = TELEMETRY_FRESH;
    state->status.phase_source = PHASE_TELEMETRY_DERIVED;
    state->status.display_state = DISPLAY_LIVE;
    state->metadata.last_updated = moment(now - 3);
    state->metadata.telemetry_updated = state->metadata.last_updated;
    state->metadata.telemetry_received = moment(now);
    (void)snprintf(state->metadata.source, sizeof(state->metadata.source), "MOCK");
}

const char *mock_provider_fixture_name(FixtureKind fixture)
{
    static const char *const names[] = {
        "cruising", "scheduled", "descending", "landed", "delayed", "stale", "unavailable"
    };
    if (fixture < 0 || fixture >= FIXTURE_COUNT) return "cruising";
    return names[(int)fixture];
}

bool mock_provider_parse_fixture(const char *name, FixtureKind *fixture)
{
    int index;
    for (index = 0; index < (int)FIXTURE_COUNT; index++) {
        if (strcmp(name, mock_provider_fixture_name((FixtureKind)index)) == 0) {
            *fixture = (FixtureKind)index;
            return true;
        }
    }
    return false;
}

FixtureKind mock_provider_next_fixture(FixtureKind fixture)
{
    return (FixtureKind)(((int)fixture + 1) % (int)FIXTURE_COUNT);
}

void mock_provider_load(FlightState *state, FixtureKind fixture, const char *flight_number,
                        time_t now)
{
    load_qf9_base(state, flight_number, now);
    switch (fixture) {
        case FIXTURE_QF9_CRUISING: break;
        case FIXTURE_SCHEDULED:
            state->status.phase = FLIGHT_PHASE_SCHEDULED;
            state->journey.progress = 0.0;
            state->journey.progress_source = PROGRESS_SCHEDULE_TIME;
            state->journey.airborne_minutes.available = false;
            state->journey.remaining_minutes = integer(801);
            state->position.flight_level.available = false;
            state->position.groundspeed_knots = integer(0);
            state->position.heading_degrees.available = false;
            state->status.telemetry_state = TELEMETRY_NOT_EXPECTED;
            state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
            state->status.display_state = DISPLAY_SCHEDULED;
            break;
        case FIXTURE_DESCENDING:
            state->status.phase = FLIGHT_PHASE_DESCENDING;
            state->journey.progress = 0.91;
            state->journey.airborne_minutes = integer(738);
            state->journey.remaining_minutes = integer(63);
            state->position.flight_level = integer(190);
            state->position.altitude_feet = integer(19000);
            state->position.groundspeed_knots = integer(420);
            break;
        case FIXTURE_LANDED:
            state->status.phase = FLIGHT_PHASE_LANDED;
            state->journey.progress = 1.0;
            state->journey.progress_source = PROGRESS_LANDED;
            state->journey.airborne_minutes = integer(801);
            state->journey.remaining_minutes = integer(0);
            state->position.flight_level = integer(0);
            state->position.altitude_feet = integer(0);
            state->position.groundspeed_knots = integer(0);
            state->timing.actual_arrival = moment(now - 60);
            state->status.telemetry_state = TELEMETRY_NOT_EXPECTED;
            state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
            state->status.display_state = DISPLAY_TRACKING;
            break;
        case FIXTURE_DELAYED:
            state->status.phase = FLIGHT_PHASE_DELAYED;
            state->status.delayed = true;
            state->journey.progress = 0.0;
            state->journey.airborne_minutes.available = false;
            state->position.flight_level.available = false;
            state->position.groundspeed_knots.available = false;
            state->position.heading_degrees.available = false;
            state->status.telemetry_state = TELEMETRY_NOT_EXPECTED;
            state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
            state->status.display_state = DISPLAY_SCHEDULED;
            break;
        case FIXTURE_STALE:
            state->status.phase = FLIGHT_PHASE_AIRBORNE;
            state->status.stale = true;
            state->status.telemetry_state = TELEMETRY_STALE;
            state->status.phase_source = PHASE_PROVIDER_CONFIRMED;
            state->status.display_state = DISPLAY_STALE;
            state->metadata.last_updated = moment(now - 900);
            state->metadata.telemetry_updated = moment(now - 900);
            break;
        case FIXTURE_UNAVAILABLE:
            state->status.phase = FLIGHT_PHASE_UNAVAILABLE;
            state->status.data_available = false;
            state->status.occurrence_confidence = OCCURRENCE_UNAVAILABLE;
            state->status.telemetry_state = TELEMETRY_UNAVAILABLE;
            state->status.phase_source = PHASE_UNAVAILABLE;
            state->status.display_state = DISPLAY_OFFLINE;
            state->journey.progress_available = false;
            state->journey.progress_source = PROGRESS_UNAVAILABLE;
            state->metadata.last_updated.available = false;
            state->position.latitude.available = false;
            state->position.longitude.available = false;
            state->position.flight_level.available = false;
            state->position.groundspeed_knots.available = false;
            state->position.heading_degrees.available = false;
            state->journey.airborne_minutes.available = false;
            state->journey.remaining_minutes.available = false;
            break;
        case FIXTURE_COUNT: break;
    }
}

void mock_provider_refresh(FlightState *state, FixtureKind fixture, time_t now)
{
    if (fixture == FIXTURE_STALE || fixture == FIXTURE_UNAVAILABLE) return;
    state->metadata.last_updated = moment(now);
    state->metadata.telemetry_updated = state->metadata.last_updated;
    state->metadata.telemetry_received = moment(now);
    if (state->status.phase == FLIGHT_PHASE_CRUISING && state->journey.progress < 0.999) {
        state->journey.progress += 0.0001;
    }
}
