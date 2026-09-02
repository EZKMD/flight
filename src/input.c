#include "input.h"

InputAction input_action_for_key(int key)
{
    if (key == 'q' || key == 'Q') return INPUT_QUIT;
    if (key == 'r' || key == 'R') return INPUT_REFRESH;
    if (key == 'f' || key == 'F') return INPUT_NEXT_FIXTURE;
    if (key == 'v' || key == 'V') return INPUT_NEXT_VISUAL;
    if (key == 'g' || key == 'G') return INPUT_TOGGLE_GEOGRAPHY;
    if (key == 'a' || key == 'A') return INPUT_BOARD_ARRIVALS;
    if (key == 'd' || key == 'D') return INPUT_BOARD_DEPARTURES;
    if (key == 'b' || key == 'B') return INPUT_BACK;
    if (key == '\r' || key == '\n') return INPUT_BOARD_OPEN;
    return INPUT_NONE;
}

void input_parser_init(InputParser *parser) { parser->escape_stage = 0U; }

bool input_parser_feed(InputParser *parser, int byte, InputAction *action)
{
    *action = INPUT_NONE;
    if (parser->escape_stage == 0U) {
        if (byte == 0x1b) { parser->escape_stage = 1U; return false; }
        *action = input_action_for_key(byte);
        return *action != INPUT_NONE;
    }
    if (parser->escape_stage == 1U) {
        parser->escape_stage = byte == '[' ? 2U : 0U;
        return false;
    }
    parser->escape_stage = 0U;
    if (byte == 'A') *action = INPUT_BOARD_UP;
    else if (byte == 'B') *action = INPUT_BOARD_DOWN;
    return *action != INPUT_NONE;
}
