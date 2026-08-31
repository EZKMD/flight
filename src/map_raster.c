#include "map_raster.h"

#include "subcell_canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int cell_x(double value, int columns)
{
    return clamp_int((int)(value * (double)(columns - 1) + 0.5), 0, columns - 1);
}

static int cell_y(double value, int rows)
{
    return clamp_int((int)(value * (double)(rows - 1) + 0.5), 0, rows - 1);
}

static void overlay_markers(MapRaster *raster, const MapPoint *route, size_t route_count,
                            const MapPoint *marker)
{
    int origin_x = cell_x(route[0].x, raster->columns);
    int origin_y = cell_y(route[0].y, raster->rows);
    int destination_x = cell_x(route[route_count - 1U].x, raster->columns);
    int destination_y = cell_y(route[route_count - 1U].y, raster->rows);
    raster->cells[origin_y][origin_x] = UINT32_C(0x25cf);
    raster->cells[destination_y][destination_x] = UINT32_C(0x25cf);
    if (marker != NULL) {
        raster->marker_column = cell_x(marker->x, raster->columns);
        raster->marker_row = cell_y(marker->y, raster->rows);
        raster->marker_available = true;
        raster->cells[raster->marker_row][raster->marker_column] = UINT32_C(0x25c6);
    }
}

static bool render_braille(MapRaster *raster, const MapPoint *route, size_t route_count)
{
    SubcellCanvas canvas;
    size_t index;
    int row;
    int column;
    if (!subcell_canvas_init(&canvas, raster->columns, raster->rows)) return false;
    for (index = 1U; index < route_count; index++) {
        int x0 = clamp_int((int)(route[index - 1U].x *
                           (double)(raster->columns * 2 - 1) + 0.5),
                           0, raster->columns * 2 - 1);
        int y0 = clamp_int((int)(route[index - 1U].y *
                           (double)(raster->rows * 4 - 1) + 0.5),
                           0, raster->rows * 4 - 1);
        int x1 = clamp_int((int)(route[index].x *
                           (double)(raster->columns * 2 - 1) + 0.5),
                           0, raster->columns * 2 - 1);
        int y1 = clamp_int((int)(route[index].y *
                           (double)(raster->rows * 4 - 1) + 0.5),
                           0, raster->rows * 4 - 1);
        subcell_canvas_line(&canvas, x0, y0, x1, y1);
    }
    for (row = 0; row < raster->rows; row++)
        for (column = 0; column < raster->columns; column++)
            raster->cells[row][column] = subcell_canvas_codepoint(&canvas, column, row);
    return true;
}

static uint32_t direction_glyph(int dx, int dy)
{
    if (dy == 0) return UINT32_C(0x2500);
    if (dx == 0) return UINT32_C(0x2502);
    return (dx > 0) == (dy > 0) ? UINT32_C(0x2572) : UINT32_C(0x2571);
}

static void merge_glyph(MapRaster *raster, int x, int y, uint32_t glyph)
{
    uint32_t existing;
    if (x < 0 || y < 0 || x >= raster->columns || y >= raster->rows) return;
    existing = raster->cells[y][x];
    if (existing == (uint32_t)' ' || existing == glyph) raster->cells[y][x] = glyph;
    else raster->cells[y][x] = UINT32_C(0x253c);
}

static void compat_line(MapRaster *raster, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int previous_x = x0;
    int previous_y = y0;
    for (;;) {
        int move_x = x0 - previous_x;
        int move_y = y0 - previous_y;
        uint32_t glyph = direction_glyph(move_x, move_y);
        if (move_x != 0 || move_y != 0) {
            merge_glyph(raster, previous_x, previous_y, glyph);
            merge_glyph(raster, x0, y0, glyph);
        }
        if (x0 == x1 && y0 == y1) break;
        previous_x = x0;
        previous_y = y0;
        if (2 * error >= dy) { error += dy; x0 += sx; }
        if (2 * error <= dx) { error += dx; y0 += sy; }
    }
}

