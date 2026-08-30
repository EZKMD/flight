#ifndef FLIGHT_CANDIDATE_H
#define FLIGHT_CANDIDATE_H

#include <stddef.h>
#include "flight_resolver.h"

#define FLIGHT_CANDIDATE_LIMIT 50

typedef struct {
    ResolvedFlight flight;
    bool airport_consistent;
    int score;
    char reason[128];
} FlightCandidate;

typedef struct {
    FlightCandidate items[FLIGHT_CANDIDATE_LIMIT];
    size_t count;
} FlightCandidateSet;

int flight_candidate_score(FlightCandidate *candidate, const FlightResolveRequest *request);
ProviderResult flight_candidate_select(FlightCandidateSet *set,
                                       const FlightResolveRequest *request,
                                       ResolvedFlight *selected);

#endif
