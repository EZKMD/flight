#include "airport_board_fixture.h"

#include "airport_reference.h"
#include "mock_provider.h"

#include <stdio.h>
#include <string.h>

#define FIXTURE_NOW ((time_t)1788256800)
#define WINDOW_SECONDS ((time_t)(6 * 60 * 60))
#define ROWS_PER_WINDOW 8U

static OptionalTime moment(time_t value) { return (OptionalTime){ true, value }; }

static const char *const destinations[] = {
    "SYD", "BNE", "HBA", "SIN", "DXB", "AKL", "PER", "DOH",
    "ADL", "CBR", "LAX", "NRT"
};
static const char *const city_names[] = {
    "SYDNEY", "BRISBANE", "HOBART", "SINGAPORE", "DUBAI", "AUCKLAND",
    "PERTH", "DOHA", "ADELAIDE", "CANBERRA", "LOS ANGELES", "TOKYO NARITA"
};
static const char *const designators[] = {
    "QF492", "VA347", "JQ713", "SQ218", "EK407", "NZ126", "QF9", "QR905",
    "QF691", "VA281", "QF93", "JL774"
};
static const AirportBoardStatus departure_statuses[] = {
    AIRPORT_STATUS_DEPARTED, AIRPORT_STATUS_GATE_CLOSED, AIRPORT_STATUS_BOARDING,
    AIRPORT_STATUS_CHECK_IN, AIRPORT_STATUS_DELAYED, AIRPORT_STATUS_EXPECTED,
    AIRPORT_STATUS_CANCELLED, AIRPORT_STATUS_DIVERTED
};
static const AirportBoardStatus arrival_statuses[] = {
    AIRPORT_STATUS_ARRIVED, AIRPORT_STATUS_ARRIVED, AIRPORT_STATUS_APPROACHING,
    AIRPORT_STATUS_EN_ROUTE, AIRPORT_STATUS_DELAYED, AIRPORT_STATUS_EXPECTED,
    AIRPORT_STATUS_CANCELLED, AIRPORT_STATUS_DIVERTED
};

static void airport(AirportState *value, const char *iata, const char *name)
{
    (void)memset(value, 0, sizeof(*value));
    (void)snprintf(value->iata, sizeof(value->iata), "%s", iata);
    (void)airport_reference_normalize(value);
    if (value->name[0] == '\0') (void)snprintf(value->name, sizeof(value->name), "%s", name);
}

