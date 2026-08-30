#include "layout.h"

Layout layout_select(TerminalSize size)
{
    Layout layout;
    layout.width = size.width > 0 ? size.width : 80;
    layout.height = size.height > 0 ? size.height : 24;

    if (layout.width >= 120) layout.mode = LAYOUT_WIDE;
    else if (layout.width >= 80) layout.mode = LAYOUT_MEDIUM;
    else if (layout.width >= 50) layout.mode = LAYOUT_COMPACT;
    else layout.mode = LAYOUT_TINY;

    layout.content_width = layout.width - 4;
    if (layout.mode == LAYOUT_WIDE && layout.content_width > 104) layout.content_width = 104;
    if (layout.mode == LAYOUT_MEDIUM && layout.content_width > 76) layout.content_width = 76;
    if (layout.content_width < 20) layout.content_width = layout.width;

    layout.show_artwork = layout.height >= 16 && layout.width >= 34;
    layout.use_small_artwork = layout.mode == LAYOUT_TINY || layout.height < 21;
    layout.show_times = layout.height >= 19;
    layout.show_telemetry_labels = layout.height >= 23;
    layout.terminal_too_small = layout.width < LAYOUT_MIN_WIDTH ||
                                layout.height < LAYOUT_MIN_HEIGHT;
    return layout;
}

const char *layout_mode_name(LayoutMode mode)
{
    switch (mode) {
        case LAYOUT_WIDE: return "WIDE";
        case LAYOUT_MEDIUM: return "MEDIUM";
        case LAYOUT_COMPACT: return "COMPACT";
        case LAYOUT_TINY: return "TINY";
    }
    return "UNKNOWN";
}
