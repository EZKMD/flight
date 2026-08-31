#include "frame.h"

#include <stdio.h>
#include <string.h>

int frame_text_width(const char *text)
{
    int width = 0;
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if ((*cursor & 0xc0U) != 0x80U) width++;
        cursor++;
    }
    return width;
}

static void copy_clipped(char *destination, FrameStyle *destination_styles,
                         const char *source, const FrameStyle *source_styles,
                         int maximum_width)
{
    int width = 0;
    size_t bytes = 0;
    const unsigned char *cursor = (const unsigned char *)source;
    while (*cursor != '\0' && width < maximum_width && bytes + 1 < FRAME_LINE_CAPACITY) {
        size_t character_bytes = 1;
        if ((*cursor & 0x80U) != 0U) {
            if ((*cursor & 0xe0U) == 0xc0U) character_bytes = 2;
            else if ((*cursor & 0xf0U) == 0xe0U) character_bytes = 3;
            else if ((*cursor & 0xf8U) == 0xf0U) character_bytes = 4;
        }
        if (bytes + character_bytes >= FRAME_LINE_CAPACITY) break;
        (void)memcpy(destination + bytes, cursor, character_bytes);
        destination_styles[width] = source_styles != NULL ?
                                    source_styles[width] : FRAME_STYLE_DEFAULT;
        bytes += character_bytes;
        cursor += character_bytes;
        width++;
    }
    destination[bytes] = '\0';
}

void frame_init(Frame *frame, int width)
{
    frame->count = 0;
    frame->width = width > 0 ? width : 1;
}

void frame_add(Frame *frame, const char *text)
{
    frame_add_styled(frame, text, NULL);
}

void frame_add_styled(Frame *frame, const char *text, const FrameStyle *styles)
{
    if (frame->count >= FRAME_MAX_LINES) return;
    (void)memset(frame->styles[frame->count], FRAME_STYLE_DEFAULT,
                 sizeof(frame->styles[frame->count]));
    copy_clipped(frame->lines[frame->count], frame->styles[frame->count],
                 text, styles, frame->width);
    frame->count++;
}

void frame_blank(Frame *frame) { frame_add(frame, ""); }

void frame_center(Frame *frame, const char *text, int offset)
{
    char line[FRAME_LINE_CAPACITY];
    int padding = (frame->width - frame_text_width(text)) / 2 + offset;
    if (padding < 0) padding = 0;
    (void)snprintf(line, sizeof(line), "%*s%s", padding, "", text);
    frame_add(frame, line);
}

void frame_sides(Frame *frame, const char *left, const char *right)
{
    char line[FRAME_LINE_CAPACITY];
    int gap = frame->width - frame_text_width(left) - frame_text_width(right);
    if (gap < 1) gap = 1;
    (void)snprintf(line, sizeof(line), "%s%*s%s", left, gap, "", right);
    frame_add(frame, line);
}

void frame_at(Frame *frame, int column, const char *text, const char *suffix)
{
    char line[FRAME_LINE_CAPACITY];
    if (column < 0) column = 0;
    (void)snprintf(line, sizeof(line), "%*s%s%s", column, "", text, suffix);
    frame_add(frame, line);
}

void frame_at_styled(Frame *frame, int column, const char *text, const char *suffix,
                     FrameStyle style)
{
    char line[FRAME_LINE_CAPACITY];
    FrameStyle styles[FRAME_LINE_CAPACITY];
    int index;
    int text_width;
    if (column < 0) column = 0;
    (void)memset(styles, FRAME_STYLE_DEFAULT, sizeof(styles));
    (void)snprintf(line, sizeof(line), "%*s%s%s", column, "", text, suffix);
    text_width = frame_text_width(text);
    for (index = column; index < column + text_width &&
         index < FRAME_LINE_CAPACITY; index++) styles[index] = style;
    frame_add_styled(frame, line, styles);
}

static const char *style_sequence(FrameStyle style)
{
    if (style == FRAME_STYLE_DIM) return "\x1b[2m";
    if (style == FRAME_STYLE_ACCENT) return "\x1b[96m";
    return "\x1b[0m";
}

static bool append_bytes(char *output, size_t capacity, size_t *used,
                         const char *bytes, size_t count)
{
    if (*used + count >= capacity) return false;
    (void)memcpy(output + *used, bytes, count);
    *used += count;
    return true;
}

bool frame_render_line(const Frame *frame, int row, bool styling_enabled,
                       char *output, size_t capacity)
{
    const unsigned char *cursor;
    size_t used = 0U;
    int column = 0;
    FrameStyle current = FRAME_STYLE_DEFAULT;
    if (frame == NULL || output == NULL || capacity == 0U ||
        row < 0 || row >= frame->count) return false;
    cursor = (const unsigned char *)frame->lines[row];
    while (*cursor != '\0') {
        size_t character_bytes = 1U;
        FrameStyle desired = frame->styles[row][column];
        if ((*cursor & 0x80U) != 0U) {
            if ((*cursor & 0xe0U) == 0xc0U) character_bytes = 2U;
            else if ((*cursor & 0xf0U) == 0xe0U) character_bytes = 3U;
            else if ((*cursor & 0xf8U) == 0xf0U) character_bytes = 4U;
        }
        if (styling_enabled && desired != current) {
            const char *sequence = style_sequence(desired);
            if (!append_bytes(output, capacity, &used, sequence, strlen(sequence)))
                return false;
            current = desired;
        }
        if (!append_bytes(output, capacity, &used, (const char *)cursor,
                          character_bytes)) return false;
        cursor += character_bytes;
        column++;
    }
    if (styling_enabled && current != FRAME_STYLE_DEFAULT) {
        const char *reset = style_sequence(FRAME_STYLE_DEFAULT);
        if (!append_bytes(output, capacity, &used, reset, strlen(reset))) return false;
    }
    output[used] = '\0';
    return true;
}
