#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    uint64_t started_ms;
    uint64_t frame;
    int drift;
    const char *heartbeat;
    const char *propulsion;
    char clock_text[16];
} AnimationState;

uint64_t animation_now_ms(void);
void animation_init(AnimationState *animation);
bool animation_update(AnimationState *animation, uint64_t now_ms);

#endif
