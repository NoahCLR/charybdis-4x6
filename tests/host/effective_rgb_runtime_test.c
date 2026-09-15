#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/effective_rgb_runtime.h"

enum {
    TEST_BUFFER_SIZE = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE + 8u,
};

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         calls;
} instrumented_reader_t;

static bool fixture_value(const char *path, const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(path, "r");
    char   line[TEST_BUFFER_SIZE * 2u + 64u];
    size_t key_length = strlen(key);

    assert(file);
    while (fgets(line, sizeof(line), file)) {
        size_t length;
        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') continue;
        length = strcspn(&line[key_length + 1u], "\r\n");
        assert(length + 1u <= capacity);
        memcpy(value, &line[key_length + 1u], length);
        value[length] = '\0';
        fclose(file);
        return true;
    }
    fclose(file);
    return false;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    abort();
}

static size_t fixture_hex(const char *path, const char *key, uint8_t *output, size_t capacity) {
    char   encoded[TEST_BUFFER_SIZE * 2u + 1u];
    size_t length;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    length = strlen(encoded);
    assert((length % 2u) == 0u && length / 2u <= capacity);
    for (size_t index = 0u; index < length; index += 2u) {
        output[index / 2u] = (uint8_t)((hex_nibble(encoded[index]) << 4u) | hex_nibble(encoded[index + 1u]));
    }
    return length / 2u;
}

static unsigned fixture_uint(const char *path, const char *key) {
    char          encoded[32];
    char         *end;
    unsigned long value;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    value = strtoul(encoded, &end, 10);
    assert(*encoded != '\0' && *end == '\0' && value <= UINT16_MAX);
    return (unsigned)value;
}

static bool instrumented_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *context = context_value;

    context->calls++;
    assert(offset <= context->length && length <= context->length - offset);
    if (length != 0u) memcpy(target, &context->bytes[offset], length);
    return true;
}

