#ifndef AIRPORT_BOARD_FIXTURE_H
#define AIRPORT_BOARD_FIXTURE_H

#include <stdbool.h>
#include "airport_board_state.h"

bool airport_board_fixture_load(AirportBoardState *board, const char *airport_iata);
bool airport_board_fixture_prefetch(AirportBoardState *board);
bool airport_board_fixture_refresh(AirportBoardState *board);
bool airport_board_fixture_open_selected(const AirportBoardState *board,
                                         FlightState *flight);

#endif
