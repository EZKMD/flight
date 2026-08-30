#include "runtime.h"

static bool due(uint64_t *deadline, uint64_t now_ms, uint64_t interval_ms)
{
    if (now_ms < *deadline) return false;
    *deadline = now_ms + interval_ms;
    return true;
}

void runtime_schedule_init(RuntimeSchedule *schedule, uint64_t now_ms)
{
    schedule->next_animation_ms = now_ms;
    schedule->next_render_ms = now_ms;
    schedule->next_data_refresh_ms = now_ms + UINT64_C(15000);
}

bool runtime_animation_due(RuntimeSchedule *schedule, uint64_t now_ms)
{
    return due(&schedule->next_animation_ms, now_ms, UINT64_C(100));
}

bool runtime_render_due(RuntimeSchedule *schedule, uint64_t now_ms)
{
    return due(&schedule->next_render_ms, now_ms, UINT64_C(1000));
}

bool runtime_data_due(RuntimeSchedule *schedule, uint64_t now_ms, uint64_t interval_ms)
{
    return due(&schedule->next_data_refresh_ms, now_ms, interval_ms);
}

void runtime_defer_data(RuntimeSchedule *schedule, uint64_t now_ms, uint64_t delay_ms)
{
    schedule->next_data_refresh_ms = now_ms + delay_ms;
}
