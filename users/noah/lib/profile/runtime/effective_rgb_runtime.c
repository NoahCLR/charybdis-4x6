// ────────────────────────────────────────────────────────────────────────────
// Effective Lighting Program
// ────────────────────────────────────────────────────────────────────────────

#include "effective_rgb_runtime.h"

#include <string.h>
#ifdef RGB_MATRIX_ENABLE
#    include "../schema/profile_rgb_compiled_v1.h"
#    include "../../rgb/lighting/lighting_runtime.h"
#endif

static noah_profile_lighting_v1_program_t program;
static noah_profile_lighting_v1_program_t candidate_program;
static uint32_t                           program_generation;
static noah_effective_rgb_runtime_t      *installed_runtime;

static bool                              candidate_prepared;
static noah_effective_profile_identity_t candidate_identity, active_identity;
static bool                              identity_ready;
static void publish_histories(void) {
#if defined(RGB_MATRIX_ENABLE) && NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
    uint8_t identity[22];
    if (!noah_effective_lighting_identity(identity)) __builtin_trap();
    noah_lighting_runtime_publish(&program, program_generation + 1u, identity);
#endif
}
static bool                              prepare_view(const noah_profile_lighting_v1_view_t *view) {
    noah_lighting_working_cost_t cost;
    return view && noah_profile_lighting_v1_working_admit(view, NOAH_LIGHTING_WORKING_CAPACITY, &cost) == NOAH_PROFILE_LIGHTING_V1_OK && noah_profile_lighting_v1_program_load(view, &candidate_program);
}
static bool prepare_compiled(void) {
#ifdef RGB_MATRIX_ENABLE
    return noah_profile_rgb_compiled_v1_prepare() && prepare_view(noah_profile_rgb_compiled_v1_view());
#else
    candidate_program = (noah_profile_lighting_v1_program_t){0};
    return true;
#endif
}
static bool load_compiled(void) {
    if (!prepare_compiled()) return false;
    program            = candidate_program;
    candidate_prepared = false;
    // Standalone compiled rendering has no provider snapshot. Both halves use
    // this canonical lighting digest; live-owner publication replaces it with
    // the provider's exact complete-profile identity before custom activation.
    uint32_t digest = UINT32_C(2166136261);
    for (uint16_t i = 0; i < program.length; i++)
        digest = (digest ^ program.bytes[i]) * UINT32_C(16777619);
    active_identity = (noah_effective_profile_identity_t){.payload_digest = digest, .compiled_default_digest = digest, .origin = 255u, .kind = NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS};
    identity_ready  = true;
    publish_histories();
    return true;
}
bool noah_effective_rgb_runtime_prepare(const noah_effective_profile_snapshot_t *snapshot) {
    candidate_prepared = false;
    if (!snapshot) return false;
    bool loaded = snapshot->identity.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && (snapshot->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) ? prepare_view(&snapshot->profile.rgb) : prepare_compiled();
    if (!loaded) return false;
    candidate_identity = snapshot->identity;
    candidate_prepared = true;
    return true;
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
    if (candidate_prepared && candidate_identity.generation == active.generation && candidate_identity.payload_digest == active.payload_digest && candidate_identity.payload_crc32 == active.payload_crc32 && candidate_identity.kind == active.kind && candidate_identity.origin == active.origin && candidate_identity.compiled_default_digest == active.compiled_default_digest && candidate_identity.action_abi_digest == active.action_abi_digest) {
        loaded = true;
    } else {
        // Direct bootstrap/standalone callers may prepare synchronously. The
        // installed owner always prepares BEFORE provider publication.
        loaded = callback_view ? noah_effective_rgb_runtime_prepare(callback_view) : prepare_compiled();
    }
    if (!loaded) return;
    program            = candidate_program; // nonfallible commit; no source I/O follows
    candidate_prepared = false;
    active_identity    = active;
    identity_ready     = true;
    runtime->loaded    = program.valid;
    publish_histories();
    program_generation++;
}

const noah_profile_lighting_v1_program_t *noah_effective_lighting_program(uint32_t *generation) {
    if (!program.valid && (installed_runtime == NULL || !installed_runtime->loaded) && load_compiled()) program_generation++;
    if (generation) *generation = program_generation;
    return &program;
}

uint32_t noah_effective_lighting_generation(void) {
    return program_generation;
}

bool noah_effective_lighting_identity(uint8_t bytes[22]) {
    if (!identity_ready || !bytes) return false;
    const uint32_t words[] = {active_identity.generation, active_identity.payload_crc32, active_identity.payload_digest, active_identity.compiled_default_digest, active_identity.action_abi_digest};
    for (uint8_t i = 0; i < 5u; i++)
        for (uint8_t b = 0; b < 4u; b++)
            bytes[i * 4u + b] = (uint8_t)(words[i] >> (b * 8u));
    bytes[20] = active_identity.origin;
    bytes[21] = active_identity.kind;
    return true;
}