static AirportFlightOccurrence make_row(AirportBoardDirection direction,
                                        int window, size_t offset)
{
    AirportFlightOccurrence row;
    size_t variant = (size_t)((window + 2) * (int)ROWS_PER_WINDOW + (int)offset);
    size_t catalog = variant % (sizeof(destinations) / sizeof(destinations[0]));
    time_t scheduled = FIXTURE_NOW + (time_t)window * WINDOW_SECONDS - (time_t)(90 * 60) +
                       (time_t)offset * (time_t)(45 * 60);
    (void)memset(&row, 0, sizeof(row));
    (void)snprintf(row.row_id, sizeof(row.row_id), "%s:%d:%02zu",
                   direction == AIRPORT_BOARD_ARRIVALS ? "arr" : "dep", window, offset);
    row.identity_confidence = AIRPORT_IDENTITY_HIGH;
    (void)snprintf(row.operating_designator, sizeof(row.operating_designator), "%s",
                   designators[catalog]);
    row.marketing_designator_count = 1U;
    (void)snprintf(row.marketing_designators[0], sizeof(row.marketing_designators[0]),
                   "%s", designators[catalog]);
    airport(&row.origin, direction == AIRPORT_BOARD_ARRIVALS ? destinations[catalog] : "MEL",
            direction == AIRPORT_BOARD_ARRIVALS ? city_names[catalog] : "TULLAMARINE");
    airport(&row.destination, direction == AIRPORT_BOARD_DEPARTURES ? destinations[catalog] : "MEL",
            direction == AIRPORT_BOARD_DEPARTURES ? city_names[catalog] : "TULLAMARINE");
    row.original_destination = row.destination;
    row.scheduled_departure_utc = moment(direction == AIRPORT_BOARD_DEPARTURES ?
                                         scheduled : scheduled - (time_t)(2 * 60 * 60));
    row.scheduled_arrival_utc = moment(direction == AIRPORT_BOARD_ARRIVALS ?
                                       scheduled : scheduled + (time_t)(2 * 60 * 60));
    if (direction == AIRPORT_BOARD_DEPARTURES) {
        row.status = departure_statuses[offset % ROWS_PER_WINDOW];
        (void)snprintf(row.departure_terminal, sizeof(row.departure_terminal), "%d",
                       offset % 3U == 0U ? 2 : 1);
        (void)snprintf(row.departure_gate, sizeof(row.departure_gate), "%zu",
                       4U + (variant % 18U));
        if (row.status == AIRPORT_STATUS_DELAYED)
            row.estimated_departure_utc = moment(scheduled + (time_t)(25 * 60));
    } else {
        row.status = arrival_statuses[offset % ROWS_PER_WINDOW];
        (void)snprintf(row.baggage_claim, sizeof(row.baggage_claim), "%zu",
                       1U + (variant % 7U));
        if (row.status == AIRPORT_STATUS_DELAYED)
            row.estimated_arrival_utc = moment(scheduled + (time_t)(30 * 60));
        if (row.status == AIRPORT_STATUS_ARRIVED)
            row.actual_arrival_utc = moment(scheduled + (time_t)(5 * 60));
    }
    row.cancelled = row.status == AIRPORT_STATUS_CANCELLED;
    row.diverted = row.status == AIRPORT_STATUS_DIVERTED;
    row.fetched_at = FIXTURE_NOW - 6;
    (void)snprintf(row.aircraft_type, sizeof(row.aircraft_type), "A320");
    return row;
}

static bool load_window(AirportBoardState *board, AirportBoardDirection direction, int window)
{
    AirportFlightOccurrence rows[ROWS_PER_WINDOW];
    AirportBoardStream *stream = &board->streams[direction == AIRPORT_BOARD_ARRIVALS ? 1 : 0];
    size_t index;
    for (index = 0U; index < ROWS_PER_WINDOW; index++) rows[index] = make_row(direction, window, index);
    if (!airport_board_stream_merge(stream, rows, ROWS_PER_WINDOW, direction)) return false;
    if (stream->row_count > 0U) {
        stream->loaded_start_time = airport_board_occurrence_time(&stream->rows[0], direction);
        stream->loaded_end_time = airport_board_occurrence_time(&stream->rows[stream->row_count - 1U],
                                                                direction) + 1;
    }
    stream->has_more_before = window > -2 || stream->windows_before_loaded < 2U;
    stream->has_more_after = window < 2 || stream->windows_after_loaded < 2U;
    return true;
}

static void select_near_now(AirportBoardStream *stream, AirportBoardDirection direction)
{
    size_t index;
    for (index = 0U; index < stream->row_count; index++) {
        if (airport_board_occurrence_time(&stream->rows[index], direction) >= FIXTURE_NOW) break;
    }
    if (index >= stream->row_count) index = stream->row_count - 1U;
    (void)snprintf(stream->selected_row_id, sizeof(stream->selected_row_id), "%s",
                   stream->rows[index].row_id);
}

bool airport_board_fixture_load(AirportBoardState *board, const char *airport_iata)
{
    AirportBoardDirection direction;
    if (strcmp(airport_iata, "MEL") != 0) return false;
    airport_board_init(board);
    airport(&board->airport, "MEL", "TULLAMARINE");
    board->local_now = FIXTURE_NOW;
    board->last_updated = FIXTURE_NOW - 6;
    board->source_state = BOARD_LIVE;
    for (direction = AIRPORT_BOARD_DEPARTURES; direction <= AIRPORT_BOARD_ARRIVALS;
         direction = (AirportBoardDirection)((int)direction + 1)) {
        AirportBoardStream *stream = &board->streams[direction == AIRPORT_BOARD_ARRIVALS ? 1 : 0];
        if (!load_window(board, direction, 0)) return false;
        stream->has_more_before = true;
        stream->has_more_after = true;
        select_near_now(stream, direction);
    }
    return true;
}

