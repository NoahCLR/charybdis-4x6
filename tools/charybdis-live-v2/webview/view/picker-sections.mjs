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
const isSymbol = (entry) => entry.group === "basic" && SYMBOLS.has(valueOf(entry));
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
        {id: "modes", label: "Pointing modes", kind: "catalogue", filter: inGroups("Pointing modes")},
        {id: "macros", label: "Macros", kind: "macros"},
        {id: "mouse", label: "Mouse", kind: "catalogue", filter: inGroups("mouse")},
        {id: "media", label: "Media", kind: "catalogue", filter: inGroups("media", "audio")},
        {id: "lighting", label: "Lighting", kind: "catalogue", filter: inGroups("backlight", "led_matrix", "underglow", "rgb", "rgb_matrix")},
        {id: "magic", label: "Magic", kind: "catalogue", filter: inGroups("magic", "swap_hands")},
        {id: "custom", label: "Custom", kind: "catalogue", filter: inGroups("kb", "user", "macro", "internal")},
        {id: "more", label: "More keys", kind: "catalogue", filter: isMoreKey},
        {id: "other", label: "Other QMK", kind: "catalogue", filter: inGroups(
            "connection", "joystick", "midi", "programmable_button", "quantum", "sequencer", "steno", "system",
        )},
        {id: "all", label: "All keycodes", kind: "catalogue", filter: () => true},
    ];
}

export const entriesForPickerSection = (catalogue, section) =>
    section?.kind === "catalogue" ? catalogue.filter(section.filter) : [];
