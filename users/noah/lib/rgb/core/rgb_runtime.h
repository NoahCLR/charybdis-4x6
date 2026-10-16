// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
#    include <stdint.h>
#endif

#include <stdbool.h>
bool noah_rgb_matrix_indicators_user(void);
void noah_rgb_runtime_post_init(void);
void noah_rgb_runtime_housekeeping(void);
void noah_rgb_runtime_suspend(void);
void noah_rgb_runtime_invalidate_layer_maps(void);

#ifdef RGB_RUNTIME_RENDER_TEST_BACKEND
uint16_t rgb_runtime_test_stage_pipeline_count(void);
uint16_t rgb_runtime_test_early_exit_count(void);
uint16_t rgb_runtime_test_resolve_count(void);
#endif
