#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t next_animation_ms;
    uint64_t next_render_ms;
    uint64_t next_data_refresh_ms;
} RuntimeSchedule;

void runtime_schedule_init(RuntimeSchedule *schedule, uint64_t now_ms);
bool runtime_animation_due(RuntimeSchedule *schedule, uint64_t now_ms);
bool runtime_render_due(RuntimeSchedule *schedule, uint64_t now_ms);
bool runtime_data_due(RuntimeSchedule *schedule, uint64_t now_ms, uint64_t interval_ms);
void runtime_defer_data(RuntimeSchedule *schedule, uint64_t now_ms, uint64_t delay_ms);

#endif
