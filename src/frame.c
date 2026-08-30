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

static void copy_clipped(char *destination, const char *source, int maximum_width)
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
    if (frame->count >= FRAME_MAX_LINES) return;
    copy_clipped(frame->lines[frame->count], text, frame->width);
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
