#ifndef AIRPORT_BOARD_STATE_H
#define AIRPORT_BOARD_STATE_H

#include <stddef.h>
#include "flight_state.h"

#define AIRPORT_BOARD_ROW_LIMIT 64

typedef enum {
    AIRPORT_BOARD_DEPARTURES,
    AIRPORT_BOARD_ARRIVALS
} AirportBoardDirection;

typedef struct {
    char scheduled_time[8];
    char estimated_or_actual_time[8];
    char flight_designator[16];
    char airline[32];
    char origin[8];
    char destination[8];
    char gate[16];
    char terminal[16];
    char status[24];
} AirportFlightRow;

/* Separate from FlightState: this represents many flights at one airport. */
typedef struct {
    AirportState airport;
    AirportBoardDirection direction;
    OptionalTime local_time;
    OptionalTime last_updated;
    char source[32];
    AirportFlightRow rows[AIRPORT_BOARD_ROW_LIMIT];
    size_t row_count;
} AirportBoardState;

#endif
