// The autocorrect engine through the report path: keys go down and up through
// the ledger and QMK's report (stubbed here as the macro ownership test stubs
// it), the real report hooks, macro engine and typing stage run, and a model
// of the computer turns every report it is sent into text.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/autocorrect/autocorrect.h"
#include "users/noah/lib/autocorrect/autocorrect_switch.h"
#include "users/noah/lib/macro/host_layout.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/lib/macro/macro_slot_provider.h"
#include "users/noah/lib/macro/typing_stage.h"
#include "users/noah/lib/profile/schema/profile_autocorrect_v1.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "%s:%d: %s (case %s)\n", __FILE__, __LINE__, #condition, case_name); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

static char case_name[96] = "none";

// ── The keyboard side: QMK's report, as live ownership makes it ─────────────

keymap_config_t keymap_config;

static uint32_t now;
static uint8_t  raw_keys[32], physical_mods, modifier_owners[8], oneshot_mods;
static uint8_t  layout_id;
static bool     layout_macos, caps_lock;
static unsigned reports_sent, backspaces_sent, characters_sent, taps_sent;
static uint32_t last_report_at;

bool keyboard_report_mods_override_user(uint8_t *mods);

uint32_t macro_payload_host_setting(void) {
    return (uint32_t)layout_id << 16u;
}
uint8_t macro_payload_host_os(void) {
    return 0u;
}
bool macro_payload_unicode_caps_lock(void) {
    return caps_lock;
}
uint32_t timer_read32(void) {
    return now;
}
uint32_t timer_elapsed32(uint32_t before) {
    return now - before;
}
void wait_ms(uint16_t delay) {
    (void)delay;
}
void pointer_layer_policy_note_action(uint16_t key, bool pressed) {
    (void)key;
    (void)pressed;
}
bool keyboard_mod_ownership_can_register_mods(uint8_t mods) {
    (void)mods;
    return true;
}
bool keyboard_mod_ownership_can_unregister_mods(uint8_t mods) {
    for (uint8_t i = 0; i < 8; i++)
        if ((mods & (1u << i)) && modifier_owners[i] == 0u) return false;
    return true;
}
bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    bool changed = false;
    for (uint8_t i = 0; i < 8; i++)
        if (mods & (1u << i)) {
            changed |= modifier_owners[i] == 0u;
            modifier_owners[i]++;
        }
    send_keyboard_report();
    return changed;
}
void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    for (uint8_t i = 0; i < 8; i++)
        if (mods & (1u << i)) {
            assert(modifier_owners[i]);
            modifier_owners[i]--;
        }
    send_keyboard_report();
}
uint8_t get_mods(void) {
    uint8_t mods = physical_mods;
    for (uint8_t i = 0; i < 8; i++)
        if (modifier_owners[i]) mods |= (uint8_t)(1u << i);
    return mods;
}
uint8_t get_weak_mods(void) {
    return 0u;
}
uint8_t get_oneshot_mods(void) {
    return oneshot_mods;
}
void clear_oneshot_mods(void) {
    oneshot_mods = 0u;
}
void register_code(uint8_t key) {
    // A mouse button leaves on the mouse report.
    if (IS_MOUSEKEY_BUTTON(key)) return;
    raw_keys[key / 8u] |= (uint8_t)(1u << (key % 8u));
    send_keyboard_report();
}
void unregister_code(uint8_t key) {
    if (IS_MOUSEKEY_BUTTON(key)) return;
    raw_keys[key / 8u] &= (uint8_t)~(1u << (key % 8u));
    send_keyboard_report();
}
void tap_code16(uint16_t key) {
    (void)key;
    assert(false);
}

// ── The computer: text, a cursor, the layout and Caps Lock ──────────────────

static uint16_t screen[512];
static unsigned screen_length, cursor;
static uint8_t  seen_keys[32], seen_mods;
static uint16_t host_dead;

static void host_insert(uint16_t character) {
    CHECK(screen_length < sizeof(screen) / sizeof(screen[0]));
    memmove(&screen[cursor + 1u], &screen[cursor], (screen_length - cursor) * sizeof(screen[0]));
    screen[cursor++] = character;
    screen_length++;
    characters_sent++;
}

static uint16_t host_caps(uint16_t character) {
    uint32_t other;
    if (!caps_lock) return character;
    if (noah_profile_autocorrect_v1_capital(character, &other)) return (uint16_t)other;
    if (!layout_macos && noah_profile_autocorrect_v1_fold(character, &other)) return (uint16_t)other;
    return character;
}

