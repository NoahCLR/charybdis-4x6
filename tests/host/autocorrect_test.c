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
#include "users/noah/lib/macro/macro_input_guard.h"
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
static uint32_t                           list_publication;

const noah_profile_autocorrect_v1_list_t *noah_active_profile_autocorrect_list(void) {
    return &list;
}

uint32_t noah_active_profile_autocorrect_publication(void) {
    return list_publication;
}

static macro_payload_ir_t macro_ir;
static char               macro_text[64];
// The macros a case stores, by slot, for corrections that play one. A macro
// key's own text is macro_text.
static char     stored_macros[NOAH_PROFILE_AUTOCORRECT_V1_MACRO_SLOTS][64];
static unsigned macros_started;
// The protection the stored macros carry: 2 is off, 1 on (D-F17).
static uint8_t stored_protection;

// The slot's macro, as the provider would start it.
static bool start_macro(uint16_t action, macro_payload_source_t source) {
    const char *text   = stored_macros[action - QK_MACRO][0] ? stored_macros[action - QK_MACRO] : macro_text;
    size_t      length = strlen(text);

    macros_started++;
    macro_ir          = (macro_payload_ir_t){.protection = stored_protection, .length = (uint16_t)(length + 2u)};
    macro_ir.bytes[0] = MACRO_PAYLOAD_IR_OP_TEXT;
    macro_ir.bytes[1] = (uint8_t)length;
    memcpy(&macro_ir.bytes[2], text, length);
    return macro_payload_start_ir(&macro_ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, 2u, source, 0u, NULL, NULL) == MACRO_PAYLOAD_START_STARTED;
}

bool noah_profile_macro_start(uint16_t action) {
    return start_macro(action, MACRO_PAYLOAD_SOURCE_SLOT);
}

bool noah_profile_macro_start_replacement(uint16_t action) {
    return start_macro(action, MACRO_PAYLOAD_SOURCE_REPLACEMENT);
}

// A correction plays only a macro that is stored.
bool noah_profile_macro_can_play(uint16_t action) {
    return stored_macros[action - QK_MACRO][0] != '\0';
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

// Each usage is its own key of the matrix. A press goes through the press
// guard first, as noah_pre_process_record_user sends it: uninterruptible
// playback that ignores presses drops it, and its release with it.
static void key(uint8_t usage, bool down) {
    keyrecord_t record = {.event = {.type = KEY_EVENT, .pressed = down, .key = {.row = (uint8_t)(usage / MATRIX_COLS % MATRIX_ROWS), .col = (uint8_t)(usage % MATRIX_COLS)}}};
    bool        suppress;

    if (!noah_record_admission_macro_admit(&record, macro_payload_engine_ignores_presses(), false)) return;
    suppress = owned_keycode_should_suppress_default(usage, &record);
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
    memset(stored_macros, 0, sizeof(stored_macros));
    noah_macro_input_guard_reset();
    macros_started    = 0u;
    stored_protection = 2u;
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
    list_publication++;
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
        } else if (strcmp(word, "SLOT") == 0) {
            uint8_t bytes[sizeof(stored_macros[0])];
            CHECK(sscanf(line, "SLOT %u %8199s", &a, text) == 2 && a < NOAH_PROFILE_AUTOCORRECT_V1_MACRO_SLOTS);
            size_t length = unhex(text, bytes, sizeof(bytes) - 1u);
            memcpy(stored_macros[a], bytes, length);
            stored_macros[a][length] = '\0';
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

// A list the runner encoded for these cases, by its name there.
static const char *lists_path;

static void use_named_list(const char *list_name) {
    static char line[9000];
    size_t      named = strlen(list_name);
    FILE       *file  = fopen(lists_path, "r");

    CHECK(file);
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, list_name, named) == 0 && line[named] == ' ') {
            use_list(list_bytes, unhex(&line[named + 1u], list_bytes, sizeof(list_bytes)));
            fclose(file);
            return;
        }
    }
    CHECK(false);
}

