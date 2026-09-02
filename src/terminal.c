#include "terminal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t resized = 1;

static void on_resize(int signal_number)
{
    (void)signal_number;
    resized = 1;
}

bool terminal_install_resize_handler(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_resize;
    (void)sigemptyset(&action.sa_mask);
    return sigaction(SIGWINCH, &action, NULL) == 0;
}

bool terminal_resize_pending(void)
{
    bool result = resized != 0;
    resized = 0;
    return result;
}

TerminalSize terminal_get_size(void)
{
    struct winsize dimensions;
    TerminalSize size = { 80, 24 };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &dimensions) == 0) {
        if (dimensions.ws_col > 0) size.width = (int)dimensions.ws_col;
        if (dimensions.ws_row > 0) size.height = (int)dimensions.ws_row;
    }
    return size;
}

bool terminal_enter(Terminal *terminal)
{
    struct termios raw;
    terminal->active = false;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &terminal->original) != 0) return false;
    terminal->original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (terminal->original_flags < 0) return false;
    raw = terminal->original;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
    if (fcntl(STDIN_FILENO, F_SETFL, terminal->original_flags | O_NONBLOCK) != 0) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal->original);
        return false;
    }
    terminal->active = true;
    (void)fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
    (void)fflush(stdout);
    return true;
}

void terminal_leave(Terminal *terminal)
{
    if (!terminal->active) return;
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal->original);
    (void)fcntl(STDIN_FILENO, F_SETFL, terminal->original_flags);
    (void)fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    (void)fflush(stdout);
    terminal->active = false;
}

int terminal_read_key(void)
{
    unsigned char key;
    ssize_t count = read(STDIN_FILENO, &key, 1);
    if (count == 1) return (int)key;
    if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return -2;
    return -1;
}
