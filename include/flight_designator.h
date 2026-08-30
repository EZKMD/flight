#ifndef FLIGHT_DESIGNATOR_H
#define FLIGHT_DESIGNATOR_H

typedef enum {
    DESIGNATOR_IATA_SUPPORTED,
    DESIGNATOR_PLAUSIBLE_UNSUPPORTED,
    DESIGNATOR_MALFORMED
} FlightDesignatorKind;

FlightDesignatorKind flight_designator_classify(const char *text);

#endif
