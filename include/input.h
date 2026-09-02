#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    INPUT_NONE,
    INPUT_QUIT,
    INPUT_REFRESH,
    INPUT_NEXT_FIXTURE,
    INPUT_NEXT_VISUAL,
    INPUT_TOGGLE_GEOGRAPHY,
    INPUT_BOARD_UP,
    INPUT_BOARD_DOWN,
    INPUT_BOARD_OPEN,
    INPUT_BOARD_ARRIVALS,
    INPUT_BOARD_DEPARTURES,
    INPUT_BACK,
    INPUT_MOUSE
} InputAction;

#define INPUT_ESCAPE_CAPACITY 32

typedef struct {
    unsigned int escape_stage;
    char escape[INPUT_ESCAPE_CAPACITY];
    size_t escape_length;
    int mouse_column;
    int mouse_row;
    bool mouse_pressed;
} InputParser;

InputAction input_action_for_key(int key);
void input_parser_init(InputParser *parser);
bool input_parser_feed(InputParser *parser, int byte, InputAction *action);

#endif
