#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <termios.h>

typedef struct {
    int width;
    int height;
} TerminalSize;

typedef struct {
    struct termios original;
    int original_flags;
    bool active;
    bool mouse_enabled;
} Terminal;

bool terminal_install_resize_handler(void);
bool terminal_resize_pending(void);
TerminalSize terminal_get_size(void);
bool terminal_enter(Terminal *terminal);
void terminal_leave(Terminal *terminal);
void terminal_set_mouse(Terminal *terminal, bool enabled);
int terminal_read_key(void);

#endif
