#include "airport_board_renderer.h"

#include <stdio.h>
#include <string.h>

#define FIXTURE_UTC_OFFSET_SECONDS (10 * 60 * 60)

static void local_time_text(time_t value, char output[16], bool seconds)
{
    struct tm local;
    time_t adjusted = value + FIXTURE_UTC_OFFSET_SECONDS;
    if (gmtime_r(&adjusted, &local) == NULL) { (void)snprintf(output, 16, "--:--"); return; }
    (void)strftime(output, 16, seconds ? "%H:%M:%S" : "%H:%M", &local);
}

static FrameStyle status_style(AirportBoardStatus status)
{
    if (status == AIRPORT_STATUS_CHECK_IN || status == AIRPORT_STATUS_BOARDING ||
        status == AIRPORT_STATUS_APPROACHING) return FRAME_STYLE_ACCENT;
    if (status == AIRPORT_STATUS_GATE_CLOSED || status == AIRPORT_STATUS_DELAYED ||
        status == AIRPORT_STATUS_DIVERTED) return FRAME_STYLE_WARNING;
    if (status == AIRPORT_STATUS_CANCELLED) return FRAME_STYLE_DANGER;
    if (status == AIRPORT_STATUS_ARRIVED) return FRAME_STYLE_SUCCESS;
    if (status == AIRPORT_STATUS_DEPARTED) return FRAME_STYLE_DIM;
    return FRAME_STYLE_DEFAULT;
}

static FrameStyle source_style(AirportBoardSourceState state)
{
    if (state == BOARD_LIVE) return FRAME_STYLE_SUCCESS;
    if (state == BOARD_STALE) return FRAME_STYLE_WARNING;
    return FRAME_STYLE_DANGER;
}

static const char *source_label(AirportBoardSourceState state)
{
    if (state == BOARD_LIVE) return "FIXTURE LIVE";
    if (state == BOARD_STALE) return "FIXTURE STALE";
    return "FIXTURE OFFLINE";
}

static const char *activity(const AirportFlightOccurrence *row,
                            const AnimationState *animation)
{
    bool active = animation != NULL && animation->heartbeat != NULL &&
                  strcmp(animation->heartbeat, "•") == 0;
    if (row->status == AIRPORT_STATUS_BOARDING) return active ? " ▶" : " ▸";
    if (row->status == AIRPORT_STATUS_CHECK_IN) return active ? " •" : " ·";
    if (row->status == AIRPORT_STATUS_APPROACHING) return active ? " ↓" : "";
    if (row->status == AIRPORT_STATUS_GATE_CLOSED) return active ? " !" : " .";
    return "";
}

static bool recently_changed(const AirportFlightOccurrence *row)
{
    size_t index;
    for (index = 0U; index < BOARD_CHANGE_COUNT; index++)
        if (row->changes[index].active) return true;
    return false;
}

static OptionalTime display_time(const AirportFlightOccurrence *row,
                                 AirportBoardDirection direction)
{
    if (direction == AIRPORT_BOARD_DEPARTURES) {
        if (row->actual_departure_utc.available) return row->actual_departure_utc;
        if (row->estimated_departure_utc.available) return row->estimated_departure_utc;
        return row->scheduled_departure_utc;
    }
    if (row->actual_arrival_utc.available) return row->actual_arrival_utc;
    if (row->estimated_arrival_utc.available) return row->estimated_arrival_utc;
    return row->scheduled_arrival_utc;
}

static void add_styled(Frame *frame, const char *line, int styled_column,
                       int styled_width, FrameStyle style)
{
    FrameStyle styles[FRAME_LINE_CAPACITY];
    int index;
    (void)memset(styles, FRAME_STYLE_DEFAULT, sizeof(styles));
    for (index = styled_column; index < styled_column + styled_width &&
         index < FRAME_LINE_CAPACITY; index++) if (index >= 0) styles[index] = style;
    frame_add_styled(frame, line, styles);
}

static void add_status_line(Frame *frame, const char *line, int status_column,
                            int status_width, int indicator_column,
                            int indicator_width, FrameStyle status,
                            bool changed)
{
    FrameStyle styles[FRAME_LINE_CAPACITY];
    int index;
    size_t length = (size_t)frame_text_width(line);
    (void)memset(styles, FRAME_STYLE_DEFAULT, sizeof(styles));
    for (index = status_column; index < status_column + status_width &&
         index < FRAME_LINE_CAPACITY; index++) if (index >= 0) styles[index] = status;
    for (index = indicator_column; index < indicator_column + indicator_width &&
         index < FRAME_LINE_CAPACITY; index++) if (index >= 0) styles[index] = status;
    if (changed) {
        size_t start = length > 7U ? length - 7U : 0U;
        for (index = (int)start; index < (int)length && index < FRAME_LINE_CAPACITY; index++)
            styles[index] = FRAME_STYLE_ACCENT;
    }
    frame_add_styled(frame, line, styles);
}

