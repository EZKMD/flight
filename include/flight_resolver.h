#ifndef FLIGHT_RESOLVER_H
#define FLIGHT_RESOLVER_H

#include "flight_state.h"
#include "provider_result.h"

typedef struct {
    const char *flight_number;
    const char *date; /* YYYY-MM-DD, empty means most relevant around now. */
    time_t now;
} FlightResolveRequest;

typedef struct {
    AirportState origin;
    AirportState destination;
    FlightTiming timing;
    AircraftState aircraft;
    OptionalInt duration_minutes;
    OptionalInt delay_minutes;
    OptionalTime updated;
    char provider_status[24];
    char departure_date[11];
    char leg_id[96];
} ResolvedFlightLeg;

typedef struct {
    FlightIdentity identity;
    ResolvedFlightLeg selected_leg;
    OccurrenceConfidence confidence;
    int candidate_count;
    int selection_score;
    char occurrence_id[96];
    char selection_reason[128];
} ResolvedFlight;

typedef ProviderResult (*FlightResolveFunction)(void *context,
    const FlightResolveRequest *request, ResolvedFlight *resolved);

typedef struct {
    void *context;
    FlightResolveFunction resolve;
    const char *name;
} FlightResolver;

#endif
