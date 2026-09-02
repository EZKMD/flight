#ifndef AIRPORT_BOARD_STATE_H
#define AIRPORT_BOARD_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "flight_state.h"

#define AIRPORT_BOARD_ROW_ID_CAPACITY 64
#define AIRPORT_BOARD_MARKETING_LIMIT 4
#define AIRPORT_BOARD_HITBOX_LIMIT 64

typedef enum { AIRPORT_BOARD_DEPARTURES, AIRPORT_BOARD_ARRIVALS } AirportBoardDirection;
typedef enum {
    AIRPORT_STATUS_UNKNOWN, AIRPORT_STATUS_EXPECTED, AIRPORT_STATUS_CHECK_IN,
    AIRPORT_STATUS_BOARDING, AIRPORT_STATUS_GATE_CLOSED, AIRPORT_STATUS_DEPARTED,
    AIRPORT_STATUS_EN_ROUTE, AIRPORT_STATUS_APPROACHING, AIRPORT_STATUS_ARRIVED,
    AIRPORT_STATUS_DELAYED, AIRPORT_STATUS_CANCELLED, AIRPORT_STATUS_DIVERTED
} AirportBoardStatus;
typedef enum { AIRPORT_IDENTITY_HIGH, AIRPORT_IDENTITY_MEDIUM,
               AIRPORT_IDENTITY_LOW } AirportBoardIdentityConfidence;
typedef enum { BOARD_LIVE, BOARD_STALE, BOARD_OFFLINE } AirportBoardSourceState;
typedef enum {
    BOARD_CHANGE_GATE, BOARD_CHANGE_TERMINAL, BOARD_CHANGE_STATUS,
    BOARD_CHANGE_DELAY, BOARD_CHANGE_BELT, BOARD_CHANGE_ESTIMATED_TIME,
    BOARD_CHANGE_ACTUAL_TIME, BOARD_CHANGE_CODESHARES, BOARD_CHANGE_COUNT
} AirportBoardChangeKind;

typedef struct { bool active; time_t detected_at; time_t expires_at; } AirportBoardChange;

typedef struct {
    char row_id[AIRPORT_BOARD_ROW_ID_CAPACITY];
    char provider_occurrence_id[64];
    bool has_provider_occurrence_id;
    AirportBoardIdentityConfidence identity_confidence;
    char operating_designator[16];
    char marketing_designators[AIRPORT_BOARD_MARKETING_LIMIT][16];
    size_t marketing_designator_count;
    AirportState origin;
    AirportState destination;
    AirportState original_destination;
    OptionalTime scheduled_departure_utc, estimated_departure_utc, actual_departure_utc;
    OptionalTime scheduled_arrival_utc, estimated_arrival_utc, actual_arrival_utc;
    char departure_terminal[16], departure_gate[16];
    char arrival_terminal[16], arrival_gate[16], baggage_claim[16];
    AirportBoardStatus status;
    bool cancelled, diverted, tombstoned;
    time_t tombstoned_at;
    char registration[16], aircraft_type[32];
    time_t fetched_at;
    AirportBoardChange changes[BOARD_CHANGE_COUNT];
} AirportFlightOccurrence;

typedef struct {
    AirportFlightOccurrence *rows;
    size_t row_count, row_capacity;
    char selected_row_id[AIRPORT_BOARD_ROW_ID_CAPACITY];
    size_t scroll_offset;
    time_t loaded_start_time, loaded_end_time;
    bool has_more_before, has_more_after;
    unsigned int windows_before_loaded, windows_after_loaded;
} AirportBoardStream;

typedef struct {
    char row_id[AIRPORT_BOARD_ROW_ID_CAPACITY];
    int screen_row_start, screen_row_end;
} BoardRowHitbox;

typedef struct {
    AirportState airport;
    AirportBoardDirection direction;
    AirportBoardStream streams[2];
    time_t local_now, last_updated;
    AirportBoardSourceState source_state;
    unsigned int fixture_snapshot;
    BoardRowHitbox hitboxes[AIRPORT_BOARD_HITBOX_LIMIT];
    size_t hitbox_count;
} AirportBoardState;

void airport_board_init(AirportBoardState *board);
void airport_board_free(AirportBoardState *board);
AirportBoardStream *airport_board_stream(AirportBoardState *board);
const AirportBoardStream *airport_board_stream_const(const AirportBoardState *board);
time_t airport_board_occurrence_time(const AirportFlightOccurrence *row,
                                     AirportBoardDirection direction);
const char *airport_board_status_label(AirportBoardStatus status);
bool airport_board_stream_merge(AirportBoardStream *stream,
                                const AirportFlightOccurrence *rows, size_t count,
                                AirportBoardDirection direction);
void airport_board_stream_sort(AirportBoardStream *stream,
                               AirportBoardDirection direction);
const AirportFlightOccurrence *airport_board_selected(const AirportBoardState *board);
bool airport_board_select_delta(AirportBoardState *board, int delta,
                                size_t visible_capacity);
void airport_board_reconcile_selection(AirportBoardStream *stream,
                                       AirportBoardDirection direction,
                                       time_t previous_time);
void airport_board_expire_changes(AirportBoardState *board, time_t now);

#endif
