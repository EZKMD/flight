#ifndef INPUT_H
#define INPUT_H

typedef enum { INPUT_NONE, INPUT_QUIT, INPUT_REFRESH, INPUT_NEXT_FIXTURE } InputAction;

InputAction input_action_for_key(int key);

#endif
