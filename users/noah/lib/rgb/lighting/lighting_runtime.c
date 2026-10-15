#include "lighting_runtime.h"
#if defined(RGB_MATRIX_ENABLE) && NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
#    include "lighting_workspace.h"
#    include "lighting_occurrence.h"
#    include "../../profile/runtime/effective_rgb_runtime.h"
#    include "../../compat/qmk_lighting_snapshot_transport.h"

static uint8_t                 storage[NOAH_LIGHTING_WORKING_CAPACITY];
static noah_lighting_facts_t   snapshot_facts;
static noah_lighting_working_t working;
static uint32_t                revision;
static bool                    role, initialized;
__attribute__((noinline)) void noah_lighting_runtime_publish(const noah_profile_lighting_v1_program_t *program, uint32_t generation, const uint8_t identity[22]) {
    bool master = is_keyboard_master();
    noah_qmk_lighting_snapshot_close();
    // Activation preflight has already admitted and copied this exact program.
    // No input producer can run recursively here or from a collector/renderer.
    if (!noah_lighting_working_begin(&working, program, storage, sizeof storage, generation)) __builtin_trap();
    role        = master;
    initialized = true;
    revision++;
    noah_qmk_lighting_snapshot_init(&working, identity);
    noah_qmk_lighting_snapshot_bind_facts(&snapshot_facts, noah_rgb_runtime_history_facts());
}
void noah_lighting_runtime_sync_role(void) {
    // Pinned QMK fixes the role in split_pre_init. This also provides a shallow
    // main boundary for an explicit role reinitialization, before producers.
    if (initialized && role != is_keyboard_master()) {
        uint8_t identity[22];
        if (!noah_effective_lighting_identity(identity)) __builtin_trap();
        noah_lighting_runtime_publish(working.program, working.generation, identity);
    }
}
bool noah_lighting_runtime_prepare(noah_lighting_scene_t *scene, uint32_t generation, uint32_t now) {
    if (!initialized || working.program != scene->program || working.generation != generation || role != is_keyboard_master()) __builtin_trap();
    noah_lighting_working_advance(&working, now);
    if (role) {
        if (!noah_lighting_working_observe(&working, scene->facts)) __builtin_trap();
    } else
        noah_lighting_working_seen(&working);
    revision++;
    scene->working = working.rules ? &working : NULL;
    return true;
}
static __attribute__((noinline)) void qualify_and_retain(const noah_lighting_event_t *event) {
    if (noah_lighting_event_qualify(working.program, &noah_lighting_workspace.capture.facts, event, 1u, &noah_lighting_workspace.capture.group)) {
        if (!noah_lighting_working_event(&working, &noah_lighting_workspace.capture.group)) __builtin_trap();
        revision++;
    }
}
__attribute__((noinline)) void noah_lighting_event_observe(const noah_lighting_event_t *event) {
    if (!event || !is_keyboard_master()) return;
    // Publication/startup initializes these owned objects before exposing its
    // generation. The input path never lazily loads or decodes a program.
    if (!initialized || role != is_keyboard_master() || working.generation != noah_effective_lighting_generation()) __builtin_trap();
    if (!working.rules) return;
    uint32_t now = timer_read32();
    noah_lighting_working_advance(&working, now);
    revision++; // Timer changes also invalidate an already published chunk cut.
    noah_lighting_facts_collect(&noah_lighting_workspace.capture.facts, true, true);
    qualify_and_retain(event);
}
void noah_lighting_runtime_task(void) {
    noah_lighting_runtime_sync_role();
    if (!initialized || !working.rules) return;
    uint32_t now = timer_read32();
    noah_lighting_working_advance(&working, now);
    revision++;
    if (role && !working.snapshot_ready) {
        noah_lighting_facts_collect(&snapshot_facts, true, true);
        noah_lighting_working_advance(&working, timer_read32());
        if (!noah_lighting_working_observe(&working, &snapshot_facts)) __builtin_trap();
    }
    if (noah_qmk_lighting_snapshot_step()) revision++;
}
bool noah_lighting_runtime_remote_scene(void) {
    return initialized && !role && working.rules && working.generation == noah_effective_lighting_generation() && role == is_keyboard_master();
}
uint32_t noah_lighting_runtime_revision(void) {
    return revision;
}
#endif