static void host_key(uint8_t usage, uint8_t mods) {
    const host_layout_t *layout   = host_layout_get(layout_id);
    uint8_t              altgr    = layout_macos ? MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_RIGHT_ALT) : MOD_BIT(KC_RIGHT_ALT);
    uint16_t             stroke   = (uint16_t)(usage | ((mods & (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_RIGHT_SHIFT))) ? HOST_LAYOUT_STROKE_SHIFT : 0u) | ((mods & altgr) ? HOST_LAYOUT_STROKE_ALTGR : 0u));
    uint16_t             character = 0u, dead_character = 0u;

    if (mods & (uint8_t)~(MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_RIGHT_SHIFT) | altgr)) return; // A shortcut types nothing.
    switch (usage) {
        case KC_BACKSPACE:
            backspaces_sent++;
            if (cursor) {
                memmove(&screen[cursor - 1u], &screen[cursor], (screen_length - cursor) * sizeof(screen[0]));
                cursor--;
                screen_length--;
            }
            return;
        case KC_DELETE:
            if (cursor < screen_length) {
                memmove(&screen[cursor], &screen[cursor + 1u], (screen_length - cursor - 1u) * sizeof(screen[0]));
                screen_length--;
            }
            return;
        case KC_LEFT:
            if (cursor) cursor--;
            return;
        case KC_RIGHT:
            if (cursor < screen_length) cursor++;
            return;
        case KC_HOME:
            cursor = 0u;
            return;
        case KC_END:
            cursor = screen_length;
            return;
        case KC_ENTER:
            host_insert('\n');
            return;
        case KC_TAB:
            host_insert('\t');
            return;
        default:
            break;
    }
    if (host_dead) {
        uint16_t dead = host_dead;
        host_dead     = 0u;
        if (host_layout_compose(layout, dead, stroke, &character)) {
            host_insert(host_caps(character));
            return;
        }
        // An unresolved dead key shows its own character, then the key's.
        if (host_layout_compose(layout, dead, KC_SPACE, &dead_character)) host_insert(dead_character);
    }
    switch (host_layout_stroke_types(layout, stroke, &character)) {
        case HOST_LAYOUT_TYPES_CHARACTER:
            host_insert(host_caps(character));
            break;
        case HOST_LAYOUT_TYPES_DEAD_KEY:
            host_dead = stroke;
            break;
        default:
            break;
    }
}

void send_keyboard_report(void) {
    report_nkro_t nkro = {0};
    uint8_t       keys[32] = {0}, mods;

    memcpy(nkro.bits, raw_keys, sizeof(nkro.bits));
    nkro_report_keys_filter_user(&nkro);
    memcpy(keys, nkro.bits, sizeof(nkro.bits));
    mods = (uint8_t)(get_mods() | oneshot_mods);
    // QMK spends a one-shot modifier on a report with keys, unless overridden.
    if (!keyboard_report_mods_override_user(&mods)) {
        for (uint8_t i = 0; i < 32; i++)
            if (keys[i]) oneshot_mods = 0u;
    }
    if (memcmp(keys, seen_keys, sizeof(keys)) == 0 && mods == seen_mods) return;
    reports_sent++;
    last_report_at = now;
    for (uint16_t key = 1; key < 240; key++)
        if ((keys[key / 8u] & (1u << (key % 8u))) && !(seen_keys[key / 8u] & (1u << (key % 8u)))) host_key((uint8_t)key, mods);
    memcpy(seen_keys, keys, sizeof(keys));
    seen_mods = mods;
}

// ── The list and macros ─────────────────────────────────────────────────────

static uint8_t                            list_bytes[4096];
static noah_profile_autocorrect_v1_list_t list;

const noah_profile_autocorrect_v1_list_t *noah_active_profile_autocorrect_list(void) {
    return &list;
}

static macro_payload_ir_t macro_ir;
static char               macro_text[64];

// The slot's macro, as the provider would start it.
bool noah_profile_macro_start(uint16_t action) {
    size_t length = strlen(macro_text);

    (void)action;
    macro_ir          = (macro_payload_ir_t){.protection = 2u, .length = (uint16_t)(length + 2u)};
    macro_ir.bytes[0] = MACRO_PAYLOAD_IR_OP_TEXT;
    macro_ir.bytes[1] = (uint8_t)length;
    memcpy(&macro_ir.bytes[2], macro_text, length);
    return macro_payload_start_ir(&macro_ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 2u, MACRO_PAYLOAD_SOURCE_SLOT, 0u, NULL, NULL) == MACRO_PAYLOAD_START_STARTED;
}