static bool render_compat(MapRaster *raster, const MapPoint *route, size_t route_count)
{
    size_t index;
    for (index = 1U; index < route_count; index++)
        compat_line(raster,
                    cell_x(route[index - 1U].x, raster->columns),
                    cell_y(route[index - 1U].y, raster->rows),
                    cell_x(route[index].x, raster->columns),
                    cell_y(route[index].y, raster->rows));
    for (index = 1U; index + 1U < route_count; index++) {
        int previous_x = cell_x(route[index - 1U].x, raster->columns);
        int previous_y = cell_y(route[index - 1U].y, raster->rows);
        int current_x = cell_x(route[index].x, raster->columns);
        int current_y = cell_y(route[index].y, raster->rows);
        int next_x = cell_x(route[index + 1U].x, raster->columns);
        int next_y = cell_y(route[index + 1U].y, raster->rows);
        bool incoming_horizontal = previous_y == current_y && previous_x != current_x;
        bool outgoing_horizontal = next_y == current_y && next_x != current_x;
        bool incoming_vertical = previous_x == current_x && previous_y != current_y;
        bool outgoing_vertical = next_x == current_x && next_y != current_y;
        if ((incoming_horizontal && outgoing_vertical) ||
            (incoming_vertical && outgoing_horizontal)) {
            bool connects_left = previous_x < current_x || next_x < current_x;
            bool connects_down = previous_y > current_y || next_y > current_y;
            if (connects_left && connects_down)
                raster->cells[current_y][current_x] = UINT32_C(0x256e);
            else if (connects_left)
                raster->cells[current_y][current_x] = UINT32_C(0x256f);
            else if (connects_down)
                raster->cells[current_y][current_x] = UINT32_C(0x256d);
            else raster->cells[current_y][current_x] = UINT32_C(0x2570);
        }
    }
    return true;
}

bool map_raster_render(MapRaster *raster, MapRasterBackend backend,
                       const MapPoint *route, size_t route_count,
                       const MapPoint *marker, int columns, int rows)
{
    if (raster == NULL || route == NULL || route_count < 2U ||
        columns <= 1 || rows <= 1 || columns > MAP_RASTER_MAX_COLUMNS ||
        rows > MAP_RASTER_MAX_ROWS) return false;
    memset(raster, 0, sizeof(*raster));
    raster->columns = columns;
    raster->rows = rows;
    {
        int row;
        int column;
        for (row = 0; row < rows; row++)
            for (column = 0; column < columns; column++)
                raster->cells[row][column] = (uint32_t)' ';
    }
    if (backend == MAP_RASTER_BRAILLE) {
        if (!render_braille(raster, route, route_count)) return false;
    } else if (!render_compat(raster, route, route_count)) return false;
    overlay_markers(raster, route, route_count, marker);
    return true;
}

static size_t encode_utf8(uint32_t codepoint, char output[4])
{
    if (codepoint <= UINT32_C(0x7f)) { output[0] = (char)codepoint; return 1U; }
    if (codepoint <= UINT32_C(0x7ff)) {
        output[0] = (char)(0xc0U | (codepoint >> 6));
        output[1] = (char)(0x80U | (codepoint & 0x3fU));
        return 2U;
    }
    output[0] = (char)(0xe0U | (codepoint >> 12));
    output[1] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
    output[2] = (char)(0x80U | (codepoint & 0x3fU));
    return 3U;
}

bool map_raster_row_utf8(const MapRaster *raster, int row, char *output, size_t capacity)
{
    size_t used = 0U;
    int column;
    if (raster == NULL || output == NULL || capacity == 0U ||
        row < 0 || row >= raster->rows) return false;
    for (column = 0; column < raster->columns; column++) {
        char encoded[4];
        size_t bytes = encode_utf8(raster->cells[row][column], encoded);
        if (used + bytes >= capacity) return false;
        (void)memcpy(output + used, encoded, bytes);
        used += bytes;
    }
    output[used] = '\0';
    return true;
}