static size_t selection_index(const AirportBoardStream *stream)
{
    size_t index;
    for (index = 0U; index < stream->row_count; index++)
        if (strcmp(stream->rows[index].row_id, stream->selected_row_id) == 0) return index;
    return stream->row_count;
}

bool airport_board_fixture_prefetch(AirportBoardState *board)
{
    AirportBoardStream *stream = airport_board_stream(board);
    size_t selected = selection_index(stream);
    size_t threshold;
    if (selected >= stream->row_count || stream->row_count == 0U) return false;
    threshold = (stream->row_count + 4U) / 5U;
    if (selected < threshold && stream->has_more_before) {
        int window = -(int)(stream->windows_before_loaded + 1U);
        size_t previous_count = stream->row_count;
        if (window < -2) { stream->has_more_before = false; return false; }
        stream->windows_before_loaded++;
        if (!load_window(board, board->direction, window)) return false;
        stream->scroll_offset += stream->row_count - previous_count;
        stream->has_more_before = stream->windows_before_loaded < 2U;
        return true;
    }
    if (selected + threshold >= stream->row_count && stream->has_more_after) {
        int window = (int)(stream->windows_after_loaded + 1U);
        if (window > 2) { stream->has_more_after = false; return false; }
        stream->windows_after_loaded++;
        if (!load_window(board, board->direction, window)) return false;
        stream->has_more_after = stream->windows_after_loaded < 2U;
        return true;
    }
    return false;
}

static void activate(AirportFlightOccurrence *row, AirportBoardChangeKind kind,
                     time_t now)
{
    row->changes[kind] = (AirportBoardChange){ true, now, now + 15 };
}

static void remove_expired_tombstones(AirportBoardStream *stream,
                                      AirportBoardDirection direction,
                                      time_t now)
{
    const AirportFlightOccurrence *selected = NULL;
    time_t previous_time = now;
    size_t source, destination = 0U;
    for (source = 0U; source < stream->row_count; source++)
        if (strcmp(stream->rows[source].row_id, stream->selected_row_id) == 0)
            selected = &stream->rows[source];
    if (selected != NULL) previous_time = airport_board_occurrence_time(selected, direction);
    for (source = 0U; source < stream->row_count; source++) {
        AirportFlightOccurrence *row = &stream->rows[source];
        if (row->tombstoned && now - row->tombstoned_at >= 60) continue;
        if (destination != source) stream->rows[destination] = stream->rows[source];
        destination++;
    }
    stream->row_count = destination;
    airport_board_reconcile_selection(stream, direction, previous_time);
}