size_t airport_board_visible_capacity(const Layout *layout)
{
    int reserved = layout->mode == LAYOUT_TINY ? 7 : 9;
    int per_row = layout->mode == LAYOUT_COMPACT || layout->mode == LAYOUT_TINY ? 2 : 1;
    int available = layout->height - reserved;
    size_t layout_limit = layout->mode == LAYOUT_TINY ? 3U :
                          layout->mode == LAYOUT_COMPACT ? 5U : 8U;
    size_t fitting;
    if (available < per_row) return 1U;
    fitting = (size_t)(available / per_row);
    return fitting < layout_limit ? fitting : layout_limit;
}

static void render_header(Frame *frame, const AirportBoardState *board,
                          const AnimationState *animation, const Layout *layout)
{
    char left[64], right[64], clock[16];
    const char *direction = board->direction == AIRPORT_BOARD_ARRIVALS ? "ARRIVALS" : "DEPARTURES";
    const char *beat = animation != NULL && animation->heartbeat != NULL ? animation->heartbeat : "·";
    local_time_text(board->local_now, clock, true);
    if (layout->mode == LAYOUT_TINY) {
        (void)snprintf(left, sizeof(left), "%s %s",
                       board->airport.iata, board->direction == AIRPORT_BOARD_ARRIVALS ? "ARR" : "DEP");
    } else (void)snprintf(left, sizeof(left), "%s / %s",
                          board->airport.iata, board->airport.icao);
    (void)snprintf(right, sizeof(right), "%s %s", source_label(board->source_state), beat);
    frame_sides(frame, left, right);
    if (layout->mode != LAYOUT_TINY) {
        frame_center(frame, direction, 0);
        frame_sides(frame, board->airport.name[0] != '\0' ? board->airport.name : "AIRPORT", clock);
    }
}

static void render_now(Frame *frame, time_t now)
{
    char clock[16], text[64];
    local_time_text(now, clock, false);
    (void)snprintf(text, sizeof(text), "──────── NOW %s ────────", clock);
    frame_center(frame, text, 0);
}

static void render_row(Frame *frame, AirportBoardState *board,
                       const AirportFlightOccurrence *row, bool selected,
                       const AnimationState *animation, const Layout *layout)
{
    char time_text[16], line[FRAME_LINE_CAPACITY], second[FRAME_LINE_CAPACITY];
    const AirportState *place = board->direction == AIRPORT_BOARD_ARRIVALS ? &row->origin : &row->destination;
    const char *resource = board->direction == AIRPORT_BOARD_ARRIVALS ? row->baggage_claim : row->departure_gate;
    const char *status = airport_board_status_label(row->status);
    const char *indicator = activity(row, animation);
    FrameStyle style = status_style(row->status);
    OptionalTime shown = display_time(row, board->direction);
    int status_column;
    local_time_text(shown.available ? shown.value : (time_t)0, time_text, false);
    if (layout->mode == LAYOUT_WIDE) {
        (void)snprintf(line, sizeof(line), "%c %-5s  %-8s %-22.22s  T%-3.3s G%-4.4s %-14s%s%s",
                       selected ? '>' : ' ', time_text, row->operating_designator,
                       place->name[0] != '\0' ? place->name : place->iata,
                       board->direction == AIRPORT_BOARD_ARRIVALS ? row->arrival_terminal : row->departure_terminal,
                       resource[0] != '\0' ? resource : "—", status, indicator,
                       recently_changed(row) ? "  UPDATED" : "");
        status_column = 53;
        add_status_line(frame, line, status_column,
                        frame_text_width(status), status_column + 14,
                        frame_text_width(indicator), style,
                        recently_changed(row));
    } else if (layout->mode == LAYOUT_MEDIUM) {
        (void)snprintf(line, sizeof(line), "%c %-5s %-8s %-17.17s %-5.5s %-14s%s%s",
                       selected ? '>' : ' ', time_text, row->operating_designator,
                       place->name[0] != '\0' ? place->name : place->iata,
                       resource[0] != '\0' ? resource : "—", status, indicator,
                       recently_changed(row) ? " *" : "");
        status_column = 41;
        add_status_line(frame, line, status_column,
                        frame_text_width(status), status_column + 14,
                        frame_text_width(indicator), style,
                        recently_changed(row));
    } else {
        (void)snprintf(line, sizeof(line), "%c%s %-8s %-12.12s",
                       selected ? '>' : ' ', time_text, row->operating_designator,
                       place->name[0] != '\0' ? place->name : place->iata);
        frame_add(frame, line);
        (void)snprintf(second, sizeof(second), "  %s%-4.4s  %s%s%s",
                       board->direction == AIRPORT_BOARD_ARRIVALS ? "B" : "G",
                       resource[0] != '\0' ? resource : "—", status, indicator,
                       recently_changed(row) ? "  UPDATED" : "");
        add_styled(frame, second, 8,
                   frame_text_width(status) + frame_text_width(indicator), style);
    }
}

