#ifndef AIRPORT_REFERENCE_H
#define AIRPORT_REFERENCE_H

#include <stdbool.h>
#include "flight_state.h"

typedef enum {
    AIRPORT_VALIDATED,
    AIRPORT_ENRICHED,
    AIRPORT_SINGLE_CODE,
    AIRPORT_MISMATCH,
    AIRPORT_UNKNOWN
} AirportValidation;

AirportValidation airport_reference_normalize(AirportState *airport);
bool airport_reference_pair_valid(const char *iata, const char *icao);

#endif
