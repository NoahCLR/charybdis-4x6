// Reads Linux (XKB) keyboard layouts with libxkbcommon and the en_US.UTF-8
// Compose table, and prints the raw host-layout source used by
// tools/host_layouts/build.py.
//
//   sh tools/host_layouts/extract_xkb.sh > tools/host_layouts/sources/linux.json
//
// Developer tool. Keys are named by the HID usage QMK sends and mapped to the
// evdev code Linux assigns to it; AltGr is Right Alt as ISO_Level3_Shift.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#define EVDEV_OFFSET 8u
#define KEY_LEFTSHIFT 42u
#define KEY_RIGHTALT 100u
#define KEY_SPACE 57u

static const struct {
    const char *slug, *layout, *variant;
} layouts[] = {
    {"linux-us-international", "us", "intl"},
    {"linux-uk", "gb", ""},
    {"linux-german", "de", ""},
    {"linux-french", "fr", ""},
};

static const struct {
    const char *name;
    unsigned    evdev;
} keys[] = {
    {"KC_A", 30}, {"KC_B", 48}, {"KC_C", 46}, {"KC_D", 32}, {"KC_E", 18}, {"KC_F", 33}, {"KC_G", 34},
    {"KC_H", 35}, {"KC_I", 23}, {"KC_J", 36}, {"KC_K", 37}, {"KC_L", 38}, {"KC_M", 50}, {"KC_N", 49},
    {"KC_O", 24}, {"KC_P", 25}, {"KC_Q", 16}, {"KC_R", 19}, {"KC_S", 31}, {"KC_T", 20}, {"KC_U", 22},
    {"KC_V", 47}, {"KC_W", 17}, {"KC_X", 45}, {"KC_Y", 21}, {"KC_Z", 44},
    {"KC_1", 2}, {"KC_2", 3}, {"KC_3", 4}, {"KC_4", 5}, {"KC_5", 6}, {"KC_6", 7}, {"KC_7", 8},
    {"KC_8", 9}, {"KC_9", 10}, {"KC_0", 11},
    {"KC_SPC", KEY_SPACE}, {"KC_MINS", 12}, {"KC_EQL", 13}, {"KC_LBRC", 26}, {"KC_RBRC", 27},
    {"KC_BSLS", 43}, {"KC_NUHS", 43}, {"KC_SCLN", 39}, {"KC_QUOT", 40}, {"KC_GRV", 41},
    {"KC_COMM", 51}, {"KC_DOT", 52}, {"KC_SLSH", 53}, {"KC_NUBS", 86},
};
#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))

static const char *const layer_format[4] = {"%s", "S(%s)", "ALGR(%s)", "S(ALGR(%s))"};

static void stroke_name(char *out, size_t size, unsigned key, unsigned layer) {
    snprintf(out, size, layer_format[layer], keys[key].name);
}

// The keysym a key produces with Shift and/or AltGr held.
static xkb_keysym_t keysym(struct xkb_keymap *keymap, unsigned key, unsigned layer) {
    struct xkb_state *state = xkb_state_new(keymap);
    if (layer & 1u) xkb_state_update_key(state, KEY_LEFTSHIFT + EVDEV_OFFSET, XKB_KEY_DOWN);
    if (layer & 2u) xkb_state_update_key(state, KEY_RIGHTALT + EVDEV_OFFSET, XKB_KEY_DOWN);
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, keys[key].evdev + EVDEV_OFFSET);
    xkb_state_unref(state);
    return sym;
}

static bool is_dead(xkb_keysym_t sym) {
    char name[64];
    return xkb_keysym_get_name(sym, name, sizeof(name)) > 0 && strncmp(name, "dead_", 5) == 0;
}

static void json_string(const char *utf8) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)utf8; *p; p++) {
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else putchar(*p);
    }
    putchar('"');
}

// A single printable scalar as UTF-8, or false.
static bool printable(uint32_t scalar, char *out, size_t size) {
    if (scalar < 0x20u || (scalar >= 0x7Fu && scalar < 0xA0u)) return false;
    return xkb_keysym_to_utf8(xkb_utf32_to_keysym(scalar), out, size) > 0 && strlen(out) > 0;
}

