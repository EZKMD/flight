#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_LINE_CAPACITY 512
#define FRAME_MAX_LINES 64
#define FRAME_RENDERED_LINE_CAPACITY 1024

typedef uint8_t FrameStyle;

enum {
    FRAME_STYLE_DEFAULT = 0,
    FRAME_STYLE_DIM,
    FRAME_STYLE_ACCENT,
    FRAME_STYLE_WARNING,
    FRAME_STYLE_DANGER,
    FRAME_STYLE_SUCCESS
};

typedef struct {
    char lines[FRAME_MAX_LINES][FRAME_LINE_CAPACITY];
    FrameStyle styles[FRAME_MAX_LINES][FRAME_LINE_CAPACITY];
    int count;
    int width;
} Frame;

void frame_init(Frame *frame, int width);
int frame_text_width(const char *text);
void frame_add(Frame *frame, const char *text);
void frame_add_styled(Frame *frame, const char *text, const FrameStyle *styles);
void frame_blank(Frame *frame);
void frame_center(Frame *frame, const char *text, int offset);
void frame_sides(Frame *frame, const char *left, const char *right);
void frame_at(Frame *frame, int column, const char *text, const char *suffix);
void frame_at_styled(Frame *frame, int column, const char *text, const char *suffix,
                     FrameStyle style);
bool frame_render_line(const Frame *frame, int row, bool styling_enabled,
                       char *output, size_t capacity);

#endif
