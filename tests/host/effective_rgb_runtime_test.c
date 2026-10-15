#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/effective_rgb_runtime.h"

// Publication decodes the profile's lighting into the one program rendering
// reads; a profile without lighting, or one whose reader fails, leaves the
// compiled defaults (none in this build, which has no RGB matrix).

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         calls;
    size_t         fail_after; // 0: never
} instrumented_reader_t;

static bool instrumented_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *context = context_value;

    context->calls++;
    if (context->fail_after != 0u && context->calls > context->fail_after) return false;
    assert(offset <= context->length && length <= context->length - offset);
    if (length != 0u) memcpy(target, &context->bytes[offset], length);
    return true;
}

static size_t read_payload(const char *path, uint8_t *out, size_t capacity) {
    FILE  *file = fopen(path, "rb");
    size_t length;

    assert(file);
    length = fread(out, 1u, capacity, file);
    assert(length > 0u && length < capacity && feof(file));
    fclose(file);
    return length;
}

// The keyboard the shared "standard" conversion vector targets.
static noah_profile_lighting_v1_limits_t standard_limits(void) {
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();

    limits.tap_depth           = 5u;
    limits.logical_layer_count = 16u;
    limits.maximum_brightness  = 200u;
    limits.supported_mode_mask = UINT32_MAX;
    limits.facts               = 0x3eu;
    limits.targets             = 0xffu;
    limits.paints              = 0x7fu;
    limits.timings             = 0x02u;
    limits.rule_extensions     = 0u;
    limits.domain_extensions   = 0x06u;
    limits.things              = 0x1eu;
    return limits;
}

static noah_effective_profile_identity_t identity(uint32_t generation, uint8_t kind) {
    return (noah_effective_profile_identity_t){
        .generation              = generation,
        .payload_crc32           = 0x11223344u + generation,
        .payload_digest          = 0x55667788u + generation,
        .compiled_default_digest = 0x99aabbccu,
        .action_abi_digest       = 0xddeeff00u,
        .origin                  = kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS ? NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED : 0u,
        .kind                    = kind,
    };
}

static noah_effective_profile_snapshot_t lighting_snapshot(noah_effective_profile_identity_t active, noah_profile_lighting_v1_view_t view) {
    noah_effective_profile_snapshot_t snapshot = {0};

    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;
    return snapshot;
}

static void test_install_is_exclusive(void) {
    noah_effective_rgb_runtime_t first;
    noah_effective_rgb_runtime_t second;
    noah_effective_rgb_runtime_t uninitialized = {0};

    noah_effective_rgb_runtime_init(&first);
    noah_effective_rgb_runtime_init(&second);
    assert(first.initialized && !first.loaded);
    assert(!noah_effective_rgb_runtime_install(NULL));
    assert(!noah_effective_rgb_runtime_install(&uninitialized));
    assert(noah_effective_rgb_runtime_install(&first));
    assert(noah_effective_rgb_runtime_install(&first));
    assert(!noah_effective_rgb_runtime_install(&second));
    noah_effective_rgb_runtime_uninstall(&second);
    assert(!noah_effective_rgb_runtime_install(&second));
    noah_effective_rgb_runtime_uninstall(&first);
    assert(noah_effective_rgb_runtime_install(&second));
    noah_effective_rgb_runtime_uninstall(&second);
    // Without RGB there are no compiled defaults to fall back to.
    assert(!noah_effective_lighting_program(NULL)->valid);
}

static void test_publication_decodes_the_profile_lighting(const uint8_t *payload, size_t length) {
    noah_profile_lighting_v1_limits_t         limits = standard_limits();
    instrumented_reader_t                     source = {.bytes = payload, .length = length};
    noah_profile_lighting_v1_view_t           view;
    noah_profile_lighting_v1_error_t          error = {0};
    noah_profile_lighting_v1_program_t        expected;
    noah_effective_rgb_runtime_t              runtime;
    noah_effective_profile_snapshot_t         snapshot;
    const noah_profile_lighting_v1_program_t *program;
    noah_profile_reader_t                     reader = {.read = instrumented_read, .context = &source, .length = length};
    uint32_t                                  before;
    uint32_t                                  after;

    assert(noah_profile_lighting_v1_decode_reader(&reader, 0u, length, &limits, &view, &error) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &expected) && expected.valid);

    noah_effective_rgb_runtime_init(&runtime);
    assert(noah_effective_rgb_runtime_install(&runtime));
    (void)noah_effective_lighting_program(&before);

    snapshot     = lighting_snapshot(identity(1u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), view);
    source.calls = 0u;
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, identity(0u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), snapshot.identity, &snapshot);
    program = noah_effective_lighting_program(&after);
    assert(runtime.loaded);
    assert(after != before);
    assert(source.calls > 0u);
    assert(program->valid && program->length == length && memcmp(program->bytes, payload, length) == 0);
    assert(program->rule_count == expected.rule_count && program->palette_count == expected.palette_count);
    assert(memcmp(program->rules, expected.rules, ((size_t)expected.rule_count + 1u) * sizeof(expected.rules[0])) == 0);
    assert(program->fade_present == expected.fade_present && program->tap_commit_flash == expected.tap_commit_flash);

    // Rendering reads the program, never the profile: reading it again does
    // not touch the reader or move the generation.
    source.calls = 0u;
    assert(noah_effective_lighting_program(&before) == program);
    assert(before == after && source.calls == 0u);
    assert(noah_effective_lighting_generation() == after && source.calls == 0u);

    // Every publication moves the generation, even of the same profile.
    noah_effective_rgb_runtime_invalidate(&runtime, 2u, snapshot.identity, snapshot.identity, &snapshot);
    (void)noah_effective_lighting_program(&before);
    assert(before != after && runtime.loaded);

    noah_effective_rgb_runtime_uninstall(&runtime);
}