static bool composed(struct xkb_compose_table *table, xkb_keysym_t dead, xkb_keysym_t base, char *out, size_t size) {
    struct xkb_compose_state *state = xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS);
    xkb_compose_state_feed(state, dead);
    xkb_compose_state_feed(state, base);
    bool ok = xkb_compose_state_get_status(state) == XKB_COMPOSE_COMPOSED && xkb_compose_state_get_utf8(state, out, size) > 0;
    xkb_compose_state_unref(state);
    if (!ok) return false;
    // Keep one printable scalar only.
    uint32_t scalar = 0; const unsigned char *p = (const unsigned char *)out; int width = 0;
    if (*p < 0x80u) {scalar = *p; width = 1;}
    else if ((*p & 0xE0u) == 0xC0u) {scalar = *p & 0x1Fu; width = 2;}
    else if ((*p & 0xF0u) == 0xE0u) {scalar = *p & 0x0Fu; width = 3;}
    else {scalar = *p & 0x07u; width = 4;}
    for (int i = 1; i < width; i++) scalar = (scalar << 6) | (p[i] & 0x3Fu);
    return p[width] == '\0' && !(scalar < 0x20u || (scalar >= 0x7Fu && scalar < 0xA0u));
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s XKB_ROOT XKEYBOARD_CONFIG_VERSION\n", argv[0]);
        return 2;
    }
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES | XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    xkb_context_include_path_append(context, argv[1]);
    struct xkb_compose_table *compose = xkb_compose_table_new_from_locale(context, "en_US.UTF-8", XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!context || !compose) {
        fprintf(stderr, "cannot load XKB data or the en_US.UTF-8 Compose table (set XLOCALEDIR)\n");
        return 1;
    }
    printf("{\n  \"generator\": \"tools/host_layouts/extract_xkb.c\",\n  \"layouts\": {\n");
    for (size_t l = 0; l < sizeof(layouts) / sizeof(layouts[0]); l++) {
        struct xkb_rule_names names = {.rules = "evdev", .model = "pc105", .layout = layouts[l].layout, .variant = layouts[l].variant};
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (!keymap) {
            fprintf(stderr, "cannot compile %s(%s)\n", layouts[l].layout, layouts[l].variant);
            return 1;
        }
        printf("    \"%s\": {\n      \"source\": \"xkb %s%s%s%s, xkeyboard-config %s, en_US.UTF-8 Compose\",\n      \"keys\": {\n",
               layouts[l].slug, layouts[l].layout, *layouts[l].variant ? "(" : "", layouts[l].variant, *layouts[l].variant ? ")" : "", argv[2]);
        for (unsigned k = 0; k < KEY_COUNT; k++) {
            printf("        \"%s\": [", keys[k].name);
            for (unsigned layer = 0; layer < 4u; layer++) {
                xkb_keysym_t sym = keysym(keymap, k, layer);
                char text[16], stroke[48];
                if (layer) printf(", ");
                if (is_dead(sym)) {
                    stroke_name(stroke, sizeof(stroke), k, layer);
                    printf("{\"dead\": \"%s\"}", stroke);
                } else if (printable(xkb_keysym_to_utf32(sym), text, sizeof(text))) {
                    json_string(text);
                } else {
                    printf("null");
                }
            }
            printf("]%s\n", k + 1u < KEY_COUNT ? "," : "");
        }
        printf("      },\n      \"dead\": {");
        bool first_dead = true;
        for (unsigned k = 0; k < KEY_COUNT; k++) {
            for (unsigned layer = 0; layer < 4u; layer++) {
                xkb_keysym_t dead = keysym(keymap, k, layer);
                if (!is_dead(dead)) continue;
                char stroke[48], text[16];
                stroke_name(stroke, sizeof(stroke), k, layer);
                printf("%s\n        \"%s\": {\"space\": ", first_dead ? "" : ",", stroke);
                first_dead = false;
                if (composed(compose, dead, XKB_KEY_space, text, sizeof(text))) json_string(text); else printf("null");
                printf(", \"compose\": {");
                bool first = true;
                for (unsigned b = 0; b < KEY_COUNT; b++) {
                    if (keys[b].evdev == KEY_SPACE) continue;
                    for (unsigned base_layer = 0; base_layer < 2u; base_layer++) {
                        xkb_keysym_t base = keysym(keymap, b, base_layer);
                        char base_text[16], base_stroke[48];
                        if (is_dead(base) || !composed(compose, dead, base, text, sizeof(text))) continue;
                        if (printable(xkb_keysym_to_utf32(base), base_text, sizeof(base_text)) && strcmp(base_text, text) == 0) continue;
                        stroke_name(base_stroke, sizeof(base_stroke), b, base_layer);
                        printf("%s\"%s\": ", first ? "" : ", ", base_stroke);
                        json_string(text);
                        first = false;
                    }
                }
                printf("}}");
            }
        }
        printf("\n      }\n    }%s\n", l + 1u < sizeof(layouts) / sizeof(layouts[0]) ? "," : "");
        xkb_keymap_unref(keymap);
    }
    printf("  }\n}\n");
    xkb_compose_table_unref(compose);
    xkb_context_unref(context);
    return 0;
}
