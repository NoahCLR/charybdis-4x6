#pragma once
#include "../rgb/lighting/lighting_working.h"
#include "../rgb/lighting/lighting_events.h"
#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_lighting_snapshot_close(void);
void noah_qmk_lighting_snapshot_init(noah_lighting_working_t *, const uint8_t profile[22]);
void noah_qmk_lighting_snapshot_bind_facts(noah_lighting_facts_t *,noah_lighting_facts_t *);
bool noah_qmk_lighting_snapshot_step(void);
#else
static inline void noah_qmk_lighting_snapshot_close(void) {}
static inline void noah_qmk_lighting_snapshot_init(noah_lighting_working_t *w,const uint8_t p[22]){(void)w;(void)p;}
static inline void noah_qmk_lighting_snapshot_bind_facts(noah_lighting_facts_t *a,noah_lighting_facts_t *b){(void)a;(void)b;}
static inline bool noah_qmk_lighting_snapshot_step(void){return false;}
#endif