static void begin_named(const char *name, const char *list_name) {
    reset();
    snprintf(case_name, sizeof(case_name), "%s", name);
    use_named_list(list_name);
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

// A list published while a word's lookup is spread over scans: the lookup
// starts over on the new list, and nothing chosen from the old one is typed
// from the new one's rows.
static void test_a_list_published_during_a_lookup(void) {
    static uint8_t other[64];
    size_t         other_length;

    use_named_list("other");
    other_length = list.text_bytes + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_ROW_SIZE + NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE + NOAH_PROFILE_AUTOCORRECT_V1_PART_ROW_SIZE;
    CHECK(other_length <= sizeof(other));
    memcpy(other, list_bytes, other_length);

    begin_named("a list published during a lookup", "forty");
    type("xaq01", false);
    key(KC_SPACE, true);
    // The Space is read, and the first 32 of the 40 typos compared: aq01
    // has matched, and the rest wait for the next scan.
    scan(2u);
    use_list(other, other_length);
    key(KC_SPACE, false);
    expect_screen("xaq01 ", false);

    // Unpublished, the same word is corrected over the two scans.
    begin_named("a lookup over two scans", "forty");
    type("xaq01 xaq40 ", false);
    expect_screen("xab01 xab40 ", false);
}

// The stage starting to read reports while a key is held shows the computer
// nothing new: the key stays down for it, and is not pressed a second time.
static void test_the_stage_starts_with_a_key_held(void) {
    begin("the stage starts with a key held", the_list, sizeof(the_list));
    keymap_config.autocorrect_enable = false;
    key(KC_A, true);
    scan(30u);
    keymap_config.autocorrect_enable = true;
    key(KC_B, true);
    scan(30u);
    CHECK(seen_keys[KC_A / 8u] & (1u << (KC_A % 8u)));
    key(KC_A, false);
    key(KC_B, false);
    expect_screen("ab", false);

    // A held Shift and its letter: the letter is not typed again as text.
    begin("the stage starts inside a shortcut", the_list, sizeof(the_list));
    keymap_config.autocorrect_enable = false;
    modifiers(MOD_BIT(KC_LEFT_CTRL), true);
    key(KC_A, true);
    scan(30u);
    keymap_config.autocorrect_enable = true;
    modifiers(MOD_BIT(KC_LEFT_CTRL), false);
    scan(30u);
    key(KC_A, false);
    type("teh teh ", false);
    expect_screen("teh the ", false);
}

// Switched by key while a replacement is typed, the replacement finishes and
// the word is forgotten: Backspace after it is only Backspace.
static void test_a_switch_key_during_a_replacement_ends_the_undo(void) {
    begin_named("a switch key during a replacement ends the undo", "phone");
    type("pnumber", false);
    key(KC_SPACE, true);
    scan(40u);
    CHECK(!macro_payload_engine_idle());
    autocorrect_switch_set_override(false);
    key(KC_SPACE, false);
    settle();
    tap(KC_BACKSPACE, 0u, false);
    expect_screen("+31 6 1234 5678", false);

    // The same with the word's first letter kept by the correction.
    begin("a switch key during a correction that kept a letter", the_list, sizeof(the_list));
    type("teh", false);
    key(KC_SPACE, true);
    scan(20u);
    CHECK(!macro_payload_engine_idle());
    autocorrect_switch_set_override(false);
    key(KC_SPACE, false);
    settle();
    tap(KC_BACKSPACE, 0u, false);
    expect_screen("the", false);
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

static void until_seen(uint8_t usage) {
    for (unsigned guard = 0; !(seen_keys[usage / 8u] & (1u << (usage % 8u))); guard++) {
        CHECK(guard < 5000u);
        scan(1u);
    }
}

static void end_a_typo_and_wait_for(uint8_t usage) {
    type("teh", false);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    until_seen(usage);
}

// A key tapped while a correction holds that same key down changes no
// report: the computer already has the key down. The ledger tells the stage,
// so the tap comes out after the correction like any other key.
static void test_a_tap_of_a_key_the_correction_holds(void) {
    // A behaviour's tap, pressed and released in one step, of the h the
    // correction is typing.
    begin("a behaviour's tap of a key the correction holds", the_list, sizeof(the_list));
    end_a_typo_and_wait_for(KC_H);
    CHECK(owned_keycode_tap(KC_H));
    expect_screen("the h", false);

    // A key as short while the correction erases with it: Backspace, the
    // next key after the correction, so its undo.
    begin("a Backspace tapped while the correction erases", the_list, sizeof(the_list));
    end_a_typo_and_wait_for(KC_BACKSPACE);
    key(KC_BACKSPACE, true);
    key(KC_BACKSPACE, false);
    expect_screen("teh", false);

    // Pressed while the correction holds it and held on, the key is shown
    // once, after the correction, and stays down until it is released.
    begin("a key held on from inside the correction's hold", the_list, sizeof(the_list));
    end_a_typo_and_wait_for(KC_H);
    key(KC_H, true);
    settle();
    CHECK(seen_keys[KC_H / 8u] & (1u << (KC_H % 8u)));
    key(KC_H, false);
    expect_screen("the h", false);

    // A key the person already held when the correction took it is still
    // theirs: a tap of it meanwhile is no second press, and its release is
    // one release.
    begin("a key already held when the correction takes it", the_list, sizeof(the_list));
    type("te", false);
    key(KC_H, true);
    scan(10u);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    until_seen(KC_H);
    CHECK(owned_keycode_tap(KC_H));
    settle();
    key(KC_H, false);
    expect_screen("the ", false);
}

// The key that ended a word, held until it repeats, only ends the chance to
// undo: a keypad character typed meanwhile still leaves the next word
// unknown until it ends.
static void test_a_held_ending_key_keeps_a_suspension(void) {
    begin("a held ending key keeps a suspension", the_list, sizeof(the_list));
    type("xy", false);
    key(KC_SPACE, true);
    scan(20u);
    key(KC_KP_1, true);
    scan(20u);
    key(KC_KP_1, false);
    scan(NOAH_AUTOCORRECT_HOLD_MS + 50u);
    key(KC_SPACE, false);
    type("teh teh ", false);
    expect_screen("xy teh the ", false);
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

// The longest correction: 128 characters after "teh". They are digits, so
// no letter typed behind it is a key the correction is holding.
enum { LONGEST = NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX };
static uint8_t longest_list[8 + 6 + 6 + LONGEST + 3];

static void begin_longest(const char *name) {
    memcpy(longest_list, (uint8_t[]){1, 0, 1, 0, 0, 0, LONGEST + 3, 0, /* correction */ 0, 0, LONGEST, 0, 0, 0, /* typo */ LONGEST, 0, 3, 0, 0, 0}, 20);
    for (unsigned i = 0; i < LONGEST; i++)
        longest_list[20u + i] = (uint8_t)('0' + i % 10u);
    memcpy(&longest_list[20 + LONGEST], "teh", 3);
    begin(name, longest_list, sizeof(longest_list));
    type("teh", false);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
}

// `count` keys, one every `period` ms, while the correction is typed.
static void type_behind(char *expected, unsigned count, unsigned period) {
    for (unsigned i = 0; i < count; i++) {
        uint8_t usage = (uint8_t)(KC_A + i % 26u);
        expected[i]   = (char)('a' + i % 26u);
        scan(1u);
        key(usage, true);
        scan(period - 2u);
        key(usage, false);
        scan(1u);
    }
    expected[count] = '\0';
}

// The queue full while a correction is typed: the correction stops, and no
// key is lost or reordered. The longest correction of 131 strokes lasts
// 1.3 s; a key every 4 ms fills the queue of 240 changes before it ends.
static void test_a_full_queue_loses_no_key(void) {
    static char expected[400], shown[1200];
    size_t      length;

    begin_longest("a full queue loses no key");
    type_behind(expected, 300u, 4u);
    settle();
    length = screen_utf8(shown, sizeof(shown));
    // The correction did not finish, and the Space still came first.
    CHECK(length >= 301u && length < 3u + LONGEST + 301u && strcmp(shown + length - 300u, expected) == 0 && shown[length - 301u] == ' ');
    expect_screen(expected, true);
}

// Fast typing never fills the queue while the longest correction is typed:
// a key every 40 ms, 25 a second and two changes each, through its 1.3 s.
// The correction finishes, and every key comes out after it in order.
static void test_typing_through_the_longest_correction(void) {
    static char expected[LONGEST + 1u + 64u + 1u], behind[64];

    begin_longest("typing through the longest correction");
    type_behind(behind, 40u, 40u);
    memcpy(expected, &longest_list[20], LONGEST);
    expected[LONGEST] = ' ';
    memcpy(&expected[LONGEST + 1u], behind, 41u);
    expect_screen(expected, false);
}

// sig -> macro 5, a whole word.
static const uint8_t macro_list[] = {1, 0, 1, 0, 0, 0, 3, 0, /* correction */ 5, 0, 0, 1, 0, 0, /* typo */ 0, 0, 3, 0, 0, 0, 's', 'i', 'g'};

// A correction's macro stored uninterruptible still loses no key: what is
// typed while it plays is held back and comes out after it, where a macro a
// key started would have its presses ignored (D-F17).
static void test_typing_through_a_protected_macro_correction(void) {
    begin("typing through a protected macro correction", macro_list, sizeof(macro_list));
    snprintf(stored_macros[5], sizeof(stored_macros[5]), "%s", "Kind regards, Sam");
    stored_protection = 1u;
    type("sig and the rest ", true);
    expect_screen("Kind regards, Sam and the rest ", false);
    CHECK(macros_started == 1u);
}

// A macro correction whose erase the queue cut short plays no macro, and one
// whose macro the queue cut short stops; either way no key is lost.
static void test_a_full_queue_stops_a_macro_correction(void) {
    static char          expected[400], shown[1200];
    size_t               length;

    begin("a full queue stops a macro correction", macro_list, sizeof(macro_list));
    snprintf(stored_macros[5], sizeof(stored_macros[5]), "%s", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456");
    type("sig", false);
    key(KC_SPACE, true);
    scan(3u);
    key(KC_SPACE, false);
    type_behind(expected, 300u, 3u);
    settle();
    length = screen_utf8(shown, sizeof(shown));
    CHECK(macros_started == 1u && length >= 301u && strcmp(shown + length - 300u, expected) == 0 && shown[length - 301u] == ' ');
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
    assert(argc == 3);
    lists_path     = argv[2];
    unsigned cases = run_fixture(argv[1]);
    test_the_stage_is_idle_without_a_list();
    test_switching_off_forgets_the_word();
    test_the_stage_starts_with_a_key_held();
    test_a_list_published_during_a_lookup();
    test_a_switch_key_during_a_replacement_ends_the_undo();
    test_the_override_and_the_stored_switch_both_allow();
    test_held_keys_around_a_correction();
    test_a_one_shot_modifier_is_spent_once();
    test_a_held_ending_key_keeps_a_suspension();
    test_a_tap_of_a_key_the_correction_holds();
    test_no_correction_starts_behind_a_long_queue();
    test_a_full_queue_loses_no_key();
    test_typing_through_the_longest_correction();
    test_typing_through_a_protected_macro_correction();
    test_a_full_queue_stops_a_macro_correction();
    test_a_correction_keeps_its_pace();
    printf("autocorrect host tests passed: %u fixture cases\n", cases);
    return 0;
}