static noah_profile_rgb_v1_limits_t fixture_limits(const char *path) {
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();

    limits.compiled_stage_mask    = (uint16_t)fixture_uint(path, "codec.compiled_stage_mask");
    limits.logical_layer_count    = (uint8_t)fixture_uint(path, "codec.logical_layer_count");
    limits.maximum_brightness     = (uint8_t)fixture_uint(path, "codec.maximum_brightness");
    limits.tap_branch_color_count = (uint8_t)fixture_uint(path, "codec.tap_branch_color_count");
    limits.supported_pd_mode_mask = (uint8_t)fixture_uint(path, "codec.supported_pd_mode_mask");
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

static noah_effective_profile_snapshot_t live_rgb_snapshot(noah_effective_profile_identity_t active, noah_profile_rgb_v1_view_t view) {
    noah_effective_profile_snapshot_t snapshot = {0};

    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;
    return snapshot;
}

static noah_effective_profile_snapshot_t fallback_snapshot(noah_effective_profile_identity_t active) {
    noah_effective_profile_snapshot_t snapshot = {0};

    snapshot.identity = active;
    return snapshot;
}

static void test_init_install_and_fail_closed_capture(void) {
    noah_effective_rgb_runtime_t first;
    noah_effective_rgb_runtime_t second;
    noah_effective_rgb_frame_t   frame;

    assert(noah_effective_rgb_capture_frame(&frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);
    assert(noah_effective_rgb_frame_status(&frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);
    assert(!noah_effective_rgb_runtime_install(NULL));

    noah_effective_rgb_runtime_init(&first);
    noah_effective_rgb_runtime_init(&second);
    assert(noah_effective_rgb_runtime_capture_frame(&first, &frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);
    assert(noah_effective_rgb_runtime_frame_status(&first, &frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);
    assert(noah_effective_rgb_runtime_install(&first));
    assert(noah_effective_rgb_runtime_install(&first));
    assert(!noah_effective_rgb_runtime_install(&second));
    assert(noah_effective_rgb_capture_frame(&frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);

    first.publication_sequence = 1u;
    assert(noah_effective_rgb_runtime_capture_frame(&first, &frame) == NOAH_EFFECTIVE_RGB_BUSY);
    assert(!frame.valid);
    first.publication_sequence = 2u;

    noah_effective_rgb_runtime_uninstall(&second);
    assert(!noah_effective_rgb_runtime_install(&second));
    noah_effective_rgb_runtime_uninstall(&first);
    assert(noah_effective_rgb_runtime_install(&second));
    noah_effective_rgb_runtime_uninstall(&second);
}

static void test_callback_only_live_view_and_frame_staleness(const char *fixture_path) {
    uint8_t                           payload[TEST_BUFFER_SIZE];
    size_t                            payload_length = fixture_hex(fixture_path, "payload.hex", payload, sizeof(payload));
    instrumented_reader_t             reader_context = {.bytes = payload, .length = payload_length};
    noah_profile_reader_t             reader         = {.read = instrumented_read, .context = &reader_context, .length = payload_length};
    noah_profile_rgb_v1_limits_t      limits         = fixture_limits(fixture_path);
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_rgb_frame_t        live_frame;
    noah_effective_rgb_frame_t        fallback_frame;
    noah_effective_profile_identity_t live_identity = identity(7u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE);
    noah_effective_profile_snapshot_t live          = live_rgb_snapshot(live_identity, (noah_profile_rgb_v1_view_t){0});
    noah_profile_rgb_v1_layer_color_t layer;
    size_t                            calls_after_decode;

    assert(noah_profile_rgb_v1_decode_reader(&reader, 0u, payload_length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    live.profile.rgb   = view;
    calls_after_decode = reader_context.calls;

    noah_effective_rgb_runtime_init(&runtime);
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, identity(0u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), live_identity, &live);
    assert(reader_context.calls == calls_after_decode);
    assert(noah_effective_rgb_runtime_capture_frame(&runtime, &live_frame) == NOAH_EFFECTIVE_RGB_OK);
    assert(live_frame.valid && live_frame.live);
    assert(live_frame.identity.generation == 7u);
    assert(live_frame.publication_count == 1u);
    assert(noah_effective_rgb_runtime_frame_status(&runtime, &live_frame) == NOAH_EFFECTIVE_RGB_OK);
    assert(reader_context.calls == calls_after_decode);
    assert(noah_profile_rgb_v1_layer_color_at(&live_frame.view, 2u, &layer, &error) == NOAH_PROFILE_RGB_V1_OK);
    assert(reader_context.calls > calls_after_decode);
    assert(layer.layer_id == 2u && layer.color.h == 40u && layer.color.s == 50u && layer.color.v == 60u && layer.mode == 1u);

    noah_effective_profile_identity_t behavior_only_identity = identity(8u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE);
    noah_effective_profile_snapshot_t behavior_only          = fallback_snapshot(behavior_only_identity);
    noah_effective_rgb_runtime_invalidate(&runtime, 2u, live_identity, behavior_only_identity, &behavior_only);
    assert(noah_effective_rgb_runtime_frame_status(&runtime, &live_frame) == NOAH_EFFECTIVE_RGB_STALE);
    assert(noah_effective_rgb_runtime_capture_frame(&runtime, &fallback_frame) == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK);
    assert(fallback_frame.valid && !fallback_frame.live && fallback_frame.publication_count == 2u);

    noah_effective_rgb_runtime_invalidate(&runtime, 3u, behavior_only_identity, behavior_only_identity, &behavior_only);
    assert(noah_effective_rgb_runtime_frame_status(&runtime, &fallback_frame) == NOAH_EFFECTIVE_RGB_STALE);
}

static void test_invalid_callback_identity_fails_closed(void) {
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_rgb_frame_t        frame;
    noah_effective_profile_identity_t active     = identity(3u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE);
    noah_effective_profile_snapshot_t mismatched = fallback_snapshot(identity(4u, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE));

    noah_effective_rgb_runtime_init(&runtime);
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, identity(0u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS), active, &mismatched);
    assert(noah_effective_rgb_runtime_capture_frame(&runtime, &frame) == NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT);
    assert(frame.valid == false);
    assert(noah_effective_rgb_runtime_frame_status(&runtime, &frame) == NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_init_install_and_fail_closed_capture();
    test_callback_only_live_view_and_frame_staleness(argv[1]);
    test_invalid_callback_identity_fails_closed();
    puts("effective RGB runtime tests: PASS");
    return 0;
}
