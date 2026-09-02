#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

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
    INPUT_BACK
} InputAction;

typedef struct { unsigned int escape_stage; } InputParser;

InputAction input_action_for_key(int key);
void input_parser_init(InputParser *parser);
bool input_parser_feed(InputParser *parser, int byte, InputAction *action);

#endif
