#ifndef MOCK_PROVIDER_H
#define MOCK_PROVIDER_H

#include <stdbool.h>
#include <time.h>
#include "flight_state.h"

typedef enum {
    FIXTURE_QF9_CRUISING,
    FIXTURE_SCHEDULED,
    FIXTURE_DESCENDING,
    FIXTURE_LANDED,
    FIXTURE_DELAYED,
    FIXTURE_STALE,
    FIXTURE_UNAVAILABLE,
    FIXTURE_COUNT
} FixtureKind;

const char *mock_provider_fixture_name(FixtureKind fixture);
bool mock_provider_parse_fixture(const char *name, FixtureKind *fixture);
FixtureKind mock_provider_next_fixture(FixtureKind fixture);
void mock_provider_load(FlightState *state, FixtureKind fixture, const char *flight_number,
                        time_t now);
void mock_provider_refresh(FlightState *state, FixtureKind fixture, time_t now);

#endif
