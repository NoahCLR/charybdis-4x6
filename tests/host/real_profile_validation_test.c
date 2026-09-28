#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/noah_runtime.h"
#include "users/noah/lib/key/behavior/keymap_validation.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/rgb/core/rgb_validation.h"

static char log_buffer[16384];

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
extern const pd_mode_color_t pd_mode_colors[];
extern const uint8_t         pd_mode_color_count;
#endif

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

int uprintf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int written = vsnprintf(log_buffer + strlen(log_buffer), sizeof(log_buffer) - strlen(log_buffer), fmt, args);
    va_end(args);

    return written;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode == keycode) {
            return pd_modes[i].mode_flag;
        }
    }

    return 0;
}


bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return true;
}

bool layer_ownership_goto(uint8_t layer) {
    (void)layer;
    return true;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

// Lock keycodes resolve as the firmware's registry does, so a lock placed as a
// step is validated as a pointing lock rather than as a plain keycode.
const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action == action) {
            return &pd_modes[i];
        }
    }

    return NULL;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    return false;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    (void)keycode;
    (void)lease;
    return false;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    (void)lease;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_tap(uint16_t keycode) {
    (void)keycode;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

void pointer_layer_policy_sync_layer_lock_anchor(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

void wait_ms(uint16_t ms) {
    (void)ms;
}

void send_char(char ascii_code) {
    (void)ascii_code;
}

void send_char_with_delay(char ascii_code, uint8_t interval) {
    (void)ascii_code;
    (void)interval;
}

const uint8_t ascii_to_shift_lut[16];
const uint8_t ascii_to_altgr_lut[16];
const uint8_t ascii_to_dead_lut[16];
const uint8_t ascii_to_keycode_lut[128];

uint32_t timer_read32(void) {
    return 0u;
}

void noah_runtime_diag_heartbeat(void) {}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return keymaps[layer_num][row][column];
}

// Optional evidence export uses the real C layout and macro encoder, not a
// source parser. The host LAYOUT stub stores its 56 arguments in row order.
static void export_authored_via(void) {
    const char *path = getenv("NOAH_AUTHORED_VIA_DUMP");
    if (!path) return;
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t key = 0; key < 56; key++) {
            uint16_t value = keymaps[layer][key / MATRIX_COLS][key % MATRIX_COLS];
            CHECK(fputc(value >> 8, file) != EOF);
            CHECK(fputc(value & 255, file) != EOF);
        }
    }
    for (uint8_t slot = 0; slot < VIA_MACRO_SLOT_COUNT; slot++) {
        uint8_t bytes[2048];
        uint16_t length;
        CHECK(macro_payload_encode(via_macro_payloads[slot], bytes, sizeof(bytes), &length));
        CHECK(fwrite(bytes, 1, length, file) == length);
        CHECK(fputc(0, file) != EOF);
    }
    CHECK(fclose(file) == 0);
}

// The names settings v4 can hold: a macro name is printable ASCII, a layer
// name UTF-8 without control characters, each ending before its field does.
static void check_authored_names(void) {
    for (uint8_t slot = 0; slot < VIA_MACRO_SLOT_COUNT; slot++) {
        const char *name = via_macro_names[slot];
        CHECK(memchr(name, '\0', NOAH_MACRO_NAME_SIZE) != NULL);
        for (const char *c = name; *c; c++)
            CHECK(*c >= 0x20 && *c <= 0x7e);
    }
    // Custom-key names follow the macro rule: plain ASCII, as settings v5 stores them.
    for (uint8_t slot = 0; slot < CUSTOM_KEY_SLOT_COUNT; slot++) {
        const char *name = custom_key_names[slot];
        CHECK(memchr(name, '\0', NOAH_MACRO_NAME_SIZE) != NULL);
        for (const char *c = name; *c; c++)
            CHECK(*c >= 0x20 && *c <= 0x7e);
    }
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        const unsigned char *name = (const unsigned char *)layer_names[layer];
        CHECK(memchr(name, '\0', NOAH_LAYER_NAME_SIZE) != NULL);
        for (size_t i = 0; name[i];) {
            unsigned char lead = name[i];
            size_t        extra = lead < 0x80 ? 0 : lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : SIZE_MAX;
            CHECK(extra != SIZE_MAX && lead >= 0x20 && lead != 0x7f);
            for (size_t k = 1; k <= extra; k++)
                CHECK((name[i + k] & 0xc0) == 0x80);
            i += extra + 1;
        }
    }
}

int main(void) {
    export_authored_via();
    check_authored_names();
#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    CHECK(pd_mode_color_count == PD_MODE_COUNT);
#endif

    uint8_t keymap_validation_errors = noah_keymap_validate();
    if (keymap_validation_errors != 0u && log_buffer[0] != '\0') {
        fputs(log_buffer, stderr);
    }
    CHECK(keymap_validation_errors == 0u);
    noah_rgb_validate_config();

    if (log_buffer[0] != '\0') {
        fputs(log_buffer, stderr);
        test_fail("log_buffer is empty", __FILE__, __LINE__);
    }

    puts("real profile validation host tests passed");
    return 0;
}