static void test_profile_without_lighting_falls_back(const uint8_t *payload, size_t length) {
    noah_profile_lighting_v1_limits_t limits = standard_limits();
    noah_profile_lighting_v1_view_t   view;
    noah_profile_lighting_v1_error_t  error = {0};
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_profile_snapshot_t snapshot;

    assert(noah_profile_lighting_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_LIGHTING_V1_OK);
    noah_effective_rgb_runtime_init(&runtime);
    assert(noah_effective_rgb_runtime_install(&runtime));

    snapshot = lighting_snapshot(identity(3u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), view);
    noah_effective_rgb_runtime_invalidate(&runtime, 3u, identity(2u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), snapshot.identity, &snapshot);
    assert(runtime.loaded);

    // The profile carries no lighting domain.
    snapshot.profile.domain_mask = 0u;
    noah_effective_rgb_runtime_invalidate(&runtime, 4u, identity(3u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), snapshot.identity, &snapshot);
    assert(!runtime.loaded);
    assert(!noah_effective_lighting_program(NULL)->valid);

    // The compiled defaults are active.
    snapshot = lighting_snapshot(identity(5u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), view);
    noah_effective_rgb_runtime_invalidate(&runtime, 5u, identity(4u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), snapshot.identity, &snapshot);
    assert(!runtime.loaded);

    // No snapshot at all.
    noah_effective_rgb_runtime_invalidate(&runtime, 6u, identity(5u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), identity(6u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), NULL);
    assert(!runtime.loaded);

    // An uninitialized runtime ignores publications.
    noah_effective_rgb_runtime_t idle = {0};
    noah_effective_rgb_runtime_invalidate(&idle, 7u, identity(6u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), snapshot.identity, &snapshot);
    assert(!idle.loaded);
    noah_effective_rgb_runtime_uninstall(&runtime);
}

static void test_reader_failure_does_not_publish_a_partial_program(const uint8_t *payload, size_t length) {
    noah_profile_lighting_v1_limits_t limits = standard_limits();
    instrumented_reader_t             source = {.bytes = payload, .length = length};
    noah_profile_reader_t             reader = {.read = instrumented_read, .context = &source, .length = length};
    noah_profile_lighting_v1_view_t   view;
    noah_profile_lighting_v1_error_t  error = {0};
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_profile_snapshot_t snapshot;

    assert(noah_profile_lighting_v1_decode_reader(&reader, 0u, length, &limits, &view, &error) == NOAH_PROFILE_LIGHTING_V1_OK);
    noah_effective_rgb_runtime_init(&runtime);
    assert(noah_effective_rgb_runtime_install(&runtime));

    // The storage read fails part-way through the copy.
    source.calls      = 0u;
    source.fail_after = 2u;
    snapshot          = lighting_snapshot(identity(8u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE), view);
    noah_effective_rgb_runtime_invalidate(&runtime, 8u, identity(7u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), snapshot.identity, &snapshot);
    assert(!runtime.loaded);
    assert(!noah_effective_lighting_program(NULL)->valid);
    noah_effective_rgb_runtime_uninstall(&runtime);
}

int main(int argc, char **argv) {
    static uint8_t payload[NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX + 1u];
    size_t         length;

    assert(argc == 2);
    length = read_payload(argv[1], payload, sizeof(payload));
    assert(length > 64u);

    test_install_is_exclusive();
    test_publication_decodes_the_profile_lighting(payload, length);
    test_profile_without_lighting_falls_back(payload, length);
    test_reader_failure_does_not_publish_a_partial_program(payload, length);
    puts("effective lighting program tests passed");
    return 0;
}