// A macro key, as noah_profile_macro_try_play handles it.
static void macro_key(void) {
    if (!noah_typing_stage_defer_macro(0u)) (void)noah_profile_macro_start(QK_MACRO);
}

// ── Driving a case ──────────────────────────────────────────────────────────

static void scan(unsigned milliseconds) {
    for (unsigned i = 0; i < milliseconds; i++) {
        now++;
        macro_payload_engine_scan();
        noah_autocorrect_scan();
    }
}

// Until the macro engine is idle and no report has gone out for a while.
static void settle(void) {
    scan(12u);
    for (unsigned guard = 0; !macro_payload_engine_idle() || now - last_report_at < 12u; guard++) {
        CHECK(guard < 20000u);
        scan(1u);
    }
}

static void key(uint8_t usage, bool down) {
    keyrecord_t record   = {.event = {.type = KEY_EVENT, .pressed = down, .key = {.row = 0, .col = 0}}};
    bool        suppress = owned_keycode_should_suppress_default(usage, &record);

    owned_keycode_track_physical_event(usage, &record);
    if (suppress) return;
    if (down) register_code(usage);
    else unregister_code(usage);
}

static void modifiers(uint8_t mods, bool down) {
    if (!mods) return;
    if (down) physical_mods |= mods;
    else physical_mods &= (uint8_t)~mods;
    send_keyboard_report();
}

// A tap with time to spare, or one with a few milliseconds between its edges
// and none after it.
static void tap(uint8_t usage, uint8_t mods, bool fast) {
    taps_sent++;
    modifiers(mods, true);
    scan(fast ? 1u : 5u);
    key(usage, true);
    scan(fast ? 3u : 20u);
    key(usage, false);
    scan(fast ? 1u : 5u);
    modifiers(mods, false);
    if (fast) scan(2u);
    else settle();
}

static void reset(void) {
    macro_payload_engine_init();
    noah_autocorrect_init();
    owned_keycode_reset_for_test();
    memset(raw_keys, 0, sizeof(raw_keys));
    memset(seen_keys, 0, sizeof(seen_keys));
    memset(modifier_owners, 0, sizeof(modifier_owners));
    physical_mods = seen_mods = oneshot_mods = 0u;
    screen_length = cursor = 0u;
    host_dead              = 0u;
    reports_sent = backspaces_sent = characters_sent = taps_sent = 0u;
    caps_lock                                        = false;
    layout_id                                        = 0u;
    layout_macos                                     = false;
    keymap_config.autocorrect_enable                 = true;
    autocorrect_switch_set_override(true);
    now                                              = 1000u;
    last_report_at                                   = 0u;
}

static size_t unhex(const char *text, uint8_t *bytes, size_t capacity) {
    size_t length = 0u;
    if (strcmp(text, "-") == 0) return 0u;
    for (; text[0] && text[1]; text += 2) {
        unsigned value;
        CHECK(length < capacity && sscanf(text, "%2x", &value) == 1);
        bytes[length++] = (uint8_t)value;
    }
    return length;
}

static void use_list(const uint8_t *bytes, size_t length) {
    noah_profile_reader_t reader = noah_profile_reader_from_memory(bytes, length);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, length));
}

static size_t screen_utf8(char *out, size_t capacity) {
    size_t length = 0u;
    for (unsigned i = 0; i < screen_length; i++) {
        uint16_t c = screen[i];
        CHECK(length + 3u < capacity);
        if (c < 0x80u) {
            out[length++] = (char)c;
        } else if (c < 0x800u) {
            out[length++] = (char)(0xC0u | (c >> 6u));
            out[length++] = (char)(0x80u | (c & 0x3Fu));
        } else {
            out[length++] = (char)(0xE0u | (c >> 12u));
            out[length++] = (char)(0x80u | ((c >> 6u) & 0x3Fu));
            out[length++] = (char)(0x80u | (c & 0x3Fu));
        }
    }
    out[length] = '\0';
    return length;
}

