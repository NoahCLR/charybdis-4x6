#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "users/noah/noah_keymap_ids.h"
#include "users/noah/lib/profile/runtime/effective_keymap_macros_runtime.h"
#include "users/noah/lib/macro/profile_macro_provider.h"
#include "users/noah/lib/macro/macro_slot_provider.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {{KC_A}}, [15] = {{KC_Z}},
};
const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT] = {[0] = "authored", [127] = "last"};
static unsigned invalidations, windows;
static uint8_t first_text;
static bool fail_reads;
static size_t largest_read;
static uint8_t blob[8 + 6 + 1920 + 6 + 34903];
static bool read_bytes(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    if (length > largest_read) largest_read = length;
    if (fail_reads || offset > sizeof(blob) || length > sizeof(blob) - offset) return false;
    memcpy(target, blob + offset, length);
    return true;
}
void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata) {
    assert(provider->slot_count == 128 && metadata);
    invalidations++;
}
macro_payload_start_result_t macro_slot_provider_start(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source) {
    (void)metadata; (void)text_output; (void)interval; (void)source;
    macro_payload_ir_t ir;
    macro_payload_stream_cursor_t cursor = {0};
    windows = 0;
    first_text = 0;
    do {
        if (!provider->load_ir(slot, &ir, &cursor, provider->context)) return MACRO_PAYLOAD_START_INVALID;
        if (windows++ == 0 && ir.length > 2) first_text = ir.bytes[2];
    } while (ir.more);
    return ir.length ? MACRO_PAYLOAD_START_STARTED : MACRO_PAYLOAD_START_EMPTY;
}
static void u32(uint8_t *bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) bytes[i] = (uint8_t)(value >> (8 * i));
}
static noah_effective_profile_snapshot_t saved(void) {
    memcpy(blob, "NLP1\4\0\2\1", 8);
    blob[8] = 0x60; blob[9] = 1; u32(blob + 10, 1920);
    blob[14] = KC_B;
    blob[14 + 1918] = KC_Y;
    blob[1934] = 0x70; blob[1935] = 1; u32(blob + 1936, 34903);
    memset(blob + 1940, 's', 34903 - 128);
    memset(blob + sizeof(blob) - 128, 0, 128);
    return (noah_effective_profile_snapshot_t){
        .identity = {.kind = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, .generation = 1},
        .reader = {.read = read_bytes, .length = sizeof(blob)},
        .profile = {.domain_mask = NOAH_PROFILE_DOMAIN_MASK_KEYMAP | NOAH_PROFILE_DOMAIN_MASK_MACROS, .byte_length = sizeof(blob)},
    };
}
int main(void) {
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_A);
    assert(keymap_key_to_keycode(15, (keypos_t){0, 0}) == KC_Z);
    assert(keymap_key_to_keycode(16, (keypos_t){0, 0}) == KC_NO);
    assert(keymap_key_to_keycode(0, (keypos_t){MATRIX_ROWS, 0}) == KC_NO);
    assert(keymap_key_to_keycode(0, (keypos_t){0, MATRIX_COLS}) == KC_NO);
    assert(noah_profile_macro_start(QK_MACRO_0));
    assert(windows == 1 && first_text == 'a');
    assert(!noah_profile_macro_try_play(KC_A));
    noah_effective_profile_snapshot_t view = saved();
    noah_effective_keymap_macros_invalidate(NULL, 1, view.identity, view.identity, &view);
    largest_read = 0;
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_B);
    assert(keymap_key_to_keycode(15, (keypos_t){9, 5}) == KC_Y);
    assert(largest_read == 2); // no domain walk or compiled serialization per key
    assert(noah_profile_macro_start(QK_MACRO_0));
    assert(windows > 1 && first_text == 's' && invalidations == 1);
    assert(!noah_profile_macro_start(QK_MACRO_0 + 127)); // empty final slot
    fail_reads = true;
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_NO);
    assert(!noah_profile_macro_start(QK_MACRO_0));
    fail_reads = false;
    // Sparse domains and compiled fallback retain the authored data.
    view.profile.domain_mask = 0;
    noah_effective_keymap_macros_invalidate(NULL, 2, view.identity, view.identity, &view);
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_A);
    assert(noah_profile_macro_start(QK_MACRO_0 + 127));
    assert(first_text == 'l' && windows == 1 && invalidations == 2);
    // A domain bit whose range cannot be found keeps the authored data too,
    // instead of turning every key into KC_NO.
    view = saved();
    blob[8] = 0x61;
    blob[1934] = 0x71;
    noah_effective_keymap_macros_invalidate(NULL, 3, view.identity, view.identity, &view);
    assert(!noah_effective_keymap_is_live() && !noah_effective_macro_bank_is_live());
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_A);
    assert(noah_profile_macro_start(QK_MACRO_0 + 127) && first_text == 'l');
    view = saved();
    view.identity.kind = NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS;
    noah_effective_keymap_macros_invalidate(NULL, 3, view.identity, view.identity, &view);
    assert(keymap_key_to_keycode(0, (keypos_t){0, 0}) == KC_A);
    puts("profile keymap and macro runtime tests passed");
}
