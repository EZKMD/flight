#ifndef SUBCELL_CANVAS_H
#define SUBCELL_CANVAS_H

#include <stdbool.h>
#include <stdint.h>

#define SUBCELL_CANVAS_MAX_COLUMNS 104
#define SUBCELL_CANVAS_MAX_ROWS 16

typedef struct {
    int columns;
    int rows;
    uint8_t cells[SUBCELL_CANVAS_MAX_ROWS][SUBCELL_CANVAS_MAX_COLUMNS];
} SubcellCanvas;

bool subcell_canvas_init(SubcellCanvas *canvas, int columns, int rows);
bool subcell_canvas_set(SubcellCanvas *canvas, int x, int y);
void subcell_canvas_line(SubcellCanvas *canvas, int x0, int y0, int x1, int y1);
uint32_t subcell_canvas_codepoint(const SubcellCanvas *canvas, int column, int row);

#endif