bool airport_board_fixture_refresh(AirportBoardState *board)
{
    AirportBoardStream *departures = &board->streams[0];
    AirportBoardStream *arrivals = &board->streams[1];
    size_t index;
    board->fixture_snapshot = (board->fixture_snapshot + 1U) % 4U;
    board->local_now += 60;
    board->last_updated = board->local_now;
    board->source_state = board->fixture_snapshot == 2U ? BOARD_STALE :
                          board->fixture_snapshot == 3U ? BOARD_OFFLINE : BOARD_LIVE;
    if (board->fixture_snapshot == 1U) {
        AirportFlightOccurrence added = make_row(AIRPORT_BOARD_DEPARTURES, 0, 1U);
        (void)snprintf(added.row_id, sizeof(added.row_id), "dep:refresh:00");
        (void)snprintf(added.operating_designator, sizeof(added.operating_designator), "ZL901");
        added.scheduled_departure_utc = moment(FIXTURE_NOW + (time_t)(5 * 60 * 60));
        added.status = AIRPORT_STATUS_EXPECTED;
        if (!airport_board_stream_merge(departures, &added, 1U,
                                        AIRPORT_BOARD_DEPARTURES)) return false;
    }
    for (index = 0U; index < departures->row_count; index++) {
        AirportFlightOccurrence *row = &departures->rows[index];
        if (strcmp(row->row_id, "dep:0:03") == 0 && board->fixture_snapshot == 1U) {
            (void)snprintf(row->departure_gate, sizeof(row->departure_gate), "14");
            row->status = AIRPORT_STATUS_BOARDING;
            activate(row, BOARD_CHANGE_GATE, board->local_now);
            activate(row, BOARD_CHANGE_STATUS, board->local_now);
        }
        if (strcmp(row->row_id, "dep:0:04") == 0 && board->fixture_snapshot == 1U) {
            row->estimated_departure_utc = moment(row->scheduled_departure_utc.value + 35 * 60);
            activate(row, BOARD_CHANGE_ESTIMATED_TIME, board->local_now);
            activate(row, BOARD_CHANGE_DELAY, board->local_now);
        }
        if (strcmp(row->row_id, "dep:0:05") == 0 && board->fixture_snapshot == 1U) {
            (void)snprintf(row->departure_terminal, sizeof(row->departure_terminal), "3");
            activate(row, BOARD_CHANGE_TERMINAL, board->local_now);
        }
        if (strcmp(row->row_id, "dep:0:06") == 0 && board->fixture_snapshot == 1U) {
            row->marketing_designator_count = 2U;
            (void)snprintf(row->marketing_designators[1],
                           sizeof(row->marketing_designators[1]), "AA7006");
            activate(row, BOARD_CHANGE_CODESHARES, board->local_now);
        }
        if (strcmp(row->row_id, "dep:0:07") == 0 && board->fixture_snapshot == 1U) {
            row->tombstoned = true;
            row->tombstoned_at = board->local_now;
        }
    }
    if (board->fixture_snapshot == 2U) {
        remove_expired_tombstones(departures, AIRPORT_BOARD_DEPARTURES, board->local_now);
        remove_expired_tombstones(arrivals, AIRPORT_BOARD_ARRIVALS, board->local_now);
    }
    for (index = 0U; index < arrivals->row_count; index++) {
        AirportFlightOccurrence *row = &arrivals->rows[index];
        if (strcmp(row->row_id, "arr:0:02") == 0 && board->fixture_snapshot == 1U) {
            (void)snprintf(row->baggage_claim, sizeof(row->baggage_claim), "6");
            activate(row, BOARD_CHANGE_BELT, board->local_now);
        }
        if (strcmp(row->row_id, "arr:0:02") == 0 && board->fixture_snapshot == 2U) {
            row->actual_arrival_utc = moment(row->scheduled_arrival_utc.value + 4 * 60);
            row->status = AIRPORT_STATUS_ARRIVED;
            activate(row, BOARD_CHANGE_ACTUAL_TIME, board->local_now);
            activate(row, BOARD_CHANGE_STATUS, board->local_now);
        }
    }
    airport_board_expire_changes(board, board->local_now);
    return true;
}

bool airport_board_fixture_open_selected(const AirportBoardState *board, FlightState *flight)
{
    const AirportFlightOccurrence *row = airport_board_selected(board);
    time_t now = board->local_now;
    if (row == NULL) return false;
    mock_provider_load(flight, FIXTURE_SCHEDULED, row->operating_designator, now);
    if (row->status == AIRPORT_STATUS_ARRIVED) mock_provider_load(flight, FIXTURE_LANDED,
                                                                  row->operating_designator, now);
    else if (row->status == AIRPORT_STATUS_EN_ROUTE || row->status == AIRPORT_STATUS_APPROACHING ||
             row->status == AIRPORT_STATUS_DEPARTED)
        mock_provider_load(flight, FIXTURE_QF9_CRUISING, row->operating_designator, now);
    flight->origin = row->origin;
    flight->destination = row->destination;
    flight->timing.scheduled_departure = row->scheduled_departure_utc;
    flight->timing.estimated_departure = row->estimated_departure_utc;
    flight->timing.actual_departure = row->actual_departure_utc;
    flight->timing.scheduled_arrival = row->scheduled_arrival_utc;
    flight->timing.estimated_arrival = row->estimated_arrival_utc;
    flight->timing.actual_arrival = row->actual_arrival_utc;
    return true;
}
