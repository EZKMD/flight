#include "animation.h"

#include <stdio.h>

uint64_t animation_now_ms(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec / UINT64_C(1000000);
}

void animation_init(AnimationState *animation)
{
    uint64_t now = animation_now_ms();
    animation->started_ms = now;
    animation->frame = 0;
    animation->drift = 0;
    animation->heartbeat = "·";
    animation->propulsion = "";
    animation->clock_text[0] = '\0';
}

bool animation_update(AnimationState *animation, uint64_t now_ms)
{
    static const char *const propulsion[] = { "", ".", "..", "..." };
    time_t wall_time = time(NULL);
    struct tm local;
    uint64_t elapsed_ms = now_ms - animation->started_ms;
    const char *old_heartbeat = animation->heartbeat;
    const char *old_propulsion = animation->propulsion;
    int old_drift = animation->drift;

    animation->frame++;
    animation->heartbeat = ((elapsed_ms / UINT64_C(700)) % UINT64_C(3)) == UINT64_C(1) ? "•" : "·";
    animation->propulsion = propulsion[(elapsed_ms / UINT64_C(900)) % UINT64_C(4)];
    animation->drift = ((elapsed_ms / UINT64_C(11000)) % UINT64_C(5)) == UINT64_C(4) ? 1 : 0;

    if (localtime_r(&wall_time, &local) != NULL) {
        (void)strftime(animation->clock_text, sizeof(animation->clock_text), "%H:%M:%S", &local);
    }
    return old_heartbeat != animation->heartbeat || old_propulsion != animation->propulsion ||
           old_drift != animation->drift;
}
