// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Trace
// ────────────────────────────────────────────────────────────────────────────
//
// Optional debug tracing for handled-key event flow, transition plans, and
// executed effects. This stays compiled out unless both CONSOLE_ENABLE and
// NOAH_KEY_RUNTIME_TRACE_ENABLE are defined.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_transition.h"

void key_runtime_trace_record(const char *stage, uint16_t keycode, const keyrecord_t *record);
void key_runtime_trace_bool_result(const char *stage, uint16_t keycode, const keyrecord_t *record, bool value);
void key_runtime_trace_message(const char *stage, const char *message);
void key_runtime_trace_plan(const char *stage, const key_runtime_transition_plan_t *plan);
void key_runtime_trace_effect_execute(uint8_t index, const key_runtime_transition_effect_t *effect);
