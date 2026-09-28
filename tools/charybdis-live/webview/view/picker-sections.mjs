// Human-facing keycode sections. QMK's source groups are useful provenance,
// but they are not a useful first navigation model for somebody configuring a
// keyboard, so the picker presents task-shaped categories instead.

const SYMBOLS = new Set([
    "KC_GRAVE", "KC_MINUS", "KC_EQUAL", "KC_LEFT_BRACKET", "KC_RIGHT_BRACKET",
    "KC_BACKSLASH", "KC_SEMICOLON", "KC_QUOTE", "KC_COMMA", "KC_DOT", "KC_SLASH",
    "KC_NONUS_HASH", "KC_NONUS_BACKSLASH",
]);
const NAVIGATION = new Set([
    "KC_INSERT", "KC_HOME", "KC_PAGE_UP", "KC_DELETE", "KC_END", "KC_PAGE_DOWN",
    "KC_RIGHT", "KC_LEFT", "KC_DOWN", "KC_UP", "KC_PRINT_SCREEN", "KC_SCROLL_LOCK", "KC_PAUSE",
]);

const valueOf = (entry) => entry.value || entry.key || entry.name || "";
const inGroups = (...groups) => (entry) => groups.includes(entry.group);
// Macro slots are offered by their VIA_MACRO_n names in the Macros section.
const NATIVE_MACRO = /^QK_MACRO_\d+$/;
// Charybdis's own drag scroll ignores the pointing-mode settings; drag scroll
// is the factory dragscroll pointing slot. A key already holding one still reads back
// by name, but the picker does not offer it.
const UNSUPPORTED = new Set(["DRGSCRL", "DRG_TOG"]);
// Keycodes for QMK features this firmware does not build, so they do nothing
// on any key: the build enables bootmagic, mouse keys, extra keys, RGB Matrix,
// pointing, combos and VIA, with QMK's default Magic, Grave Escape and Space
// Cadet. A stopgap written against that build until the keyboard reports its
// features; see Open Issues in docs/LIVE_EDIT_APP_DIRECTION.md.
const UNBUILT_GROUPS = new Set([
    "audio", "backlight", "connection", "joystick", "led_matrix", "midi", "programmable_button",
    "rgb", "sequencer", "steno", "swap_hands", "underglow",
]);
const UNBUILT = new RegExp("^(?:" + [
    "QK_AUTO_SHIFT_", "QK_AUTOCORRECT_", "QK_CAPS_WORD_", "QK_DYNAMIC_MACRO_", "QK_DYNAMIC_TAPPING_TERM_",
    "QK_HAPTIC_", "QK_KEY_OVERRIDE_", "QK_SECURE_", "QK_TRI_LAYER_", "QK_UNICODE_MODE_",
    "QK_ALT_REPEAT_KEY$", "QK_LAYER_LOCK$", "QK_LEADER$", "QK_LOCK$", "QK_REPEAT_KEY$", "QK_VELOCIKEY_TOGGLE$",
    "KC_LOCKING_",
    // The QK_KB slots the Charybdis leaves unnamed.
    "QK_KB_\\d+$",
].join("|") + ")");
const unbuilt = (entry) => UNBUILT_GROUPS.has(entry.group) || UNBUILT.test(valueOf(entry));
// Whether the picker offers an entry at all, in any section or search.
export const pickable = (entry) => !NATIVE_MACRO.test(valueOf(entry)) && !UNSUPPORTED.has(valueOf(entry)) && !unbuilt(entry);
// The Charybdis's own keys: DPI and sniping.
const isKeyboardKey = inGroups("kb");
// The keyboard's 64 custom keys, by the names the model gives them, with the
// bare user slots a keyboard of unknown vocabulary reports and KC_NO/KC_TRNS.
const isCustom = inGroups("Custom keys", "user", "internal");
const isSymbol = (entry) => entry.group === "shifted" || (entry.group === "basic" && SYMBOLS.has(valueOf(entry)));
const isNavigation = (entry) => entry.group === "basic" && NAVIGATION.has(valueOf(entry));
const isNumpad = (entry) => entry.group === "basic" && /^KC_(?:KP|NUMPAD)_/.test(valueOf(entry));
const isFunction = (entry) => entry.group === "basic" && /^KC_F(?:[1-9]|1\d|2[0-4])$/.test(valueOf(entry));
const isMoreKey = (entry) => entry.group === "modifiers" || (entry.group === "basic"
    && !isSymbol(entry) && !isNavigation(entry) && !isNumpad(entry)
    && (isFunction(entry) || !/^KC_(?:[A-Z]|[0-9])$/.test(valueOf(entry))));

export function pickerSections() {
    return [
        {id: "board", label: "Keyboard", kind: "board"},
        {id: "symbols", label: "Symbols", kind: "catalogue", filter: isSymbol},
        {id: "navigation", label: "Navigation", kind: "catalogue", filter: isNavigation},
        {id: "numpad", label: "Numpad", kind: "catalogue", filter: isNumpad},
        {id: "layers", label: "Layers", kind: "layers"},
        {id: "modes", label: "Pointing modes", kind: "modes"},
        {id: "macros", label: "Macros", kind: "macros"},
        {id: "mouse", label: "Mouse", kind: "catalogue", filter: (entry) => entry.group === "mouse" || isKeyboardKey(entry)},
        {id: "media", label: "Media", kind: "catalogue", filter: inGroups("media", "audio")},
        {id: "lighting", label: "Lighting", kind: "catalogue", filter: inGroups("backlight", "led_matrix", "underglow", "rgb", "rgb_matrix")},
        {id: "magic", label: "Magic", kind: "catalogue", filter: inGroups("magic", "swap_hands")},
        {id: "custom", label: "Custom keys", kind: "catalogue", filter: isCustom},
        {id: "more", label: "More keys", kind: "catalogue", filter: isMoreKey},
        {id: "other", label: "Other QMK", kind: "catalogue", filter: inGroups(
            "connection", "joystick", "midi", "programmable_button", "quantum", "sequencer", "steno", "system",
        )},
        {id: "all", label: "All keycodes", kind: "catalogue", filter: () => true},
    ];
}

export const entriesForPickerSection = (catalogue, section) =>
    section?.kind === "catalogue" ? catalogue.filter((entry) => pickable(entry) && section.filter(entry)) : [];
