#pragma once
#include <stdint.h>

// Optional host operation-count seam. Production has no counters, callbacks or
// storage; these counts never claim target cycles, milliseconds or cadence.
enum { NOAH_LIGHTING_WORK_SEARCH, NOAH_LIGHTING_WORK_MOVE, NOAH_LIGHTING_WORK_CALENDAR, NOAH_LIGHTING_WORK_DESCRIBE, NOAH_LIGHTING_WORK_DIRTY, NOAH_LIGHTING_WORK_KINDS };
#ifdef NOAH_LIGHTING_HISTORY_WORK_TEST_BACKEND
void noah_lighting_history_work(uint8_t kind, uint32_t amount);
#    define NOAH_LIGHTING_HISTORY_WORK(kind, amount) noah_lighting_history_work(kind, amount)
#else
#    define NOAH_LIGHTING_HISTORY_WORK(kind, amount) ((void)0)
#endif
