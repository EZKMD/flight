#include "subcell_canvas.h"

#include <stdlib.h>
#include <string.h>

bool subcell_canvas_init(SubcellCanvas *canvas, int columns, int rows)
{
    if (canvas == NULL || columns <= 0 || rows <= 0 ||
        columns > SUBCELL_CANVAS_MAX_COLUMNS || rows > SUBCELL_CANVAS_MAX_ROWS)
        return false;
    memset(canvas, 0, sizeof(*canvas));
    canvas->columns = columns;
    canvas->rows = rows;
    return true;
}

bool subcell_canvas_set(SubcellCanvas *canvas, int x, int y)
{
    static const uint8_t bits[4][2] = {
        { 0x01U, 0x08U }, { 0x02U, 0x10U },
        { 0x04U, 0x20U }, { 0x40U, 0x80U }
    };
    int column;
    int row;
    if (canvas == NULL || x < 0 || y < 0 ||
        x >= canvas->columns * 2 || y >= canvas->rows * 4) return false;
    column = x / 2;
    row = y / 4;
    canvas->cells[row][column] |= bits[y % 4][x % 2];
    return true;
}

void subcell_canvas_line(SubcellCanvas *canvas, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        (void)subcell_canvas_set(canvas, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        if (2 * error >= dy) { error += dy; x0 += sx; }
        if (2 * error <= dx) { error += dx; y0 += sy; }
    }
}

uint32_t subcell_canvas_codepoint(const SubcellCanvas *canvas, int column, int row)
{
    if (canvas == NULL || column < 0 || row < 0 ||
        column >= canvas->columns || row >= canvas->rows) return 0U;
    if (canvas->cells[row][column] == 0U) return (uint32_t)' ';
    return UINT32_C(0x2800) + canvas->cells[row][column];
}
