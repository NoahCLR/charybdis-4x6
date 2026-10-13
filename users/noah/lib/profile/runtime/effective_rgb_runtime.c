// ────────────────────────────────────────────────────────────────────────────
// Effective Lighting Program
// ────────────────────────────────────────────────────────────────────────────

#include "effective_rgb_runtime.h"

#include <string.h>
#ifdef RGB_MATRIX_ENABLE
#    include "../schema/profile_rgb_compiled_v1.h"
#endif

static noah_profile_lighting_v1_program_t program;
static uint32_t                           program_generation;
static noah_effective_rgb_runtime_t      *installed_runtime;

static bool load_compiled(void) {
#ifdef RGB_MATRIX_ENABLE
    const noah_profile_lighting_v1_view_t *compiled = noah_profile_rgb_compiled_v1_view();
    return compiled && noah_profile_lighting_v1_program_load(compiled, &program);
#else
    program.valid = false;
    return false;
#endif
}

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime) {
    if (!runtime) return;
    memset(runtime, 0, sizeof(*runtime));
    runtime->initialized = true;
}

bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime) {
    if (!runtime || !runtime->initialized || (installed_runtime && installed_runtime != runtime)) return false;
    installed_runtime = runtime;
    return true;
}

void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime) {
    if (installed_runtime == runtime) installed_runtime = NULL;
}

void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    noah_effective_rgb_runtime_t *runtime = context;
    bool                          loaded  = false;

    (void)publication_count;
    (void)previous;
    if (!runtime || !runtime->initialized) return;
    if (callback_view && active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && (callback_view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) != 0u) {
        loaded = noah_profile_lighting_v1_program_load(&callback_view->profile.rgb, &program);
    }
    // A profile without lighting, or one that cannot be read, shows the
    // compiled defaults rather than nothing.
    if (!loaded) loaded = load_compiled();
    runtime->loaded = loaded;
    program_generation++;
}

const noah_profile_lighting_v1_program_t *noah_effective_lighting_program(uint32_t *generation) {
    if (!program.valid && (installed_runtime == NULL || !installed_runtime->loaded) && load_compiled()) program_generation++;
    if (generation) *generation = program_generation;
    return &program;
}
