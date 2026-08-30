#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include "terminal.h"

typedef enum {
    LAYOUT_WIDE,
    LAYOUT_MEDIUM,
    LAYOUT_COMPACT,
    LAYOUT_TINY
} LayoutMode;

typedef struct {
    LayoutMode mode;
    int width;
    int height;
    int content_width;
    bool show_artwork;
    bool use_small_artwork;
    bool show_times;
    bool show_telemetry_labels;
    bool terminal_too_small;
} Layout;

#define LAYOUT_MIN_WIDTH 32
#define LAYOUT_MIN_HEIGHT 9

Layout layout_select(TerminalSize size);
const char *layout_mode_name(LayoutMode mode);

#endif