void airport_board_render(Frame *frame, AirportBoardState *board,
                          const AnimationState *animation, const Layout *layout)
{
    AirportBoardStream *stream = airport_board_stream(board);
    size_t capacity = airport_board_visible_capacity(layout), index, rendered = 0U;
    int body_end;
    bool now_rendered = false;
    char footer[FRAME_LINE_CAPACITY], freshness[64];
    frame_init(frame, layout->content_width);
    board->hitbox_count = 0U;
    if (layout->terminal_too_small) { frame_center(frame, "terminal too small", 0); return; }
    render_header(frame, board, animation, layout);
    if (layout->mode != LAYOUT_TINY) {
        frame_blank(frame);
        if (layout->mode == LAYOUT_WIDE)
            frame_add(frame, "  TIME   FLIGHT   DESTINATION / ORIGIN     TERM GATE STATUS");
        else frame_add(frame, "  TIME  FLIGHT   DESTINATION/ORIGIN GATE  STATUS");
    }
    body_end = frame->count + (int)capacity *
               (layout->mode == LAYOUT_COMPACT || layout->mode == LAYOUT_TINY ? 2 : 1) + 1;
    for (index = stream->scroll_offset; index < stream->row_count && rendered < capacity; index++) {
        const AirportFlightOccurrence *row = &stream->rows[index];
        int start;
        if (!now_rendered && airport_board_occurrence_time(row, board->direction) >= board->local_now) {
            render_now(frame, board->local_now);
            now_rendered = true;
        }
        start = frame->count;
        render_row(frame, board, row, strcmp(row->row_id, stream->selected_row_id) == 0,
                   animation, layout);
        if (board->hitbox_count < AIRPORT_BOARD_HITBOX_LIMIT) {
            BoardRowHitbox *hitbox = &board->hitboxes[board->hitbox_count++];
            (void)snprintf(hitbox->row_id, sizeof(hitbox->row_id), "%s", row->row_id);
            hitbox->screen_row_start = start;
            hitbox->screen_row_end = frame->count - 1;
        }
        rendered++;
    }
    if (!now_rendered && stream->row_count > 0U &&
        airport_board_occurrence_time(&stream->rows[stream->row_count - 1U], board->direction) < board->local_now)
        render_now(frame, board->local_now);
    while (frame->count < body_end) frame_blank(frame);
    frame_blank(frame);
    if (layout->mode == LAYOUT_WIDE || layout->mode == LAYOUT_MEDIUM)
        (void)snprintf(footer, sizeof(footer),
                       "↑↓/CLICK SELECT  ENTER OPEN  a ARRIVALS  d DEPARTURES  r REFRESH  q QUIT");
    else (void)snprintf(footer, sizeof(footer), "↑↓/CLICK SELECT  ENTER OPEN  a/d BOARD  q QUIT");
    frame_center(frame, footer, 0);
    (void)snprintf(freshness, sizeof(freshness), "%s · UPDATED %lds AGO",
                   source_label(board->source_state),
                   (long)(board->local_now - board->last_updated));
    add_styled(frame, freshness, 0, (int)strlen(source_label(board->source_state)),
               source_style(board->source_state));
    {
        int visible = frame->count < layout->height ? frame->count : layout->height;
        int top = (layout->height - visible) / 2;
        for (index = 0U; index < board->hitbox_count; index++) {
            board->hitboxes[index].screen_row_start += top;
            board->hitboxes[index].screen_row_end += top;
        }
    }
}
