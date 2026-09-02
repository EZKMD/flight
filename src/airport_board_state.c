#include "airport_board_state.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t direction_index(AirportBoardDirection direction)
{ return direction == AIRPORT_BOARD_ARRIVALS ? 1U : 0U; }

void airport_board_init(AirportBoardState *board)
{
    (void)memset(board, 0, sizeof(*board));
    board->direction = AIRPORT_BOARD_DEPARTURES;
    board->source_state = BOARD_LIVE;
}

void airport_board_free(AirportBoardState *board)
{
    size_t index;
    if (board == NULL) return;
    for (index = 0U; index < 2U; index++) {
        free(board->streams[index].rows);
        board->streams[index].rows = NULL;
        board->streams[index].row_count = board->streams[index].row_capacity = 0U;
    }
}

AirportBoardStream *airport_board_stream(AirportBoardState *board)
{ return &board->streams[direction_index(board->direction)]; }
const AirportBoardStream *airport_board_stream_const(const AirportBoardState *board)
{ return &board->streams[direction_index(board->direction)]; }

time_t airport_board_occurrence_time(const AirportFlightOccurrence *row,
                                     AirportBoardDirection direction)
{
    OptionalTime value = direction == AIRPORT_BOARD_ARRIVALS ?
                         row->scheduled_arrival_utc : row->scheduled_departure_utc;
    return value.available ? value.value : (time_t)0;
}

const char *airport_board_status_label(AirportBoardStatus status)
{
    static const char *const labels[] = {
        "UNKNOWN", "EXPECTED", "CHECK-IN", "BOARDING", "GATE CLOSED", "DEPARTED",
        "EN ROUTE", "APPROACHING", "ARRIVED", "DELAYED", "CANCELLED", "DIVERTED"
    };
    if (status < AIRPORT_STATUS_UNKNOWN || status > AIRPORT_STATUS_DIVERTED)
        return "UNKNOWN";
    return labels[(int)status];
}

static AirportBoardDirection sort_direction;
static int compare_rows(const void *left, const void *right)
{
    const AirportFlightOccurrence *a = left, *b = right;
    time_t at = airport_board_occurrence_time(a, sort_direction);
    time_t bt = airport_board_occurrence_time(b, sort_direction);
    if (at < bt) return -1;
    if (at > bt) return 1;
    return strcmp(a->row_id, b->row_id);
}

void airport_board_stream_sort(AirportBoardStream *stream, AirportBoardDirection direction)
{
    sort_direction = direction;
    qsort(stream->rows, stream->row_count, sizeof(stream->rows[0]), compare_rows);
}

static bool reserve(AirportBoardStream *stream, size_t needed)
{
    AirportFlightOccurrence *resized;
    size_t capacity = stream->row_capacity == 0U ? 16U : stream->row_capacity;
    while (capacity < needed) capacity *= 2U;
    if (capacity == stream->row_capacity) return true;
    resized = realloc(stream->rows, capacity * sizeof(stream->rows[0]));
    if (resized == NULL) return false;
    stream->rows = resized;
    stream->row_capacity = capacity;
    return true;
}

bool airport_board_stream_merge(AirportBoardStream *stream,
                                const AirportFlightOccurrence *rows, size_t count,
                                AirportBoardDirection direction)
{
    size_t source;
    if (stream == NULL || (count > 0U && rows == NULL) ||
        !reserve(stream, stream->row_count + count)) return false;
    for (source = 0U; source < count; source++) {
        size_t index;
        bool found = false;
        for (index = 0U; index < stream->row_count; index++) {
            if (strcmp(stream->rows[index].row_id, rows[source].row_id) == 0) {
                stream->rows[index] = rows[source];
                found = true;
                break;
            }
        }
        if (!found) stream->rows[stream->row_count++] = rows[source];
    }
    airport_board_stream_sort(stream, direction);
    return true;
}

static size_t selected_index(const AirportBoardStream *stream)
{
    size_t index;
    for (index = 0U; index < stream->row_count; index++)
        if (strcmp(stream->rows[index].row_id, stream->selected_row_id) == 0) return index;
    return stream->row_count;
}

const AirportFlightOccurrence *airport_board_selected(const AirportBoardState *board)
{
    const AirportBoardStream *stream = airport_board_stream_const(board);
    size_t index = selected_index(stream);
    return index < stream->row_count ? &stream->rows[index] : NULL;
}

bool airport_board_select_delta(AirportBoardState *board, int delta, size_t visible_capacity)
{
    AirportBoardStream *stream = airport_board_stream(board);
    size_t index, target;
    if (stream->row_count == 0U) return false;
    index = selected_index(stream);
    if (index == stream->row_count) index = 0U;
    target = index;
    if (delta < 0 && target > 0U) target--;
    else if (delta > 0 && target + 1U < stream->row_count) target++;
    if (target == index && stream->selected_row_id[0] != '\0') return false;
    (void)snprintf(stream->selected_row_id, sizeof(stream->selected_row_id), "%s",
                   stream->rows[target].row_id);
    if (visible_capacity == 0U) visible_capacity = 1U;
    if (target < stream->scroll_offset) stream->scroll_offset = target;
    else if (target >= stream->scroll_offset + visible_capacity)
        stream->scroll_offset = target - visible_capacity + 1U;
    return true;
}

void airport_board_reconcile_selection(AirportBoardStream *stream,
                                       AirportBoardDirection direction,
                                       time_t previous_time)
{
    size_t index, nearest = 0U;
    uint64_t nearest_distance = UINT64_MAX;
    if (stream->row_count == 0U) {
        stream->selected_row_id[0] = '\0';
        stream->scroll_offset = 0U;
        return;
    }
    if (selected_index(stream) < stream->row_count) return;
    for (index = 0U; index < stream->row_count; index++) {
        time_t row_time = airport_board_occurrence_time(&stream->rows[index], direction);
        uint64_t distance = row_time >= previous_time ? (uint64_t)(row_time - previous_time) :
                                                        (uint64_t)(previous_time - row_time);
        if (distance < nearest_distance) { nearest_distance = distance; nearest = index; }
    }
    (void)snprintf(stream->selected_row_id, sizeof(stream->selected_row_id), "%s",
                   stream->rows[nearest].row_id);
    if (stream->scroll_offset >= stream->row_count)
        stream->scroll_offset = stream->row_count - 1U;
}

void airport_board_expire_changes(AirportBoardState *board, time_t now)
{
    size_t stream_index;
    for (stream_index = 0U; stream_index < 2U; stream_index++) {
        size_t row;
        for (row = 0U; row < board->streams[stream_index].row_count; row++) {
            size_t change;
            for (change = 0U; change < BOARD_CHANGE_COUNT; change++) {
                AirportBoardChange *flag = &board->streams[stream_index].rows[row].changes[change];
                if (flag->active && now >= flag->expires_at) flag->active = false;
            }
        }
    }
}
