// ────────────────────────────────────────────────────────────────────────────
// Effective Lighting Program
// ────────────────────────────────────────────────────────────────────────────
//
// The published lighting domain, decoded into RAM once per publication
// (docs/architecture/lighting-rules-v1.md, "Publication"). Rendering reads
// only this program, never the profile. Without a live-profile owner, or
// before its first publication, the program is the compiled defaults.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "effective_profile_provider.h"
#include "../schema/profile_lighting_v1.h"

// The owner's publication state. The program itself is one per firmware.
typedef struct {
    bool initialized;
    bool loaded; // the last publication produced a program
} noah_effective_rgb_runtime_t;

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime);
bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime);
void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime);

// Decodes the published profile's lighting, or the compiled defaults when the
// profile carries none.
void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view);

// The program to render, and a generation that changes with every
// publication.
const noah_profile_lighting_v1_program_t *noah_effective_lighting_program(uint32_t *generation);

// RAM-only publication stamp, including between chunks. Unlike obtaining the
// program, this never lazily loads defaults or touches a profile reader.
uint32_t noah_effective_lighting_generation(void);

// Owner safe-boundary preflight. Reads/admission finish before provider commit.
bool noah_effective_rgb_runtime_prepare(const noah_effective_profile_snapshot_t *snapshot);

// RAM-only exact published provider identity for lighting split agreement.
bool noah_effective_lighting_identity(uint8_t bytes[22]);
