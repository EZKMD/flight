#include "input.h"

InputAction input_action_for_key(int key)
{
    if (key == 'q' || key == 'Q') return INPUT_QUIT;
    if (key == 'r' || key == 'R') return INPUT_REFRESH;
    if (key == 'f' || key == 'F') return INPUT_NEXT_FIXTURE;
    return INPUT_NONE;
}
