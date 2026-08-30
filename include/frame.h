#ifndef FRAME_H
#define FRAME_H

#define FRAME_LINE_CAPACITY 512
#define FRAME_MAX_LINES 64

typedef struct {
    char lines[FRAME_MAX_LINES][FRAME_LINE_CAPACITY];
    int count;
    int width;
} Frame;

void frame_init(Frame *frame, int width);
int frame_text_width(const char *text);
void frame_add(Frame *frame, const char *text);
void frame_blank(Frame *frame);
void frame_center(Frame *frame, const char *text, int offset);
void frame_sides(Frame *frame, const char *left, const char *right);
void frame_at(Frame *frame, int column, const char *text, const char *suffix);

#endif