static void expect_screen(const char *expected, bool only_end) {
    char   shown[1600];
    size_t length = screen_utf8(shown, sizeof(shown)), wanted = strlen(expected);

    settle();
    length = screen_utf8(shown, sizeof(shown));
    if (!(only_end ? length >= wanted && strcmp(shown + length - wanted, expected) == 0 : strcmp(shown, expected) == 0)) {
        fprintf(stderr, "case %s: the computer shows \"%s\", expected %s\"%s\"\n", case_name, shown, only_end ? "it to end with " : "", expected);
        exit(1);
    }
    // Nothing is left held or held back.
    for (uint8_t i = 0; i < 32; i++)
        CHECK(seen_keys[i] == 0u);
    CHECK(seen_mods == 0u);
}

static unsigned run_fixture(const char *path) {
    static char line[9000];
    FILE       *file  = fopen(path, "r");
    unsigned    cases = 0u;

    assert(file);
    while (fgets(line, sizeof(line), file)) {
        char     word[16], text[8200];
        unsigned a = 0u, b = 0u, c = 0u, d = 0u;

        line[strcspn(line, "\n")] = '\0';
        CHECK(sscanf(line, "%15s", word) == 1);
        if (strcmp(word, "CASE") == 0) {
            reset();
            snprintf(case_name, sizeof(case_name), "%s", line + 5);
            cases++;
        } else if (strcmp(word, "LAYOUT") == 0) {
            CHECK(sscanf(line, "LAYOUT %u %u", &a, &b) == 2 && host_layout_get((uint8_t)a));
            layout_id    = (uint8_t)a;
            layout_macos = b != 0u;
        } else if (strcmp(word, "ON") == 0) {
            CHECK(sscanf(line, "ON %u", &a) == 1);
            keymap_config.autocorrect_enable = a != 0u;
        } else if (strcmp(word, "CAPS") == 0) {
            CHECK(sscanf(line, "CAPS %u", &a) == 1);
            caps_lock = a != 0u;
        } else if (strcmp(word, "LIST") == 0) {
            CHECK(sscanf(line, "LIST %8199s", text) == 1);
            use_list(list_bytes, unhex(text, list_bytes, sizeof(list_bytes)));
        } else if (strcmp(word, "TAP") == 0 || strcmp(word, "FAST") == 0) {
            CHECK(sscanf(line, "%*s %u %u", &a, &b) == 2);
            tap((uint8_t)a, (uint8_t)b, word[0] == 'F');
        } else if (strcmp(word, "PRESS") == 0 || strcmp(word, "RELEASE") == 0) {
            CHECK(sscanf(line, "%*s %u", &a) == 1);
            modifiers((uint8_t)a, word[0] == 'P');
            settle();
        } else if (strcmp(word, "CLICK") == 0) {
            owned_keycode_lease_t click = {0};
            CHECK(owned_keycode_acquire(QK_MOUSE_BUTTON_1, &click));
            scan(20u);
            CHECK(owned_keycode_release(&click));
            settle();
        } else if (strcmp(word, "HOLD") == 0) {
            CHECK(sscanf(line, "HOLD %u %u %u %u", &a, &b, &c, &d) == 4);
            modifiers((uint8_t)b, true);
            key((uint8_t)a, true);
            scan(c);
            // The computer repeats the key it still sees down.
            CHECK(seen_keys[a / 8u] & (1u << (a % 8u)));
            for (unsigned i = 0; i < d; i++)
                host_key((uint8_t)a, seen_mods);
            key((uint8_t)a, false);
            modifiers((uint8_t)b, false);
            settle();
        } else if (strcmp(word, "MACRO") == 0) {
            uint8_t bytes[sizeof(macro_text)];
            CHECK(sscanf(line, "MACRO %u %8199s", &a, text) == 2);
            size_t length = unhex(text, bytes, sizeof(bytes) - 1u);
            memcpy(macro_text, bytes, length);
            macro_text[length] = '\0';
            macro_key();
            if (a) scan(2u);
            else settle();
        } else if (strcmp(word, "STROKES") == 0) {
            CHECK(sscanf(line, "STROKES %u %u", &a, &b) == 2);
            settle();
            // Beyond the keys tapped, the computer was sent only the erasing
            // and the typing of the correction.
            CHECK(backspaces_sent == a && characters_sent == taps_sent + b);
        } else if (strcmp(word, "RESULT") == 0 || strcmp(word, "ENDS") == 0) {
            uint8_t bytes[1024];
            CHECK(sscanf(line, "%*s %8199s", text) == 1);
            size_t length = unhex(text, bytes, sizeof(bytes) - 1u);
            bytes[length] = '\0';
            expect_screen((const char *)bytes, word[0] == 'E');
        } else {
            CHECK(false);
        }
    }
    fclose(file);
    return cases;
}

