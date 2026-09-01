#ifndef APP_MODE_H
#define APP_MODE_H

/* Airport mode owns a multi-flight board; it is not a single-flight visual. */
typedef enum {
    APP_MODE_FLIGHT,
    APP_MODE_AIRPORT
} AppMode;

#endif