// ── What the fixture cannot say ─────────────────────────────────────────────

static const uint8_t the_list[] = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 't', 'h', 'e', 't', 'e', 'h'};
static const uint8_t no_list[]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static void begin(const char *name, const uint8_t *bytes, size_t length) {
    reset();
    snprintf(case_name, sizeof(case_name), "%s", name);
    use_list(bytes, length);
}

static void type(const char *text, bool fast) {
    for (; *text; text++) {
        uint16_t strokes[2];
        CHECK(host_layout_lookup(host_layout_get(layout_id), (uint8_t)*text, strokes) && !strokes[1]);
        tap(HOST_LAYOUT_STROKE_KEYCODE(strokes[0]), (strokes[0] & HOST_LAYOUT_STROKE_SHIFT) ? MOD_BIT(KC_LEFT_SHIFT) : 0u, fast);
    }
}

// With no list, or switched off, the stage is not in the report path at all.
static void test_the_stage_is_idle_without_a_list(void) {
    begin("idle without a list", no_list, sizeof(no_list));
    CHECK(!noah_typing_stage_active());
    type("teh ", false);
    expect_screen("teh ", false);
    CHECK(!noah_typing_stage_active());

    begin("idle when switched off", the_list, sizeof(the_list));
    keymap_config.autocorrect_enable = false;
    CHECK(!noah_typing_stage_active());
    keymap_config.autocorrect_enable = true;
    CHECK(noah_typing_stage_active());
}

// Switched off in the middle of a word and on again, the keyboard does not
// know where the word began, so it waits for the next one.
static void test_switching_off_forgets_the_word(void) {
    begin("switching off forgets the word", the_list, sizeof(the_list));
    type("te", false);
    keymap_config.autocorrect_enable = false;
    type("h", false);
    keymap_config.autocorrect_enable = true;
    type(" teh ", false);
    expect_screen("teh the ", false);
}

// The engine corrects only while the stored switch and the override both
// allow: switched off by key it stops, and no key turns on what the profile
// has off.
static void test_the_override_and_the_stored_switch_both_allow(void) {
    begin("the override stops corrections", the_list, sizeof(the_list));
    type("teh ", false);
    autocorrect_switch_set_override(false);
    type("teh ", false);
    autocorrect_switch_set_override(true);
    type("teh ", false);
    expect_screen("the teh the ", false);

    // Switched off and on inside one word, that word is left alone.
    begin("switched by key inside a word", the_list, sizeof(the_list));
    type("te", false);
    autocorrect_switch_set_override(false);
    type("h", false);
    autocorrect_switch_set_override(true);
    type(" teh ", false);
    expect_screen("teh the ", false);

    begin("the override cannot turn on what is stored off", the_list, sizeof(the_list));
    keymap_config.autocorrect_enable = false;
    autocorrect_switch_set_override(true);
    type("teh ", false);
    expect_screen("teh ", false);

    // Switched off while a correction is being typed, that correction still
    // finishes and the keys behind it still come out.
    begin("a correction under way finishes", the_list, sizeof(the_list));
    type("teh", false);
    key(KC_SPACE, true);
    scan(4u);
    autocorrect_switch_set_override(false);
    key(KC_SPACE, false);
    type("teh ", true);
    expect_screen("the teh ", false);
}

// A key the correction found held stays up for the computer until it is
// released, and the ending key, still held, goes down after the correction.
static void test_held_keys_around_a_correction(void) {
    begin("held keys around a correction", the_list, sizeof(the_list));
    type("te", false);
    key(KC_H, true);
    scan(10u);
    key(KC_SPACE, true);
    settle();
    // h is still down on the keyboard and was not typed again; the Space
    // that ended the word is down for the computer now.
    char shown[64];
    screen_utf8(shown, sizeof(shown));
    CHECK(strcmp(shown, "the ") == 0);
    CHECK(seen_keys[KC_SPACE / 8u] & (1u << (KC_SPACE % 8u)));
    CHECK(!(seen_keys[KC_H / 8u] & (1u << (KC_H % 8u))));
    key(KC_H, false);
    key(KC_SPACE, false);
    expect_screen("the ", false);
}

// A one-shot modifier read with a held-back key is spent on that key alone.
static void test_a_one_shot_modifier_is_spent_once(void) {
    begin("a one-shot modifier is spent once", the_list, sizeof(the_list));
    type("teh", false);
    key(KC_SPACE, true);
    scan(1u);
    key(KC_SPACE, false);
    oneshot_mods = MOD_BIT(KC_LEFT_SHIFT);
    send_keyboard_report();
    tap(KC_A, 0u, true);
    tap(KC_B, 0u, true);
    expect_screen("the Ab", false);
}

// With more waiting than a correction may start behind, the typo is left.
static void burst_behind_a_correction(unsigned trailing) {
    type("teh", false);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    // No scans from here: every change waits behind the correction.
    for (unsigned i = 0; i < 4u; i++)
        key(KC_X, true), key(KC_X, false), key(KC_Y, true), key(KC_Y, false);
    key(KC_SPACE, true), key(KC_SPACE, false);
    key(KC_T, true), key(KC_T, false);
    key(KC_E, true), key(KC_E, false);
    key(KC_H, true), key(KC_H, false);
    key(KC_SPACE, true), key(KC_SPACE, false);
    for (unsigned i = 0; i < trailing; i++)
        key(KC_Z, true), key(KC_Z, false);
}

static void test_no_correction_starts_behind_a_long_queue(void) {
    // The second teh ends with its Space and six changes waiting.
    begin("a typo behind a short queue", the_list, sizeof(the_list));
    burst_behind_a_correction(3u);
    expect_screen("the xyxyxyxy the zzz", false);
    // Here with its Space and eighteen, over the sixteen a correction may
    // start with: it is left as typed.
    begin("a typo behind a long queue", the_list, sizeof(the_list));
    burst_behind_a_correction(9u);
    expect_screen("the xyxyxyxy teh zzzzzzzzz", false);
}

// The queue full while a correction is typed: the correction stops, and no
// key is lost or reordered. A correction of 35 strokes lasts 350 ms; a key
// every 4 ms fills the queue of 64 changes long before it ends.
static void test_a_full_queue_loses_no_key(void) {
    static const char correction[] = "abcdefghijklmnopqrstuvwxyz012345";
    static uint8_t    long_list[8 + 6 + 6 + 35];
    char              expected[200], shown[400];
    size_t            length;

    memcpy(long_list, (uint8_t[]){1, 0, 1, 0, 0, 0, 35, 0, /* correction */ 0, 0, 32, 0, 0, 0, /* typo */ 32, 0, 3, 0, 0, 0}, 20);
    memcpy(&long_list[20], correction, 32);
    memcpy(&long_list[52], "teh", 3);
    begin("a full queue loses no key", long_list, sizeof(long_list));
    type("teh", false);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    for (unsigned i = 0; i < 78u; i++) {
        uint8_t usage = (uint8_t)(KC_A + i % 26u);
        expected[i]   = (char)('a' + i % 26u);
        scan(1u);
        key(usage, true);
        scan(3u);
        key(usage, false);
    }
    expected[78] = '\0';
    settle();
    length = screen_utf8(shown, sizeof(shown));
    // The correction did not finish, and the Space still came first.
    CHECK(length >= 79u && length < 3u + 32u + 79u && strcmp(shown + length - 78u, expected) == 0 && shown[length - 79u] == ' ');
    expect_screen(expected, true);
}

// Corrections keep their pace: each key held and followed by a gap.
static void test_a_correction_keeps_its_pace(void) {
    begin("a correction keeps its pace", the_list, sizeof(the_list));
    type("teh", false);
    uint32_t started = now;
    unsigned before  = reports_sent;
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    settle();
    // Two erased and two typed: four keys, each held 5 ms and followed by
    // 5 ms. The reports are a press and a release for each, then the ending
    // key's two.
    CHECK(last_report_at - started >= 4u * 2u * NOAH_AUTOCORRECT_PACE_MS);
    CHECK(reports_sent - before == 8u + 2u);
    expect_screen("the ", false);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    unsigned cases = run_fixture(argv[1]);
    test_the_stage_is_idle_without_a_list();
    test_switching_off_forgets_the_word();
    test_the_override_and_the_stored_switch_both_allow();
    test_held_keys_around_a_correction();
    test_a_one_shot_modifier_is_spent_once();
    test_no_correction_starts_behind_a_long_queue();
    test_a_full_queue_loses_no_key();
    test_a_correction_keeps_its_pace();
    printf("autocorrect host tests passed: %u fixture cases\n", cases);
    return 0;
}
