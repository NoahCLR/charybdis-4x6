"use strict";

const vscode = require("vscode");
const { execFile, spawn } = require("child_process");
const fs = require("fs/promises");
const path = require("path");
const { promisify } = require("util");

const execFileAsync = promisify(execFile);
let profileStudioOutputChannel = undefined;

const PROFILE_KEYBOARD = "bastardkb/charybdis/4x6";
const PROFILE_KEYMAPS_RELATIVE_PATH = path.join(
    "keyboards",
    "bastardkb",
    "charybdis",
    "4x6",
    "keymaps"
);
const DEFAULT_PROFILE_KEYMAP = "noah";
const DEFAULT_PROFILE_TARGET = profileTargetForKeymap(DEFAULT_PROFILE_KEYMAP);
const KEYMAP_RELATIVE_PATH = DEFAULT_PROFILE_TARGET.keymapPath;
const RGB_RELATIVE_PATH = DEFAULT_PROFILE_TARGET.rgbPath;
const KEYMAP_CONFIG_RELATIVE_PATH = DEFAULT_PROFILE_TARGET.configPath;
const PROFILE_TEMPLATE_RELATIVE_PATH = path.join("templates", "charybdis-4x6");
const QMK_KEYCODE_DATA_RELATIVE_PATH = path.join("data", "constants", "keycodes");
const MACRO_PAYLOAD_KEYCODES_RELATIVE_PATH = path.join("users", "noah", "lib", "macro", "macro_payload_keycodes.c");

const LAYOUT_SLOT_COUNT = 56;
const KEY_BEHAVIOR_TIMING_MAX_MS = 65535;
const TAP_COUNT_NAMES = ["Single Tap Branch", "Double Tap Branch", "Triple Tap Branch", "Quadruple Tap Branch", "Quintuple Tap Branch"];
const HOLD_HELPERS = [
    "",
    "PRESS_AND_HOLD_UNTIL_RELEASE",
    "TAP_AT_HOLD_THRESHOLD",
    "TAP_ON_RELEASE_AFTER_HOLD",
    "REPEAT_WHILE_HELD",
];
const TAP_HELPERS = ["", "TAP_SENDS"];
const LAYER_COLOR_MODES = ["ALL_KEYS", "KEYS_MAPPED_ON_THIS_LAYER_ONLY"];
const RGB_LOCALITIES = ["RGB_BOTH_HALVES", "RGB_LEFT_HALF", "RGB_RIGHT_HALF", "RGB_KEY_HALF", "RGB_KEYS_ONLY"];
const AUTOMOUSE_FADE_MODES = ["FOLLOW_REAL_DESTINATION", "END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW", "END_COLOR_ON_ALL_KEYS"];
const KEY_FEEDBACK_TAP_COMMIT_MODES = [
    "KEY_FEEDBACK_TAP_COMMIT_OFF",
    "KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS",
];
const KEY_FEEDBACK_BRANCH_CONFIRM_MODES = [
    "KEY_FEEDBACK_BRANCH_CONFIRM_OFF",
    "KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS",
];
const CONFIG_DEFAULT_SECTIONS = [
    {
        id: "keyTiming",
        label: "Key Timing",
        fields: [
            { macro: "TAPPING_TERM", label: "QMK tapping term", kind: "number", validate: "timing-ms", tooltip: "Tap-vs-hold window for QMK dual-role keys such as LT() and MT(). It does not drive custom key_behaviors[] rows. Higher values make taps easier; lower values make holds start sooner." },
            { macro: "COMBO_TERM", label: "Combo term", kind: "number", validate: "timing-ms", tooltip: "Maximum time between combo member key presses in COMBOS(...). Higher values allow slower chords; lower values reduce accidental combo triggers." },
            { macro: "CUSTOM_TAP_HOLD_TERM", label: "Tap-hold term", kind: "number", validate: "timing-ms", tooltip: "Default tap-vs-hold boundary for key_behaviors[] rows that leave tap_hold_term empty. Row-level values override this. Lower values make holds activate sooner." },
            { macro: "CUSTOM_LONGER_HOLD_TERM", label: "Long hold term", kind: "number", validate: "timing-ms", tooltip: "Default hold-vs-long-hold boundary for key_behaviors[] rows that leave longer_hold_term empty. Row-level values override this. Higher values require a longer press for third-tier actions." },
            { macro: "CUSTOM_MULTI_TAP_TERM", label: "Multi-tap term", kind: "number", validate: "timing-ms", tooltip: "Default maximum gap between repeated taps in key_behaviors[] tap-count branches. Row-level values override this. Higher values allow slower double/triple taps." },
        ],
    },
    {
        id: "normalPointerSpeed",
        label: "Normal Pointer Speed",
        fields: [
            { macro: "CHARYBDIS_MINIMUM_DEFAULT_DPI", label: "Default DPI minimum", kind: "number", validate: "positive-int", tooltip: "Base DPI/CPI for the normal pointer ladder used by DPI controls and by pointing modes whose override is 0. Actual normal DPI/CPI is minimum + selected index * step." },
            { macro: "CHARYBDIS_DEFAULT_DPI_CONFIG_STEP", label: "Default DPI step", kind: "number", validate: "positive-int", tooltip: "DPI/CPI distance between normal pointer ladder entries. Higher values make each DPI up/down adjustment jump farther." },
        ],
    },
    {
        id: "pointingModeSpeeds",
        label: "Pointing Mode Speeds",
        fields: [
            { macro: "CHARYBDIS_DRAGSCROLL_DPI", label: "Drag-scroll DPI", kind: "number", validate: "positive-int", tooltip: "Explicit DPI/CPI while drag-scroll mode is active. Higher values make ball movement produce faster scrolling. This field does not use the 0 fallback convention." },
            { macro: "PD_MODE_VOLUME_DPI", label: "Volume mode DPI override", kind: "number", validate: "nonnegative-int", hint: "0 uses normal pointer DPI", tooltip: "Temporary DPI/CPI while volume mode maps ball movement to volume changes. 0 keeps the current normal pointer DPI instead of meaning no movement." },
            { macro: "PD_MODE_BRIGHTNESS_DPI", label: "Brightness mode DPI override", kind: "number", validate: "nonnegative-int", hint: "0 uses normal pointer DPI", tooltip: "Temporary DPI/CPI while brightness mode maps ball movement to brightness changes. 0 keeps the current normal pointer DPI instead of meaning no movement." },
            { macro: "PD_MODE_ZOOM_DPI", label: "Zoom mode DPI override", kind: "number", validate: "nonnegative-int", hint: "0 uses normal pointer DPI", tooltip: "Temporary DPI/CPI while zoom mode is active. 0 keeps the current normal pointer DPI; higher values make zoom movement more sensitive." },
            { macro: "PD_MODE_ARROW_DPI", label: "Arrow mode DPI override", kind: "number", validate: "nonnegative-int", hint: "0 uses normal pointer DPI", tooltip: "Temporary DPI/CPI while arrow mode turns ball movement into directional output. 0 keeps the current normal pointer DPI; higher values increase sensitivity." },
        ],
    },
    {
        id: "sniping",
        label: "Sniping",
        fields: [
            { macro: "CHARYBDIS_MINIMUM_SNIPING_DPI", label: "Sniping DPI minimum", kind: "number", validate: "positive-int", tooltip: "Base DPI/CPI for the sniping ladder. While sniping is active it uses this lower-sensitivity ladder and takes priority over pointing-mode DPI overrides." },
            { macro: "CHARYBDIS_SNIPING_DPI_CONFIG_STEP", label: "Sniping DPI step", kind: "number", validate: "positive-int", tooltip: "DPI/CPI distance between sniping ladder entries. Higher values make each sniping DPI adjustment jump farther." },
            { macro: "CHARYBDIS_AUTO_SNIPING_ENABLE", label: "Auto-sniping", kind: "toggle", tooltip: "Automatically enters sniping while the configured auto-sniping layer is active. Disable this if sniping should only be entered by manual controls." },
            { macro: "CHARYBDIS_AUTO_SNIPING_LAYER", label: "Auto-sniping layer", kind: "layer", validate: "layer", tooltip: "Layer that triggers automatic sniping while active. Changing this moves the sniping trigger; it does not change the layer's normal keymap contents." },
        ],
    },
    {
        id: "autoMouse",
        label: "Auto-mouse",
        fields: [
            { macro: "POINTING_DEVICE_AUTO_MOUSE_ENABLE", label: "Auto-mouse", kind: "toggle", tooltip: "Enables automatic layer activation from trackball movement. Disable this if pointing movement should never switch to the auto-mouse layer." },
            { macro: "AUTO_MOUSE_DEFAULT_LAYER", label: "Auto-mouse layer", kind: "layer", validate: "layer", tooltip: "Layer activated by auto-mouse movement. Changing this chooses which layer appears while the trackball is in use; it does not change the layer's key contents." },
            { macro: "AUTO_MOUSE_TIME", label: "Auto-mouse timeout", kind: "number", validate: "positive-int", tooltip: "Milliseconds auto-mouse remains active after the last pointing movement. Higher values keep the layer active longer and also lengthen the auto-mouse RGB fade window." },
        ],
    },
    {
        id: "rgbAppearance",
        label: "Base Lighting",
        fields: [
            { macro: "RGB_MATRIX_DEFAULT_MODE", label: "Default RGB mode", kind: "expression", validate: "identifier", tooltip: "Base QMK RGB Matrix effect used when no layer color or feedback overlay paints an LED. Profile RGB rows can still override individual LEDs." },
            { macro: "RGB_MATRIX_DEFAULT_HUE", label: "Default hue", kind: "number", validate: "uint8", tooltip: "Hue channel for the base RGB Matrix color. It is visible where layer colors pass through to the default effect." },
            { macro: "RGB_MATRIX_DEFAULT_SAT", label: "Default saturation", kind: "number", validate: "uint8", tooltip: "Saturation channel for the base RGB Matrix color. 0 is white/gray; 255 is fully saturated." },
            { macro: "RGB_MATRIX_MAXIMUM_BRIGHTNESS", label: "Maximum brightness", kind: "number", validate: "uint8", tooltip: "Global brightness cap for RGB Matrix output. Lower values reduce LED brightness and current draw; values are 0-255." },
            { macro: "RGB_MATRIX_DEFAULT_VAL", label: "Default value", kind: "expression", validate: "safe-expression", tooltip: "Brightness channel for the base RGB Matrix color. This sets the default effect brightness, while maximum brightness caps the final output." },
            { macro: "RGB_MATRIX_TIMEOUT", label: "RGB timeout", kind: "number", validate: "nonnegative-int", tooltip: "Milliseconds of keyboard inactivity before RGB Matrix turns off. 0 disables the timeout; higher values keep lighting on longer." },
        ],
    },
    {
        id: "lightingFeedback",
        label: "Lighting Feedback",
        fields: [
            { macro: "RGB_PD_MODE_FEEDBACK_ENABLE", label: "Pointing-mode feedback", kind: "toggle", tooltip: "Enables the RGB overlay for active pointing modes such as volume, brightness, zoom, and arrows. Disable this to remove pointing-mode color feedback entirely." },
            { macro: "RGB_COMBO_FEEDBACK_ENABLE", label: "Combo feedback", kind: "toggle", tooltip: "Enables the RGB overlay shown while combo member keys are active. Disable this to remove combo footprint feedback without changing combo behavior." },
            { macro: "RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE", label: "Key-behavior feedback", kind: "toggle", tooltip: "Enables RGB feedback for custom key behavior taps, holds, long holds, and tap-count branches. Disable this to remove the visual stage without changing key output." },
            { macro: "CUSTOM_RGB_BRANCH_CONFIRM_TERM", label: "RGB branch-confirm term", kind: "number", validate: "timing-ms", tooltip: "Default visible confirmation window for committed non-base tap-count branches. Row-level branch_confirm_term values override this; rows can also skip the confirm window." },
            { macro: "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS", label: "Key feedback flash half-period", kind: "number", validate: "positive-int", tooltip: "Blink half-period for held or repeating key-behavior feedback. One full blink cycle is twice this value; lower values flash faster." },
            { macro: "RGB_AUTOMOUSE_GRADIENT_ENABLE", label: "Auto-mouse gradient", kind: "toggle", tooltip: "Enables the RGB fade that shows auto-mouse approaching its timeout. Disable this to remove the timeout fade while leaving auto-mouse layer activation unchanged." },
            { macro: "AUTOMOUSE_RGB_DEAD_TIME", label: "Auto-mouse RGB dead time", kind: "expression", validate: "safe-expression", tooltip: "Initial part of AUTO_MOUSE_TIME before the auto-mouse RGB fade starts. Must stay below AUTO_MOUSE_TIME; higher values delay the visible timeout warning." },
            { macro: "RGB_MATRIX_LED_FLUSH_LIMIT", label: "LED flush limit", kind: "number", validate: "positive-int", tooltip: "Global minimum milliseconds between RGB Matrix LED updates. Higher values reduce CPU/LED update load but make animation and feedback changes less smooth." },
        ],
    },
];
const CONFIG_DEFAULT_FIELD_BY_MACRO = new Map(
    CONFIG_DEFAULT_SECTIONS.flatMap((section) => section.fields.map((field) => [field.macro, field]))
);
const RGB_LAYER_GROUP_ALL = "RGB_LAYER_GROUP_ALL";
const RGB_PD_MODE_GROUP_ALL = "RGB_PD_MODE_GROUP_ALL";
const KEY_FEEDBACK_GROUP_ALL = "KEY_FEEDBACK_GROUP_ALL";
const QMK_KEYCODE_SECTION_GROUPS = [
    { id: "qmk-media", label: "Media", groups: ["media", "system"] },
    { id: "qmk-lighting", label: "Lighting", groups: ["rgb", "rgb_matrix", "led_matrix", "backlight", "underglow"] },
    { id: "qmk-magic", label: "Magic", groups: ["magic"] },
    { id: "qmk-other", label: "Other QMK", groups: ["quantum", "sequencer", "swap_hands", "audio", "midi", "steno", "joystick", "programmable_button"] },
];
const KEY_PICKER_KEYBOARD_SVG_LAYOUT = {
    width: 1563.3125,
    height: 466.2,
    keys: [
        [24.658, 24.929, 61.203, 61.203, "KC_ESC", "Esc"],
        [159.658, 24.929, 61.203, 61.203, "KC_F1", "F1"],
        [227.161, 24.929, 61.203, 61.203, "KC_F2", "F2"],
        [294.664, 24.929, 61.203, 61.203, "KC_F3", "F3"],
        [362.167, 24.929, 61.203, 61.203, "KC_F4", "F4"],
        [463.408, 24.929, 61.203, 61.203, "KC_F5", "F5"],
        [530.911, 24.929, 61.203, 61.203, "KC_F6", "F6"],
        [598.414, 24.929, 61.203, 61.203, "KC_F7", "F7"],
        [665.917, 24.929, 61.203, 61.203, "KC_F8", "F8"],
        [767.158, 24.929, 61.203, 61.203, "KC_F9", "F9"],
        [834.661, 24.929, 61.203, 61.203, "KC_F10", "F10"],
        [902.164, 24.929, 61.203, 61.203, "KC_F11", "F11"],
        [969.667, 24.929, 61.203, 61.203, "KC_F12", "F12"],
        [24.658, 110.069, 61.203, 61.203, "KC_GRV", ["`", "~"]],
        [92.161, 110.069, 61.203, 61.203, "KC_1", ["1", "!"]],
        [159.664, 110.069, 61.203, 61.203, "KC_2", ["2", "@"]],
        [227.167, 110.069, 61.203, 61.203, "KC_3", ["3", "#"]],
        [294.67, 110.069, 61.203, 61.203, "KC_4", ["4", "$"]],
        [362.173, 110.069, 61.203, 61.203, "KC_5", ["5", "%"]],
        [429.676, 110.069, 61.203, 61.203, "KC_6", ["6", "^"]],
        [497.179, 110.069, 61.203, 61.203, "KC_7", ["7", "&"]],
        [564.682, 110.069, 61.203, 61.203, "KC_8", ["8", "*"]],
        [632.185, 110.069, 61.203, 61.203, "KC_9", ["9", "("]],
        [699.688, 110.069, 61.203, 61.203, "KC_0", ["0", ")"]],
        [767.191, 110.069, 61.203, 61.203, "KC_MINS", ["-", "_"]],
        [834.694, 110.069, 61.203, 61.203, "KC_EQL", ["=", "+"]],
        [902.197, 110.069, 128.703, 61.203, "KC_BSPC", ["Backspace", "⇦"]],
        [24.658, 177.569, 95.943, 61.203, "KC_TAB", ["Tab", "↹"]],
        [126.898, 177.569, 61.203, 61.203, "KC_Q", "Q"],
        [194.401, 177.569, 61.203, 61.203, "KC_W", "W"],
        [261.904, 177.569, 61.203, 61.203, "KC_E", "E"],
        [329.407, 177.569, 61.203, 61.203, "KC_R", "R"],
        [396.91, 177.569, 61.203, 61.203, "KC_T", "T"],
        [464.413, 177.569, 61.203, 61.203, "KC_Y", "Y"],
        [531.916, 177.569, 61.203, 61.203, "KC_U", "U"],
        [599.419, 177.569, 61.203, 61.203, "KC_I", "I"],
        [666.922, 177.569, 61.203, 61.203, "KC_O", "O"],
        [734.425, 177.569, 61.203, 61.203, "KC_P", "P"],
        [801.928, 177.569, 61.203, 61.203, "KC_LBRC", ["[", "{"]],
        [869.431, 177.569, 61.203, 61.203, "KC_RBRC", ["]", "}"]],
        [936.934, 177.569, 93.963, 61.203, "KC_BSLS", ["\\", "|"]],
        [24.658, 245.069, 112.593, 61.203, "KC_CAPS", "Caps"],
        [143.548, 245.069, 61.203, 61.203, "KC_A", "A"],
        [211.051, 245.069, 61.203, 61.203, "KC_S", "S"],
        [278.554, 245.069, 61.203, 61.203, "KC_D", "D"],
        [346.057, 245.069, 61.203, 61.203, "KC_F", "F"],
        [413.56, 245.069, 61.203, 61.203, "KC_G", "G"],
        [481.063, 245.069, 61.203, 61.203, "KC_H", "H"],
        [548.566, 245.069, 61.203, 61.203, "KC_J", "J"],
        [616.069, 245.069, 61.203, 61.203, "KC_K", "K"],
        [683.572, 245.069, 61.203, 61.203, "KC_L", "L"],
        [751.075, 245.069, 61.203, 61.203, "KC_SCLN", [";", ":"]],
        [818.578, 245.069, 61.203, 61.203, "KC_QUOT", ["'", "\""]],
        [886.081, 245.069, 144.813, 61.203, "KC_ENT", ["Enter", "↵"]],
        [24.658, 312.569, 146.343, 61.203, "KC_LSFT", ["Shift", "⇧"]],
        [177.298, 312.569, 61.203, 61.203, "KC_Z", "Z"],
        [244.801, 312.569, 61.203, 61.203, "KC_X", "X"],
        [312.304, 312.569, 61.203, 61.203, "KC_C", "C"],
        [379.807, 312.569, 61.203, 61.203, "KC_V", "V"],
        [447.31, 312.569, 61.203, 61.203, "KC_B", "B"],
        [514.813, 312.569, 61.203, 61.203, "KC_N", "N"],
        [582.316, 312.569, 61.203, 61.203, "KC_M", "M"],
        [649.819, 312.569, 61.203, 61.203, "KC_COMM", [",", "<"]],
        [717.322, 312.569, 61.203, 61.203, "KC_DOT", [".", ">"]],
        [784.825, 312.569, 61.203, 61.203, "KC_SLSH", ["/", "?"]],
        [852.328, 312.569, 178.563, 61.203, "KC_RSFT", ["Shift", "⇧"]],
        [24.658, 380.069, 74.703, 61.203, "KC_LCTL", "Ctrl"],
        [105.661, 380.069, 74.703, 61.203, "KC_LGUI", "Cmd"],
        [186.664, 380.069, 74.703, 61.203, "KC_LALT", "Alt"],
        [267.667, 380.069, 439.203, 61.203, "KC_SPC", "Space"],
        [713.158, 380.069, 74.703, 61.203, "KC_RALT", "AltGr"],
        [794.161, 380.069, 74.703, 61.203, "KC_APP", "Menu"],
        [875.164, 380.069, 74.703, 61.203, "KC_RCTL", "Ctrl"],
        [956.164, 380.069, 74.703, 61.203, "KC_RGUI", "Cmd"],
        [1054.64, 24.939, 61.452, 61.359, "KC_PSCR", "PrtSc"],
        [1122.14, 24.896, 61.452, 61.402, "KC_SCRL", "ScrLk"],
        [1189.64, 24.896, 61.452, 61.402, "KC_PAUS", "Pause"],
        [1054.64, 110.015, 61.452, 61.402, "KC_INS", "Insert"],
        [1122.14, 110.015, 61.452, 61.402, "KC_HOME", "Home"],
        [1189.64, 110.015, 61.452, 61.402, "KC_PGUP", "PgUp"],
        [1054.64, 177.515, 61.452, 61.402, "KC_DEL", "Delete"],
        [1122.14, 177.515, 61.452, 61.402, "KC_END", "End"],
        [1189.64, 177.515, 61.452, 61.402, "KC_PGDN", "PgDn"],
        [1122.14, 312.515, 61.452, 61.402, "KC_UP", "↑"],
        [1054.64, 380.015, 61.452, 61.402, "KC_LEFT", "←"],
        [1122.14, 380.015, 61.452, 61.402, "KC_DOWN", "↓"],
        [1189.64, 380.015, 61.452, 61.402, "KC_RGHT", "→"],
        [1274.78, 110.015, 61.452, 61.402, "KC_NUM", "NumL"],
        [1342.28, 110.015, 61.452, 61.402, "KC_PSLS", "/"],
        [1409.78, 110.015, 61.452, 61.402, "KC_PAST", "*"],
        [1477.28, 110.015, 61.452, 61.402, "KC_PMNS", "-"],
        [1274.78, 177.515, 61.452, 61.402, "KC_P7", ["7", "Hme"]],
        [1342.28, 177.515, 61.452, 61.402, "KC_P8", ["8", "↑"]],
        [1409.78, 177.515, 61.452, 61.402, "KC_P9", ["9", "PgU"]],
        [1477.28, 177.481, 61.452, 129.032, "KC_PPLS", "+"],
        [1274.78, 245.015, 61.452, 61.359, "KC_P4", ["4", "←"]],
        [1342.28, 245.015, 61.452, 61.359, "KC_P5", "5"],
        [1409.78, 245.015, 61.452, 61.359, "KC_P6", ["6", "→"]],
        [1274.78, 312.537, 61.452, 61.359, "KC_P1", ["1", "End"]],
        [1342.28, 312.537, 61.452, 61.359, "KC_P2", ["2", "↓"]],
        [1409.78, 312.537, 61.452, 61.359, "KC_P3", ["3", "PgD"]],
        [1477.28, 312.524, 61.452, 129.032, "KC_PENT", "Enter"],
        [1274.643, 380.058, 129.227, 61.359, "KC_P0", ["0", "Ins"]],
        [1409.78, 380.058, 61.452, 61.359, "KC_PDOT", [".", "Del"]],
    ].map(([x, y, w, h, value, label]) => ({ x, y, w, h, value, label })),
};
const MOD_WRAPPER_LABELS = {
    C: ["Ctrl"],
    S: ["Shift"],
    A: ["Alt"],
    G: ["Cmd"],
    LCTL: ["Ctrl"],
    LSFT: ["Shift"],
    LALT: ["Alt"],
    LGUI: ["Cmd"],
    RCTL: ["Right Ctrl"],
    RSFT: ["Right Shift"],
    RALT: ["Right Alt"],
    RGUI: ["Right Cmd"],
    LAG: ["Alt", "Cmd"],
    LSG: ["Shift", "Cmd"],
    LCAG: ["Ctrl", "Alt", "Cmd"],
    MEH: ["Ctrl", "Shift", "Alt"],
    HYPR: ["Ctrl", "Shift", "Alt", "Cmd"],
};
const LAYOUT_KEY_CALL_FUNCTIONS = ["LT", "MO", "TG", "TO", "TT", "DF", "OSL", "LM", "OSM", "MT", "CUSTOM"];
const RGB_LED_GROUP_TARGETS = {
    layer: {
        tableName: "layer_led_groups_data",
        ownerField: ".layer",
        ownerLabel: "layer",
    },
    pdMode: {
        tableName: "pd_mode_led_groups_data",
        ownerField: ".pointing_mode",
        ownerLabel: "pointing mode",
    },
    combo: {
        tableName: "combo_feedback_led_groups_data",
        ownerField: "",
        ownerLabel: "",
    },
    keyBehavior: {
        tableName: "key_behavior_feedback_led_groups_data",
        ownerField: ".semantic",
        ownerLabel: "semantic",
    },
};
const USER_KEY_ALIASES = {
    transparent: "_______",
    trans: "_______",
    kc_transparent: "_______",
    kc_trns: "_______",
    disabled: "XXXXXXX",
    none: "XXXXXXX",
    no: "XXXXXXX",
    kc_no: "XXXXXXX",
    esc: "KC_ESC",
    escape: "KC_ESC",
    tab: "KC_TAB",
    enter: "KC_ENT",
    return: "KC_ENT",
    space: "KC_SPC",
    backspace: "KC_BSPC",
    delete: "KC_DEL",
    del: "KC_DEL",
    caps: "KC_CAPS",
    capslock: "KC_CAPS",
    "caps lock": "KC_CAPS",
    left: "KC_LEFT",
    right: "KC_RIGHT",
    up: "KC_UP",
    down: "KC_DOWN",
    home: "KC_HOME",
    end: "KC_END",
    pageup: "KC_PGUP",
    "page up": "KC_PGUP",
    pgup: "KC_PGUP",
    pagedown: "KC_PGDN",
    "page down": "KC_PGDN",
    pgdn: "KC_PGDN",
    "left shift": "KC_LEFT_SHIFT",
    lshift: "KC_LEFT_SHIFT",
    shift: "KC_LEFT_SHIFT",
    "right shift": "KC_RIGHT_SHIFT",
    rshift: "KC_RIGHT_SHIFT",
    "left ctrl": "KC_LEFT_CTRL",
    lctrl: "KC_LEFT_CTRL",
    ctrl: "KC_LEFT_CTRL",
    "right ctrl": "KC_RIGHT_CTRL",
    rctrl: "KC_RIGHT_CTRL",
    "left alt": "KC_LEFT_ALT",
    lalt: "KC_LEFT_ALT",
    alt: "KC_LEFT_ALT",
    "right alt": "KC_RIGHT_ALT",
    ralt: "KC_RIGHT_ALT",
    "left gui": "KC_LEFT_GUI",
    lgui: "KC_LEFT_GUI",
    gui: "KC_LEFT_GUI",
    cmd: "KC_LEFT_GUI",
    command: "KC_LEFT_GUI",
    "right gui": "KC_RIGHT_GUI",
    rgui: "KC_RIGHT_GUI",
    play: "KC_MPLY",
    next: "KC_MNXT",
    previous: "KC_MPRV",
    prev: "KC_MPRV",
    mute: "KC_MUTE",
    "mouse 1": "MS_BTN1",
    "mouse 2": "MS_BTN2",
    "mouse 3": "MS_BTN3",
    btn1: "MS_BTN1",
    btn2: "MS_BTN2",
    btn3: "MS_BTN3",
    "-": "KC_MINS",
    "_": "KC_UNDS",
    "=": "KC_EQL",
    "+": "KC_PLUS",
    "[": "KC_LBRC",
    "]": "KC_RBRC",
    "{": "KC_LCBR",
    "}": "KC_RCBR",
    "\\": "KC_BSLS",
    "|": "KC_PIPE",
    ";": "KC_SCLN",
    ":": "KC_COLN",
    "'": "KC_QUOT",
    "\"": "KC_DQUO",
    ",": "KC_COMM",
    ".": "KC_DOT",
    "/": "KC_SLSH",
    "?": "KC_QUES",
    "<": "KC_LABK",
    ">": "KC_RABK",
    "`": "KC_GRV",
    grave: "KC_GRV",
    backtick: "KC_GRV",
    "~": "KC_TILD",
    "!": "KC_EXLM",
    "@": "KC_AT",
    "#": "KC_HASH",
    "$": "KC_DLR",
    "%": "KC_PERC",
    "^": "KC_CIRC",
    "&": "KC_AMPR",
    "*": "KC_ASTR",
    "(": "KC_LPRN",
    ")": "KC_RPRN",
};

const QMK_KEY_LABELS = {
    _______: "_______",
    XXXXXXX: "Disabled",
    KC_ESC: "Esc",
    KC_TAB: "Tab",
    KC_ENT: "Enter",
    KC_ENTER: "Enter",
    KC_SPC: "Space",
    KC_SPACE: "Space",
    KC_BSPC: "Backspace",
    KC_BACKSPACE: "Backspace",
    KC_DEL: "Delete",
    KC_DELETE: "Delete",
    KC_CAPS: "Caps Lock",
    KC_LEFT: "Left",
    KC_RGHT: "Right",
    KC_RIGHT: "Right",
    KC_UP: "Up",
    KC_DOWN: "Down",
    KC_HOME: "Home",
    KC_END: "End",
    KC_PGUP: "Page Up",
    KC_PGDN: "Page Down",
    KC_LEFT_SHIFT: "Left Shift",
    KC_RIGHT_SHIFT: "Right Shift",
    KC_LEFT_CTRL: "Left Ctrl",
    KC_RIGHT_CTRL: "Right Ctrl",
    KC_LEFT_ALT: "Left Alt",
    KC_RIGHT_ALT: "Right Alt",
    KC_LEFT_GUI: "Left Cmd",
    KC_RIGHT_GUI: "Right Cmd",
    KC_MPLY: "Play",
    KC_MNXT: "Next",
    KC_MPRV: "Previous",
    KC_MUTE: "Mute",
    MS_BTN1: "Mouse 1",
    MS_BTN2: "Mouse 2",
    MS_BTN3: "Mouse 3",
    KC_MINS: "-",
    KC_UNDS: "_",
    KC_EQL: "=",
    KC_PLUS: "+",
    KC_LBRC: "[",
    KC_RBRC: "]",
    KC_LCBR: "{",
    KC_RCBR: "}",
    KC_BSLS: "\\",
    KC_PIPE: "|",
    KC_SCLN: ";",
    KC_COLN: ":",
    KC_QUOT: "'",
    KC_DQUO: "\"",
    KC_COMM: ",",
    KC_DOT: ".",
    KC_SLSH: "/",
    KC_QUES: "?",
    KC_LABK: "<",
    KC_RABK: ">",
    KC_GRV: "`",
    KC_TILD: "~",
    KC_EXLM: "!",
    KC_AT: "@",
    KC_HASH: "#",
    KC_DLR: "$",
    KC_PERC: "%",
    KC_CIRC: "^",
    KC_AMPR: "&",
    KC_ASTR: "*",
    KC_LPRN: "(",
    KC_RPRN: ")",
    DRAGSCROLL: "Dragscroll",
    DRAGSCROLL_LOCK: "Dragscroll Lock",
};
const SHIFTED_KEY_OUTPUT_LABELS = {
    KC_GRV: "~",
    KC_1: "!",
    KC_2: "@",
    KC_3: "#",
    KC_4: "$",
    KC_5: "%",
    KC_6: "^",
    KC_7: "&",
    KC_8: "*",
    KC_9: "(",
    KC_0: ")",
    KC_MINS: "_",
    KC_EQL: "+",
    KC_LBRC: "{",
    KC_RBRC: "}",
    KC_BSLS: "|",
    KC_SCLN: ":",
    KC_QUOT: "\"",
    KC_COMM: "<",
    KC_DOT: ">",
    KC_SLSH: "?",
};

for (let index = 0; index <= 9; index += 1) {
    QMK_KEY_LABELS[`KC_${index}`] = String(index);
    USER_KEY_ALIASES[String(index)] = `KC_${index}`;
}
for (let code = 65; code <= 90; code += 1) {
    const letter = String.fromCharCode(code);
    QMK_KEY_LABELS[`KC_${letter}`] = letter;
    USER_KEY_ALIASES[letter.toLowerCase()] = `KC_${letter}`;
    USER_KEY_ALIASES[letter] = `KC_${letter}`;
}
const SHIFTED_KEY_BASE_LABELS = Object.fromEntries(
    Object.entries(SHIFTED_KEY_OUTPUT_LABELS)
        .map(([baseKeycode, shiftedLabel]) => [USER_KEY_ALIASES[shiftedLabel], QMK_KEY_LABELS[baseKeycode] || baseKeycode])
        .filter(([shiftedKeycode]) => Boolean(shiftedKeycode))
);

function profileTargetForKeymap(keymap, source = {}) {
    const keymapDir = path.join(PROFILE_KEYMAPS_RELATIVE_PATH, keymap);
    return {
        id: `${PROFILE_KEYBOARD}:${keymap}`,
        keyboard: PROFILE_KEYBOARD,
        keymap,
        keymapDir,
        keymapPath: path.join(keymapDir, "keymap.c"),
        configPath: path.join(keymapDir, "config.h"),
        rgbPath: path.join(keymapDir, "rgb_config.c"),
        rulesPath: path.join(keymapDir, "rules.mk"),
        userspaceName: "noah",
        userspaceDir: path.join("users", "noah"),
        registered: Boolean(source.registered),
        discovered: Boolean(source.discovered),
        complete: Boolean(source.complete),
        editable: Boolean(source.editable),
        buildable: Boolean(source.buildable),
    };
}

function profileTargetPaths(root, target) {
    return {
        keymap: path.join(root, target.keymapPath),
        config: path.join(root, target.configPath),
        rgb: path.join(root, target.rgbPath),
        rules: path.join(root, target.rulesPath),
    };
}

function normalizeProfileKeymapName(value) {
    return String(value || "").trim();
}

function assertValidProfileKeymapName(keymap) {
    if (!keymap) {
        throw new Error("Profile name is required.");
    }
    if (!/^[a-z0-9_-]+$/.test(keymap)) {
        throw new Error("Profile name must use only lowercase letters, numbers, hyphens, and underscores.");
    }
    if (keymap === "." || keymap === ".." || keymap.includes("/") || keymap.includes("\\")) {
        throw new Error("Profile name must not contain path separators.");
    }
}

async function findRepoRoot() {
    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        const root = folder.uri.fsPath;
        if (await isProfileRepoRoot(root)) {
            return root;
        }
    }

    const activeFile = vscode.window.activeTextEditor?.document?.uri.fsPath;
    if (activeFile) {
        let cursor = path.dirname(activeFile);
        while (cursor !== path.dirname(cursor)) {
            if (await isProfileRepoRoot(cursor)) {
                return cursor;
            }
            cursor = path.dirname(cursor);
        }
    }

    return undefined;
}

async function isProfileRepoRoot(root) {
    return await fileExists(path.join(root, "qmk.json")) ||
        await fileExists(path.join(root, DEFAULT_PROFILE_TARGET.keymapPath)) ||
        await fileExists(path.join(root, PROFILE_KEYMAPS_RELATIVE_PATH));
}

async function discoverProfileTargets(root) {
    const byKeymap = new Map();
    for (const keymap of await keymapsFromQmkJson(root)) {
        byKeymap.set(keymap, { registered: true });
    }

    const keymapsRoot = path.join(root, PROFILE_KEYMAPS_RELATIVE_PATH);
    try {
        for (const entry of await fs.readdir(keymapsRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) continue;
            const keymap = entry.name;
            const state = byKeymap.get(keymap) || {};
            state.discovered = true;
            byKeymap.set(keymap, state);
        }
    } catch {
        // A repo with no keymaps yet can still create one from Studio.
    }

    const targets = [];
    for (const [keymap, state] of byKeymap.entries()) {
        const target = profileTargetForKeymap(keymap, state);
        const paths = profileTargetPaths(root, target);
        const hasKeymap = await fileExists(paths.keymap);
        const hasConfig = await fileExists(paths.config);
        const hasRgb = await fileExists(paths.rgb);
        const hasRules = await fileExists(paths.rules);
        target.editable = hasKeymap && hasConfig && hasRgb;
        target.complete = target.editable && (hasRules || keymap === DEFAULT_PROFILE_KEYMAP);
        target.buildable = target.complete;
        targets.push(target);
    }

    return targets.sort(compareProfileTargets);
}

function compareProfileTargets(left, right) {
    if (left.keymap === DEFAULT_PROFILE_KEYMAP && right.keymap !== DEFAULT_PROFILE_KEYMAP) return -1;
    if (right.keymap === DEFAULT_PROFILE_KEYMAP && left.keymap !== DEFAULT_PROFILE_KEYMAP) return 1;
    if (left.registered !== right.registered) return left.registered ? -1 : 1;
    return left.keymap.localeCompare(right.keymap, undefined, { numeric: true });
}

async function keymapsFromQmkJson(root) {
    const qmkJsonPath = path.join(root, "qmk.json");
    try {
        const parsed = JSON.parse(await fs.readFile(qmkJsonPath, "utf8"));
        return (Array.isArray(parsed.build_targets) ? parsed.build_targets : [])
            .filter((target) => Array.isArray(target) && target[0] === PROFILE_KEYBOARD && typeof target[1] === "string")
            .map((target) => target[1]);
    } catch {
        return [];
    }
}

async function ensureQmkBuildTarget(root, keymap) {
    const qmkJsonPath = path.join(root, "qmk.json");
    let parsed = {};
    try {
        parsed = JSON.parse(await fs.readFile(qmkJsonPath, "utf8"));
    } catch {
        parsed = { userspace_version: "1.0", build_targets: [] };
    }

    const buildTargets = Array.isArray(parsed.build_targets) ? parsed.build_targets : [];
    if (!buildTargets.some((target) => Array.isArray(target) && target[0] === PROFILE_KEYBOARD && target[1] === keymap)) {
        buildTargets.push([PROFILE_KEYBOARD, keymap]);
    }
    parsed.build_targets = buildTargets;
    await writeText(qmkJsonPath, JSON.stringify(parsed, null, 4) + "\n");
}

async function removeQmkBuildTarget(root, keymap) {
    const qmkJsonPath = path.join(root, "qmk.json");
    let parsed;
    try {
        parsed = JSON.parse(await fs.readFile(qmkJsonPath, "utf8"));
    } catch {
        return;
    }

    const buildTargets = Array.isArray(parsed.build_targets) ? parsed.build_targets : [];
    parsed.build_targets = buildTargets.filter((target) => !(Array.isArray(target) && target[0] === PROFILE_KEYBOARD && target[1] === keymap));
    await writeText(qmkJsonPath, JSON.stringify(parsed, null, 4) + "\n");
}

async function renameQmkBuildTarget(root, oldKeymap, newKeymap) {
    const qmkJsonPath = path.join(root, "qmk.json");
    let parsed = {};
    try {
        parsed = JSON.parse(await fs.readFile(qmkJsonPath, "utf8"));
    } catch {
        parsed = { userspace_version: "1.0", build_targets: [] };
    }

    const buildTargets = Array.isArray(parsed.build_targets) ? parsed.build_targets : [];
    let renamed = false;
    const nextTargets = [];
    for (const target of buildTargets) {
        if (!Array.isArray(target) || target[0] !== PROFILE_KEYBOARD || target[1] !== oldKeymap) {
            nextTargets.push(target);
            continue;
        }
        if (!nextTargets.some((existing) => Array.isArray(existing) && existing[0] === PROFILE_KEYBOARD && existing[1] === newKeymap)) {
            nextTargets.push([PROFILE_KEYBOARD, newKeymap]);
        }
        renamed = true;
    }
    if (!renamed && !nextTargets.some((target) => Array.isArray(target) && target[0] === PROFILE_KEYBOARD && target[1] === newKeymap)) {
        nextTargets.push([PROFILE_KEYBOARD, newKeymap]);
    }
    parsed.build_targets = nextTargets;
    await writeText(qmkJsonPath, JSON.stringify(parsed, null, 4) + "\n");
}

async function pruneMissingProfileBuildTargets(root) {
    const qmkJsonPath = path.join(root, "qmk.json");
    let parsed;
    try {
        parsed = JSON.parse(await fs.readFile(qmkJsonPath, "utf8"));
    } catch {
        return [];
    }

    if (!Array.isArray(parsed.build_targets)) {
        return [];
    }

    const removed = [];
    const retained = [];
    for (const target of parsed.build_targets) {
        if (!Array.isArray(target) || target[0] !== PROFILE_KEYBOARD || typeof target[1] !== "string") {
            retained.push(target);
            continue;
        }

        const keymapDir = path.join(root, PROFILE_KEYMAPS_RELATIVE_PATH, target[1]);
        if (await directoryExists(keymapDir)) {
            retained.push(target);
        } else {
            removed.push(target[1]);
        }
    }

    if (removed.length) {
        parsed.build_targets = retained;
        await writeText(qmkJsonPath, JSON.stringify(parsed, null, 4) + "\n");
    }
    return removed;
}

async function activeProfileTarget(root, state) {
    const profiles = await discoverProfileTargets(root);
    let target = profiles.find((profile) => profile.id === state.activeProfileId && profile.editable);
    if (!target) {
        target = profiles.find((profile) => profile.keymap === DEFAULT_PROFILE_KEYMAP && profile.editable) ||
            profiles.find((profile) => profile.editable) ||
            profiles[0];
    }
    state.activeProfileId = target?.id || "";
    return { profiles, target };
}

function requireActiveProfile(target) {
    if (!target || !target.editable) {
        throw new Error("No editable profile is selected.");
    }
    return target;
}

async function createProfile(root, keymapName) {
    const keymap = normalizeProfileKeymapName(keymapName);
    assertValidProfileKeymapName(keymap);

    const target = profileTargetForKeymap(keymap);
    const targetDir = path.join(root, target.keymapDir);
    if (await fileExists(targetDir)) {
        throw new Error(`Profile already exists: ${keymap}`);
    }

    await fs.mkdir(targetDir, { recursive: true });
    const rendered = await renderProfileTemplates(keymap);
    const paths = profileTargetPaths(root, target);
    await Promise.all([
        writeText(paths.rules, rendered.rules),
        writeText(paths.config, rendered.config),
        writeText(paths.keymap, rendered.keymap),
        writeText(paths.rgb, rendered.rgb),
    ]);
    await ensureQmkBuildTarget(root, keymap);
    return profileTargetForKeymap(keymap, {
        registered: true,
        discovered: true,
        complete: true,
        editable: true,
        buildable: true,
    });
}

function assertMutableProfile(target, action) {
    requireActiveProfile(target);
    if (target.keymap === DEFAULT_PROFILE_KEYMAP) {
        throw new Error(`The default ${DEFAULT_PROFILE_KEYMAP} profile cannot be ${action}.`);
    }
}

async function cloneProfile(root, sourceTarget, keymapName) {
    requireActiveProfile(sourceTarget);
    const keymap = normalizeProfileKeymapName(keymapName);
    assertValidProfileKeymapName(keymap);

    const target = profileTargetForKeymap(keymap);
    const sourceDir = path.join(root, sourceTarget.keymapDir);
    const targetDir = path.join(root, target.keymapDir);
    if (!(await directoryExists(sourceDir))) {
        throw new Error(`Profile folder not found: ${sourceTarget.keymap}`);
    }
    if (await fileExists(targetDir)) {
        throw new Error(`Profile already exists: ${keymap}`);
    }

    await fs.cp(sourceDir, targetDir, { recursive: true });
    const targetPaths = profileTargetPaths(root, target);
    if (!(await fileExists(targetPaths.rules))) {
        const rendered = await renderProfileTemplates(keymap);
        await writeText(targetPaths.rules, rendered.rules);
    }
    await ensureQmkBuildTarget(root, keymap);
    return profileTargetForKeymap(keymap, {
        registered: true,
        discovered: true,
        complete: true,
        editable: true,
        buildable: true,
    });
}

async function renameProfile(root, sourceTarget, keymapName) {
    assertMutableProfile(sourceTarget, "renamed");
    const keymap = normalizeProfileKeymapName(keymapName);
    assertValidProfileKeymapName(keymap);
    if (keymap === sourceTarget.keymap) {
        return sourceTarget;
    }

    const target = profileTargetForKeymap(keymap);
    const sourceDir = path.join(root, sourceTarget.keymapDir);
    const targetDir = path.join(root, target.keymapDir);
    if (!(await directoryExists(sourceDir))) {
        throw new Error(`Profile folder not found: ${sourceTarget.keymap}`);
    }
    if (await fileExists(targetDir)) {
        throw new Error(`Profile already exists: ${keymap}`);
    }

    await fs.rename(sourceDir, targetDir);
    await renameQmkBuildTarget(root, sourceTarget.keymap, keymap);
    return profileTargetForKeymap(keymap, {
        registered: true,
        discovered: true,
        complete: true,
        editable: true,
        buildable: true,
    });
}

async function deleteProfile(root, target) {
    assertMutableProfile(target, "deleted");
    const targetDir = path.join(root, target.keymapDir);
    if (!(await directoryExists(targetDir))) {
        throw new Error(`Profile folder not found: ${target.keymap}`);
    }

    await fs.rm(targetDir, { recursive: true, force: false });
    await removeQmkBuildTarget(root, target.keymap);
}

async function runProfileIntrospection(root, target, mode) {
    requireActiveProfile(target);
    const python = process.env.PYTHON || "python3";
    const args = [
        path.join(root, "tools", "profile_introspect.py"),
        "--keymap",
        target.keymap,
        mode === "check" ? "--check" : "--write",
    ];
    await execFileAsync(python, args, { cwd: root, maxBuffer: 10 * 1024 * 1024 });
}

function studioOutputChannel() {
    if (!profileStudioOutputChannel) {
        profileStudioOutputChannel = vscode.window.createOutputChannel("Charybdis Profile Studio");
    }
    return profileStudioOutputChannel;
}

function qmkKeyboardFilesafe(keyboard) {
    return String(keyboard || PROFILE_KEYBOARD).replace(/[^A-Za-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
}

function firmwareTargetName(target, side) {
    return `${qmkKeyboardFilesafe(target.keyboard || PROFILE_KEYBOARD)}_${target.keymap}_${side}`;
}

function shellDisplayArg(value) {
    const text = String(value);
    return /^[A-Za-z0-9_./:=+-]+$/.test(text) ? text : JSON.stringify(text);
}

function lastUsefulLogLine(text) {
    const lines = String(text || "")
        .replace(/\x1b\[[0-9;]*m/g, "")
        .split(/\r?\n/)
        .map((line) => line.trim())
        .filter(Boolean);
    return lines.slice(-1)[0] || "";
}

async function compileProfileFirmware(root, target) {
    requireActiveProfile(target);
    if (!target.buildable) {
        throw new Error(`Profile ${target.keymap} is incomplete and cannot be compiled.`);
    }

    const channel = studioOutputChannel();
    channel.show(true);
    channel.appendLine("");
    channel.appendLine(`Starting ${target.keymap} firmware compile`);
    const title = `Compiling ${target.keymap} left and right firmware`;
    const task = async (progress = { report() {} }) => {
        const builds = [];
        progress.report({ message: "left firmware" });
        builds.push(await runQmkCompile(root, target, {
            side: "left",
            label: "left",
            env: "FORCE_MASTER",
        }));
        progress.report({ message: "right firmware" });
        builds.push(await runQmkCompile(root, target, {
            side: "right",
            label: "right",
            env: "FORCE_SLAVE",
        }));
        return builds;
    };

    if (typeof vscode.window.withProgress === "function" && vscode.ProgressLocation) {
        return vscode.window.withProgress(
            { location: vscode.ProgressLocation.Notification, title, cancellable: false },
            task
        );
    }
    return task();
}

function runQmkCompile(root, target, build) {
    const firmwareTarget = firmwareTargetName(target, build.side);
    const args = [
        "compile",
        "-kb",
        target.keyboard || PROFILE_KEYBOARD,
        "-km",
        target.keymap,
        "-e",
        `${build.env}=yes`,
        "-e",
        `TARGET=${firmwareTarget}`,
    ];
    const channel = studioOutputChannel();
    channel.show(true);
    channel.appendLine("");
    channel.appendLine(`$ qmk ${args.map(shellDisplayArg).join(" ")}`);

    return new Promise((resolve, reject) => {
        let stdout = "";
        let stderr = "";
        let settled = false;
        const child = spawn("qmk", args, { cwd: root });
        const finish = (callback) => {
            if (settled) return;
            settled = true;
            callback();
        };
        child.stdout?.on("data", (chunk) => {
            const text = String(chunk);
            stdout += text;
            channel.append(text);
        });
        child.stderr?.on("data", (chunk) => {
            const text = String(chunk);
            stderr += text;
            channel.append(text);
        });
        child.on("error", (error) => {
            finish(() => {
                channel.show(true);
                reject(new Error(`Failed to start QMK ${build.label} firmware compile: ${error.message}`));
            });
        });
        child.on("close", (code, signal) => {
            finish(() => {
                if (code === 0) {
                    resolve({
                        side: build.side,
                        label: build.label,
                        role: build.env,
                        firmware: `${firmwareTarget}.uf2`,
                        path: path.join(root, `${firmwareTarget}.uf2`),
                    });
                    return;
                }
                channel.show(true);
                const exitDetail = signal ? `terminated by signal ${signal}` : `exited with code ${code}`;
                const detail = lastUsefulLogLine(stderr) || lastUsefulLogLine(stdout) || exitDetail;
                reject(new Error(`QMK ${build.label} firmware compile failed: ${detail}. See the Charybdis Profile Studio output for the full log.`));
            });
        });
    });
}

function compiledFirmwareNotice(target, builds) {
    const firmware = builds.map((build) => build.firmware).join(", ");
    return `Compiled ${target.keymap} firmware for left and right halves: ${firmware}.`;
}

async function renderProfileTemplates(keymap) {
    const templateRoot = path.join(__dirname, PROFILE_TEMPLATE_RELATIVE_PATH);
    const replacements = {
        "{{KEYMAP_NAME}}": keymap,
        "{{PROFILE_TITLE}}": keymap.replace(/[-_]+/g, " ").replace(/\b[a-z]/g, (char) => char.toUpperCase()),
    };
    const render = async (fileName) => {
        let text = await fs.readFile(path.join(templateRoot, fileName), "utf8");
        for (const [from, to] of Object.entries(replacements)) {
            text = text.split(from).join(to);
        }
        return text;
    };
    return {
        rules: await render("rules.mk"),
        config: await render("config.h"),
        keymap: await render("keymap.c"),
        rgb: await render("rgb_config.c"),
    };
}

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand("charybdisProfileStudio.open", () => openStudio(context))
    );
    setupNativeEntryPoints(context);
}

function deactivate() { }

async function setupNativeEntryPoints(context) {
    const root = await findRepoRoot();
    if (!root) {
        return;
    }

    const status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 2);
    status.text = "$(keyboard) Profile Studio";
    status.tooltip = "Open Charybdis Profile Studio";
    status.command = "charybdisProfileStudio.open";
    status.show();
    context.subscriptions.push(status);
}

async function openStudio(context) {
    const root = await findRepoRoot();
    if (!root) {
        vscode.window.showErrorMessage(
            "Could not find a Charybdis userspace repo in the open workspace."
        );
        return;
    }
    const state = { activeProfileId: "" };

    const panel = vscode.window.createWebviewPanel(
        "charybdisProfileStudio",
        "Charybdis Profile Studio",
        vscode.ViewColumn.One,
        {
            enableScripts: true,
            retainContextWhenHidden: true,
        }
    );

    panel.webview.html = getStudioHtml(panel.webview);

    panel.webview.onDidReceiveMessage(
        async (message) => {
            try {
                await handleWebviewMessage(panel, root, state, message);
            } catch (error) {
                const text = error instanceof Error ? error.message : String(error);
                panel.webview.postMessage({ type: "error", message: text });
                vscode.window.showErrorMessage(`Charybdis Profile Studio: ${text}`);
            }
        },
        undefined,
        context.subscriptions
    );

    await postModel(panel, root, state);
}

async function fileExists(filePath) {
    try {
        await fs.access(filePath);
        return true;
    } catch {
        return false;
    }
}

async function directoryExists(filePath) {
    try {
        return (await fs.stat(filePath)).isDirectory();
    } catch {
        return false;
    }
}

async function handleWebviewMessage(panel, root, state, message) {
    switch (message?.type) {
        case "ready":
            await postModel(panel, root, state);
            return;
        case "refresh": {
            const removedProfiles = await pruneMissingProfileBuildTargets(root);
            const notice = removedProfiles.length
                ? `Reloaded files, removed stale qmk.json target${removedProfiles.length === 1 ? "" : "s"}: ${removedProfiles.join(", ")}.`
                : "Reloaded files and discarded uncommitted Studio edits.";
            await postModel(panel, root, state, notice);
            return;
        }
        case "selectProfile":
            state.activeProfileId = String(message.profileId || "");
            await postModel(panel, root, state, "Switched Profile Studio target.");
            return;
        case "requestCreateProfile": {
            const keymap = await promptForProfileName({ title: "New Charybdis profile", placeHolder: "fresh_profile" });
            if (!keymap) {
                await postModel(panel, root, state, "Profile creation cancelled.");
                return;
            }
            const target = await createProfile(root, keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, `Created ${target.keymap}.`);
            return;
        }
        case "requestCloneProfile": {
            const source = requireActiveProfile((await activeProfileTarget(root, state)).target);
            const keymap = await promptForProfileName({
                title: `Clone ${source.keymap}`,
                prompt: "Enter the new cloned keymap folder name.",
                placeHolder: `${source.keymap}_copy`,
            });
            if (!keymap) {
                await postModel(panel, root, state, "Profile clone cancelled.");
                return;
            }
            const target = await cloneProfile(root, source, keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, `Cloned ${source.keymap} to ${target.keymap}.`);
            return;
        }
        case "requestRenameProfile": {
            const source = requireActiveProfile((await activeProfileTarget(root, state)).target);
            const keymap = await promptForProfileName({
                title: `Rename ${source.keymap}`,
                prompt: "Enter the new keymap folder name.",
                value: source.keymap,
            });
            if (!keymap) {
                await postModel(panel, root, state, "Profile rename cancelled.");
                return;
            }
            const target = await renameProfile(root, source, keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, target.keymap === source.keymap ? `Profile remains ${target.keymap}.` : `Renamed ${source.keymap} to ${target.keymap}.`);
            return;
        }
        case "requestDeleteProfile": {
            const target = requireActiveProfile((await activeProfileTarget(root, state)).target);
            assertMutableProfile(target, "deleted");
            const confirmed = await confirmDeleteProfile(target);
            if (!confirmed) {
                await postModel(panel, root, state, "Profile deletion cancelled.");
                return;
            }
            await deleteProfile(root, target);
            state.activeProfileId = "";
            await postModel(panel, root, state, `Deleted ${target.keymap}.`);
            return;
        }
        case "createProfile": {
            const target = await createProfile(root, message.keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, `Created ${target.keymap}.`);
            return;
        }
        case "cloneProfile": {
            const source = await checkedMessageProfile(root, state, message);
            const target = await cloneProfile(root, source, message.keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, `Cloned ${source.keymap} to ${target.keymap}.`);
            return;
        }
        case "renameProfile": {
            const source = await checkedMessageProfile(root, state, message);
            const target = await renameProfile(root, source, message.keymap);
            state.activeProfileId = target.id;
            await postModel(panel, root, state, target.keymap === source.keymap ? `Profile remains ${target.keymap}.` : `Renamed ${source.keymap} to ${target.keymap}.`);
            return;
        }
        case "deleteProfile": {
            const target = await checkedMessageProfile(root, state, message);
            await deleteProfile(root, target);
            state.activeProfileId = "";
            await postModel(panel, root, state, `Deleted ${target.keymap}.`);
            return;
        }
        case "generateProfileDocs": {
            const target = await checkedMessageProfile(root, state, message);
            try {
                await runProfileIntrospection(root, target, "write");
                const notice = `Generated ${target.keymap} profile overview docs.`;
                panel.webview.postMessage({ type: "docsResult", notice });
                vscode.window.showInformationMessage(notice);
            } catch (error) {
                const text = error instanceof Error ? error.message : String(error);
                panel.webview.postMessage({ type: "docsResult", error: text });
                vscode.window.showErrorMessage(`Charybdis Profile Studio: ${text}`);
            }
            return;
        }
        case "compileFirmware": {
            const target = await checkedMessageProfile(root, state, message);
            try {
                const builds = await compileProfileFirmware(root, target);
                const notice = compiledFirmwareNotice(target, builds);
                panel.webview.postMessage({ type: "compileResult", notice });
                vscode.window.showInformationMessage(notice);
            } catch (error) {
                const text = error instanceof Error ? error.message : String(error);
                panel.webview.postMessage({ type: "compileResult", error: text });
                vscode.window.showErrorMessage(`Charybdis Profile Studio: ${text}`);
            }
            return;
        }
        case "openSource":
            await openSource(root, requireActiveProfile((await activeProfileTarget(root, state)).target), message.file);
            return;
        case "applyAllChanges": {
            const target = await checkedMessageProfile(root, state, message);
            await applyAllChanges(root, target, message);
            await postModel(panel, root, state, "Applied all staged Studio changes.", { activeLayer: message.activeLayer, appliedLayerChanges: Boolean(message.adds?.length || message.deletes?.length) });
            return;
        }
        case "applyAllChangesAndCompile": {
            const target = await checkedMessageProfile(root, state, message);
            await applyAllChanges(root, target, message);
            const builds = await compileProfileFirmware(root, target);
            await postModel(panel, root, state, `Applied staged changes and ${compiledFirmwareNotice(target, builds)}`, { activeLayer: message.activeLayer, appliedLayerChanges: Boolean(message.adds?.length || message.deletes?.length) });
            return;
        }
        case "applyAllChangesAndGenerateProfileDocs": {
            const target = await checkedMessageProfile(root, state, message);
            await applyAllChanges(root, target, message);
            await runProfileIntrospection(root, target, "write");
            await postModel(panel, root, state, `Applied staged changes and generated ${target.keymap} profile overview docs.`, { activeLayer: message.activeLayer, appliedLayerChanges: Boolean(message.adds?.length || message.deletes?.length) });
            return;
        }
        case "applyLayerChanges": {
            const target = await checkedMessageProfile(root, state, message);
            await applyLayerChanges(root, target, message.adds, message.deletes);
            await postModel(panel, root, state, "Applied staged layer changes.", { activeLayer: message.activeLayer, appliedLayerChanges: true });
            return;
        }
        case "updateLayoutKeys": {
            const target = await checkedMessageProfile(root, state, message);
            if (Array.isArray(message.layers)) {
                await patchLayoutKeyGroups(root, target, message.layers);
            } else {
                await patchLayoutKeys(root, target, message.layer, message.changes);
            }
            await postModel(panel, root, state, "Updated keymap.c layout keys.");
            return;
        }
        case "updateLayerColor":
            await patchLayerColor(root, await checkedMessageProfile(root, state, message), message.layer, message.hue, message.sat, message.val, message.mode);
            await postModel(panel, root, state, "Updated rgb_config.c layer color.");
            return;
        case "updatePdModeColor":
            await patchPdModeColor(root, await checkedMessageProfile(root, state, message), message.pointingMode, message.hue, message.sat, message.val, message.locality);
            await postModel(panel, root, state, "Updated rgb_config.c pointing-mode color.");
            return;
        case "updateAutomouseFade":
            await patchAutomouseFade(root, await checkedMessageProfile(root, state, message), message.mode, message.hue, message.sat, message.val);
            await postModel(panel, root, state, "Updated rgb_config.c auto-mouse fade.");
            return;
        case "updateComboFeedback":
            await patchComboFeedback(root, await checkedMessageProfile(root, state, message), message.hue, message.sat, message.val, message.locality);
            await postModel(panel, root, state, "Updated rgb_config.c combo feedback.");
            return;
        case "updateKeyBehaviorFeedback":
            await patchKeyBehaviorFeedback(root, await checkedMessageProfile(root, state, message), message.config);
            await postModel(panel, root, state, "Updated rgb_config.c key behavior feedback.");
            return;
        case "updateConfigDefaults":
            await patchConfigDefaults(root, await checkedMessageProfile(root, state, message), message.fields);
            await postModel(panel, root, state, "Updated config.h defaults.");
            return;
        case "saveRgbReusableLedGroup":
            await saveRgbReusableLedGroup(root, await checkedMessageProfile(root, state, message), message.group);
            await postModel(panel, root, state, "Saved rgb_config.c reusable LED group.", { clearedRgbReusableGroupDraft: true });
            return;
        case "deleteRgbReusableLedGroup":
            await deleteRgbReusableLedGroup(root, await checkedMessageProfile(root, state, message), message.name);
            await postModel(panel, root, state, "Deleted rgb_config.c reusable LED group.", { clearedRgbReusableGroupDraft: true });
            return;
        case "addRgbLedGroup":
            await appendRgbLedGroup(root, await checkedMessageProfile(root, state, message), message.group);
            await postModel(panel, root, state, "Added rgb_config.c LED group row.");
            return;
        case "updateViaMacro":
            await patchViaMacro(root, await checkedMessageProfile(root, state, message), message.keycode, message.payload);
            await postModel(panel, root, state, "Updated keymap.c VIA macro payload.");
            return;
        case "addCombo":
            await appendCombo(root, await checkedMessageProfile(root, state, message), message.output, message.inputs);
            await postModel(panel, root, state, "Added keymap.c combo row.");
            return;
        case "saveCombo":
            await saveCombo(root, await checkedMessageProfile(root, state, message), message.originalOutput, message.originalInputs, message.output, message.inputs);
            await postModel(panel, root, state, "Saved keymap.c combo row.");
            return;
        case "addBehavior":
            await appendKeyBehavior(root, await checkedMessageProfile(root, state, message), message.behavior);
            await postModel(panel, root, state, "Added keymap.c key behavior row.");
            return;
        case "saveBehavior":
            await saveKeyBehavior(root, await checkedMessageProfile(root, state, message), message.behavior);
            await postModel(panel, root, state, "Saved keymap.c key behavior row.");
            return;
        default:
            throw new Error(`Unknown studio message: ${message?.type}`);
    }
}

async function promptForProfileName(options = {}) {
    const value = await vscode.window.showInputBox({
        title: options.title || "Charybdis profile",
        prompt: options.prompt || "Enter the keymap folder name.",
        placeHolder: options.placeHolder || "fresh_profile",
        value: options.value,
        validateInput(input) {
            try {
                assertValidProfileKeymapName(normalizeProfileKeymapName(input));
                return undefined;
            } catch (error) {
                return error instanceof Error ? error.message : String(error);
            }
        },
    });
    return value === undefined ? undefined : normalizeProfileKeymapName(value);
}

async function confirmDeleteProfile(target) {
    const choice = await vscode.window.showWarningMessage(
        `Delete Charybdis profile ${target.keymap}? This removes ${target.keymapDir} and its qmk.json build target.`,
        { modal: true },
        "Delete Profile"
    );
    return choice === "Delete Profile";
}

async function checkedMessageProfile(root, state, message) {
    const { target } = await activeProfileTarget(root, state);
    requireActiveProfile(target);
    if (message?.profileId && message.profileId !== target.id) {
        throw new Error("Profile changed before this write was applied. Reload and apply the change again.");
    }
    return target;
}

async function openSource(root, target, file) {
    const relative = file === "rgb" ? target.rgbPath : file === "config" ? target.configPath : target.keymapPath;
    const document = await vscode.workspace.openTextDocument(path.join(root, relative));
    await vscode.window.showTextDocument(document, vscode.ViewColumn.Beside);
}

async function postModel(panel, root, state, notice, options = {}) {
    const { profiles, target } = await activeProfileTarget(root, state);
    const model = await buildModel(root, target, profiles);
    panel.webview.postMessage({ type: "model", model, notice, ...options });
}

async function buildModel(root, target = DEFAULT_PROFILE_TARGET, profiles) {
    const allProfiles = profiles || await discoverProfileTargets(root);
    const requestedTarget = target || DEFAULT_PROFILE_TARGET;
    const profileTarget = allProfiles.find((profile) => profile.id === requestedTarget.id) || requestedTarget;
    const paths = profileTargetPaths(root, profileTarget);
    const keymapPath = paths.keymap;
    const rgbPath = paths.rgb;
    const configPath = paths.config;
    const macroPayloadKeycodesPath = path.join(root, MACRO_PAYLOAD_KEYCODES_RELATIVE_PATH);
    if (!profileTarget.editable && !(await fileExists(keymapPath) && await fileExists(configPath) && await fileExists(rgbPath))) {
        const qmkKeycodeCatalog = await loadQmkKeycodeCatalog(root).catch(() => fallbackQmkKeycodeCatalog());
        return emptyModel(root, allProfiles, profileTarget, qmkKeycodeCatalog, ["Create a profile to start editing keymap.c."]);
    }
    const [keymapText, rgbText, configText, macroPayloadKeycodesText] = await Promise.all([
        fs.readFile(keymapPath, "utf8"),
        fs.readFile(rgbPath, "utf8"),
        fs.readFile(configPath, "utf8").catch(() => ""),
        fs.readFile(macroPayloadKeycodesPath, "utf8").catch(() => ""),
    ]);

    const diagnostics = [];
    const safe = (label, fallback, callback) => {
        try {
            return callback();
        } catch (error) {
            diagnostics.push(`${label}: ${error instanceof Error ? error.message : String(error)}`);
            return fallback;
        }
    };
    const configMacros = safe("configMacros", {}, () => parseConfigMacros(configText));
    const configDefines = safe("configDefines", {}, () => parseConfigDefineStates(configText));
    const qmkKeycodeCatalog = await loadQmkKeycodeCatalog(root).catch((error) => {
        diagnostics.push(`qmkKeycodes: ${error instanceof Error ? error.message : String(error)}`);
        return fallbackQmkKeycodeCatalog();
    });

    return {
        root,
        profiles: allProfiles,
        activeProfile: profileTarget,
        files: {
            keymap: profileTarget.keymapPath,
            config: profileTarget.configPath,
            rgb: profileTarget.rgbPath,
        },
        layers: safe("layers", [], () => parseLayers(keymapText)),
        customKeycodes: safe("customKeycodes", [], () => parseKeymapCustomKeycodes(keymapText)),
        keyBehaviors: safe("keyBehaviors", [], () => parseKeyBehaviors(keymapText)),
        combos: safe("combos", [], () => parseMacroTable(keymapText, "COMBOS", "COMBO").map(parseComboRow)),
        viaMacros: safe("viaMacros", [], () => parseMacroTable(keymapText, "VIA_MACROS", "MACRO").map((row) => parseMacroSlot(row, "via"))),
        hardcodedMacros: safe("hardcodedMacros", [], () =>
            parseMacroTable(keymapText, "HARDCODED_MACROS", "MACRO").map((row) => parseMacroSlot(row, "hardcoded"))
        ),
        behaviorTimingDefaults: safe("behaviorTimingDefaults", {}, () => resolveBehaviorTimingDefaults(configMacros)),
        configDefaults: safe("configDefaults", [], () => resolveConfigDefaultSections(configDefines)),
        rgb: safe("rgb", {}, () => parseRgbConfig(rgbText, configMacros)),
        qmkKeycodes: qmkKeycodeCatalog.entries,
        qmkKeyLabels: qmkKeycodeCatalog.labels,
        qmkKeycodeAliases: qmkKeycodeCatalog.aliases,
        qmkKeycodeSource: qmkKeycodeCatalog.source,
        macroPayloadKeycodes: safe("macroPayloadKeycodes", [], () => parseMacroPayloadKeycodes(macroPayloadKeycodesText)),
        diagnostics,
    };
}

function emptyModel(root, profiles, target, qmkKeycodeCatalog, diagnostics = []) {
    return {
        root,
        profiles,
        activeProfile: target || null,
        files: target ? {
            keymap: target.keymapPath,
            config: target.configPath,
            rgb: target.rgbPath,
        } : {},
        layers: [],
        customKeycodes: [],
        keyBehaviors: [],
        combos: [],
        viaMacros: [],
        hardcodedMacros: [],
        behaviorTimingDefaults: {},
        configDefaults: [],
        rgb: {},
        qmkKeycodes: qmkKeycodeCatalog.entries,
        qmkKeyLabels: qmkKeycodeCatalog.labels,
        qmkKeycodeAliases: qmkKeycodeCatalog.aliases,
        qmkKeycodeSource: qmkKeycodeCatalog.source,
        macroPayloadKeycodes: [],
        diagnostics,
    };
}

function parseMacroPayloadKeycodes(text) {
    const names = [];
    for (const match of String(text || "").matchAll(/^\s*X\(\s*([A-Z_][A-Z0-9_]*)\s*,/gm)) {
        names.push(match[1]);
    }
    return uniqueStrings(names);
}

async function loadQmkKeycodeCatalog(root) {
    const qmkRoot = await findQmkRoot(root);
    if (!qmkRoot) {
        return fallbackQmkKeycodeCatalog();
    }

    const keycodeDir = path.join(qmkRoot, QMK_KEYCODE_DATA_RELATIVE_PATH);
    const files = await listQmkKeycodeDataFiles(keycodeDir);
    const entriesByValue = new Map();

    for (const file of files) {
        const text = await fs.readFile(file, "utf8");
        for (const entry of parseQmkKeycodeHjsonEntries(text)) {
            const value = preferredQmkKeycodeValue(entry);
            if (!value || entriesByValue.has(value) || shouldSkipQmkKeycode(entry)) {
                continue;
            }
            entriesByValue.set(value, {
                ...entry,
                value,
                ...buildQmkKeycodeSearch(entry, value),
            });
        }
    }

    const entries = Array.from(entriesByValue.values()).sort(compareQmkKeycodes);
    return {
        source: `${path.join(keycodeDir, "*.hjson")} + ${path.join(keycodeDir, "extras", "keycodes_us_*.hjson")}`,
        entries,
        labels: qmkKeyLabelsFromEntries(entries),
        aliases: qmkKeyAliasesFromEntries(entries),
    };
}

async function listQmkKeycodeDataFiles(root) {
    const entries = await fs.readdir(root, { withFileTypes: true });
    const files = entries
        .filter((entry) => entry.isFile() && entry.name.endsWith(".hjson"))
        .map((entry) => path.join(root, entry.name));

    const extrasDir = path.join(root, "extras");
    const extraEntries = await fs.readdir(extrasDir, { withFileTypes: true }).catch(() => []);
    files.push(...extraEntries
        .filter((entry) => entry.isFile() && /^keycodes_us_\d+\.\d+\.\d+\.hjson$/.test(entry.name))
        .map((entry) => path.join(extrasDir, entry.name)));

    return files.sort();
}

async function findQmkRoot(root) {
    const candidates = [
        path.resolve(root, "..", "bastardkb-qmk"),
        path.resolve(root, "..", "qmk_firmware"),
        root,
    ];
    for (const folder of vscode.workspace.workspaceFolders || []) {
        candidates.push(folder.uri.fsPath);
    }
    for (const candidate of uniqueStrings(candidates)) {
        if (await fileExists(path.join(candidate, QMK_KEYCODE_DATA_RELATIVE_PATH))) {
            return candidate;
        }
    }
    return undefined;
}

function parseQmkKeycodeHjsonEntries(text) {
    return parseQmkKeycodeHjsonSectionEntries(text, "keycodes")
        .concat(parseQmkKeycodeHjsonSectionEntries(text, "aliases"));
}

function parseQmkKeycodeHjsonSectionEntries(text, sectionName) {
    let section;
    try {
        section = findInitializerBody(text, new RegExp(`"${escapeRegex(sectionName)}"\\s*:`)).body;
    } catch {
        return [];
    }

    const entries = [];
    const pattern = /"([^"]+)"\s*:\s*\{/g;
    let match;
    while ((match = pattern.exec(section)) !== null) {
        const open = section.indexOf("{", match.index + match[0].length - 1);
        const close = findMatching(section, open, "{", "}");
        const body = section.slice(open + 1, close);
        const key = extractHjsonStringField(body, "key");
        if (!key) {
            pattern.lastIndex = close + 1;
            continue;
        }
        entries.push({
            key,
            label: extractHjsonStringField(body, "label") || key,
            group: extractHjsonStringField(body, "group") || "other",
            aliases: uniqueStrings(extractHjsonStringListField(body, "aliases").filter((alias) => !alias.startsWith("!"))),
        });
        pattern.lastIndex = close + 1;
    }
    return entries;
}

function extractHjsonStringField(body, field) {
    const match = body.match(new RegExp(`"${escapeRegex(field)}"\\s*:\\s*"((?:\\\\.|[^"\\\\])*)"`));
    return match ? decodeHjsonString(match[1]) : "";
}

function extractHjsonStringListField(body, field) {
    const match = body.match(new RegExp(`"${escapeRegex(field)}"\\s*:\\s*\\[([\\s\\S]*?)\\]`));
    if (!match) {
        return [];
    }
    return Array.from(match[1].matchAll(/"((?:\\.|[^"\\])*)"/g)).map((item) => decodeHjsonString(item[1]));
}

function decodeHjsonString(value) {
    try {
        return JSON.parse(`"${value}"`);
    } catch {
        return value;
    }
}

function shouldSkipQmkKeycode(entry) {
    if (!entry.key || entry.key === "SAFE_RANGE") {
        return true;
    }
    if (entry.key.endsWith("_MIN") || entry.key.endsWith("_MAX")) {
        return true;
    }
    if (entry.key.startsWith("QK_") && !(entry.aliases || []).some((alias) => !alias.startsWith("!"))) {
        return entry.key !== "QK_LAYER_LOCK";
    }
    return false;
}

function preferredQmkKeycodeValue(entry) {
    if (!entry.key.startsWith("QK_")) {
        return entry.key;
    }
    return (entry.aliases || []).find((alias) => !alias.startsWith("!")) || entry.key;
}

function qmkKeyLabelsFromEntries(entries) {
    const labels = {};
    for (const entry of entries) {
        const label = entry.label || entry.value;
        labels[entry.value] = label;
        labels[entry.key] = label;
        for (const alias of entry.aliases || []) {
            labels[alias] = label;
        }
    }
    return labels;
}

function qmkKeyAliasesFromEntries(entries) {
    const aliases = {};
    for (const entry of entries) {
        const value = entry.value || entry.key;
        if (!value) {
            continue;
        }
        for (const alias of [entry.value, entry.key].concat(entry.aliases || [])) {
            if (alias && !aliases[alias]) {
                aliases[alias] = value;
            }
        }
    }
    aliases._______ = "_______";
    aliases.XXXXXXX = "XXXXXXX";
    return aliases;
}

function buildQmkKeycodeSearch(entry, value) {
    const terms = uniqueStrings([value, entry.key, entry.label, QMK_KEY_LABELS[value], QMK_KEY_LABELS[entry.key], displayKeyExpression(value)]
        .concat(entry.aliases || [])
        .flatMap(qmkSearchTermVariants));
    return {
        searchTerms: terms,
        search: terms.join(" "),
        searchCompact: uniqueStrings(terms.map(compactSearchToken)).join(" "),
    };
}

function qmkSearchTermVariants(value) {
    const term = String(value || "").trim().toLowerCase();
    if (!term) {
        return [];
    }

    const variants = [term];
    const prefixed = term.match(/^kc_(.+)$/);
    if (prefixed) {
        variants.push(prefixed[1]);
    }

    const compact = compactSearchToken(term);
    if (compact && compact !== term) {
        variants.push(compact);
    }
    return uniqueStrings(variants);
}

function compactSearchToken(value) {
    return String(value || "").toLowerCase().replace(/[^a-z0-9]+/g, "");
}

function fallbackQmkKeycodeCatalog() {
    const entries = Object.entries(QMK_KEY_LABELS).map(([value, label]) => ({
        value,
        key: value,
        label,
        group: value === "_______" || value === "XXXXXXX" ? "internal" : "basic",
        aliases: [],
        ...buildQmkKeycodeSearch({ key: value, label, aliases: [] }, value),
    }));
    return {
        source: "built-in fallback",
        entries,
        labels: { ...QMK_KEY_LABELS },
        aliases: qmkKeyAliasesFromEntries(entries),
    };
}

function canonicalKeyExpression(value, aliases = {}) {
    const normalized = normalizeExpr(value || "");
    if (!normalized) {
        return "";
    }
    if (aliases[normalized]) {
        return aliases[normalized];
    }

    const open = normalized.indexOf("(");
    if (open <= 0 || !normalized.endsWith(")")) {
        return normalized;
    }

    let close;
    try {
        close = findMatching(normalized, open, "(", ")");
    } catch {
        return normalized;
    }
    if (close !== normalized.length - 1) {
        return normalized;
    }

    const helper = normalized.slice(0, open);
    const args = splitTopLevel(normalized.slice(open + 1, -1)).map((arg) => canonicalKeyExpression(arg, aliases));
    return `${helper}(${args.join(", ")})`;
}

function compareQmkKeycodes(left, right) {
    const groupOrder = ["internal", "basic", "modifiers", "media", "system", "mouse", "rgb", "rgb_matrix", "led_matrix", "backlight", "underglow", "magic", "quantum"];
    const leftGroup = groupOrder.indexOf(left.group);
    const rightGroup = groupOrder.indexOf(right.group);
    if (leftGroup !== rightGroup) {
        return (leftGroup === -1 ? 999 : leftGroup) - (rightGroup === -1 ? 999 : rightGroup);
    }
    return String(left.label || left.value).localeCompare(String(right.label || right.value), undefined, { numeric: true });
}

function uniqueStrings(values) {
    return Array.from(new Set(values.filter(Boolean)));
}

function parseLayers(text) {
    const array = findInitializerBody(text, /keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=/);
    const layers = [];
    const layerPattern = /\[([A-Z_][A-Z0-9_]*)\]\s*=\s*LAYOUT\s*\(/g;
    let match;

    while ((match = layerPattern.exec(array.body)) !== null) {
        const layer = match[1];
        const open = array.body.indexOf("(", match.index);
        const close = findMatching(array.body, open, "(", ")");
        const argsBody = array.body.slice(open + 1, close);
        const items = splitTopLevelWithRanges(argsBody).map((item, index) => {
            const tokenRange = trimCodeRange(argsBody, item.start, item.end);
            const rawKeycode = argsBody.slice(tokenRange.start, tokenRange.end).trim();
            const keycode = authoredInternalKeyExpression(rawKeycode) || rawKeycode;
            return {
                layoutIndex: index,
                keycode,
                display: displayKeycode(keycode),
                editLabel: editLabelForKeycode(keycode),
            };
        });

        if (items.length !== LAYOUT_SLOT_COUNT) {
            throw new Error(`${layer} expected ${LAYOUT_SLOT_COUNT} layout entries, got ${items.length}`);
        }

        layers.push({ name: layer, positions: items });
        layerPattern.lastIndex = close + 1;
    }

    return layers;
}

function parseKeymapCustomKeycodes(text) {
    let body;
    try {
        body = findEnumBody(text, /enum\s+keymap_custom_keycodes\s*\{/).body;
    } catch {
        return [];
    }

    return uniqueStrings(splitTopLevel(stripComments(body))
        .map((entry) => entry.split("=")[0].trim())
        .filter((entry) => /^[A-Z_][A-Z0-9_]*$/.test(entry))
        .filter((entry) => entry !== "KEYMAP_CUSTOM_KEYCODE_SENTINEL"));
}

function parseKeyBehaviors(text) {
    const initializer = findInitializerBody(text, /key_behaviors\s*\[\]\s*=/);
    return splitTopLevelWithRanges(initializer.body)
        .map((item) => initializer.body.slice(item.start, item.end).trim())
        .map(trimOuterInitializer)
        .filter(Boolean)
        .map((entry) => {
            const fields = parseDesignatedFields(entry);
            const steps = parseBehaviorSteps(fields[".tap_counts"] || "{}");
            return {
                keycode: normalizeExpr(fields[".keycode"] || ""),
                tapHoldTerm: normalizeExpr(fields[".tap_hold_term"] || ""),
                longerHoldTerm: normalizeExpr(fields[".longer_hold_term"] || ""),
                multiTapTerm: normalizeExpr(fields[".multi_tap_term"] || ""),
                rgbBranchConfirmTerm: normalizeExpr(fields[".rgb_branch_confirm_term"] || ""),
                skipRgbBranchConfirm: ["true", "1"].includes(normalizeExpr(fields[".skip_rgb_branch_confirm"] || "")),
                steps,
            };
        })
        .filter((row) => row.keycode);
}

function parseBehaviorSteps(text) {
    const outer = trimOuterInitializer(text);
    if (!outer) {
        return [];
    }

    return splitTopLevelWithRanges(outer)
        .map((item) => outer.slice(item.start, item.end).trim())
        .filter((item) => item.includes("="))
        .map((item) => {
            const eq = findTopLevelEquals(item);
            const indexMatch = item.slice(0, eq).match(/\[(\d+)\]/);
            const stepBody = trimOuterInitializer(item.slice(eq + 1).trim());
            const fields = parseDesignatedFields(stepBody);
            const tapCount = indexMatch ? Number(indexMatch[1]) : 0;
            return {
                tapCount,
                tapCountName: TAP_COUNT_NAMES[tapCount] || `tap ${tapCount + 1}`,
                tap: parseBehaviorAction(fields[".tap"]),
                hold: parseBehaviorAction(fields[".hold"]),
                longHold: parseBehaviorAction(fields[".long_hold"]),
            };
        });
}

function parseBehaviorAction(value) {
    const normalized = normalizeExpr(value || "");
    if (!normalized) {
        return undefined;
    }

    const open = normalized.indexOf("(");
    if (open === -1 || !normalized.endsWith(")")) {
        return { helper: normalized, action: "", repeatHz: "" };
    }

    const helper = normalized.slice(0, open);
    const args = splitTopLevel(normalized.slice(open + 1, -1)).map(normalizeExpr);
    return {
        helper,
        action: args[0] || "",
        actionDisplay: displayKeyExpression(args[0] || ""),
        repeatHz: args[1] || "",
    };
}

function parseMacroTable(text, macroName, invocationName) {
    const block = findMacroDefinitionBlock(text, macroName);
    const rows = [];

    for (const rawLine of block.text.split(/\r?\n/)) {
        const line = stripInlineLineComment(rawLine).trim();
        if (!line.startsWith(`${invocationName}(`)) {
            continue;
        }

        const open = line.indexOf("(");
        const close = findMatching(line, open, "(", ")");
        rows.push(splitTopLevel(line.slice(open + 1, close)).map(normalizeExpr));
    }

    return rows;
}

function parseComboRow(row) {
    const inputs = (row[1] || "").replace(/^\(/, "").replace(/\)$/, "");
    const output = normalizeExpr(row[0] || "");
    const inputRows = splitTopLevel(inputs).map(normalizeExpr).filter(Boolean);
    return {
        output,
        outputDisplay: displayKeyExpression(output),
        inputs: inputRows,
        inputDisplays: inputRows.map(displayKeyExpression),
    };
}

function parseMacroSlot(row, kind) {
    const keycode = normalizeExpr(row[0] || "");
    return {
        kind,
        keycode,
        payload: parseCString(row[1] || "\"\""),
        empty: parseCString(row[1] || "\"\"") === "",
    };
}

function parseConfigMacros(text) {
    const macros = {};
    for (const line of String(text || "").split(/\r?\n/)) {
        const match = line.match(/^\s*#\s*define\s+([A-Z_][A-Z0-9_]*)\s+(.+?)\s*(?:\/\/.*)?$/);
        if (match) {
            macros[match[1]] = normalizeExpr(match[2]);
        }
    }
    return macros;
}

function parseConfigDefineStates(text) {
    const states = {};
    for (const rawLine of String(text || "").split(/\r?\n/)) {
        const active = rawLine.match(/^\s*#\s*define\s+([A-Z_][A-Z0-9_]*)(?:\s+(.*?))?\s*(?:\/\/.*)?$/);
        if (active) {
            states[active[1]] = {
                defined: true,
                value: normalizeExpr(active[2] || ""),
            };
            continue;
        }

        const commented = rawLine.match(/^\s*\/\/\s*#\s*define\s+([A-Z_][A-Z0-9_]*)(?:\s+(.*?))?\s*(?:\/\/.*)?$/);
        if (commented && !states[commented[1]]) {
            states[commented[1]] = {
                defined: false,
                value: normalizeExpr(commented[2] || ""),
            };
        }
    }
    return states;
}

function resolveConfigDefaultSections(defines) {
    return CONFIG_DEFAULT_SECTIONS.map((section) => ({
        id: section.id,
        label: section.label,
        fields: section.fields.map((field) => {
            const state = defines[field.macro] || {};
            return {
                ...field,
                value: normalizeExpr(state.value || ""),
                enabled: state.defined === true,
                present: Boolean(defines[field.macro]),
            };
        }),
    }));
}

function resolveBehaviorTimingDefaults(macros) {
    return {
        tappingTerm: normalizeExpr(macros.TAPPING_TERM || ""),
        tapHoldTerm: normalizeExpr(macros.CUSTOM_TAP_HOLD_TERM || ""),
        longerHoldTerm: normalizeExpr(macros.CUSTOM_LONGER_HOLD_TERM || ""),
        multiTapTerm: normalizeExpr(macros.CUSTOM_MULTI_TAP_TERM || ""),
        rgbBranchConfirmTerm: normalizeExpr(macros.CUSTOM_RGB_BRANCH_CONFIRM_TERM || ""),
    };
}

function parseRgbConfig(text, configMacros = {}) {
    const ledGroups = parseRgbReusableLedGroups(text);
    const ledGroupMacros = Object.fromEntries(ledGroups.map((group) => [group.name, group.ledIndices]));
    const config = {
        ledGroups,
        layerColors: parseLayerColors(text),
        layerLedGroups: parseRgbLedGroupTable(text, "layer_led_groups_data", ".layer", ledGroupMacros),
        pdModeColors: parsePdModeColors(text),
        pdModeLedGroups: parseRgbLedGroupTable(text, "pd_mode_led_groups_data", ".pointing_mode", ledGroupMacros),
        comboFeedback: parseSimpleColorStruct(text, /combo_feedback_colors\s*=/, [".color", ".locality"]),
        comboFeedbackLedGroups: parseRgbLedGroupTable(text, "combo_feedback_led_groups_data", undefined, ledGroupMacros),
        automouseFade: parseSimpleColorStruct(text, /automouse_fade_end_config\s*=/, [".mode", ".end_color"]),
        keyBehaviorFeedback: parseKeyBehaviorFeedback(text),
        keyBehaviorFeedbackLedGroups: parseRgbLedGroupTable(text, "key_behavior_feedback_led_groups_data", ".semantic", ledGroupMacros),
        defaultColor: resolveDefaultRgbColor(configMacros),
    };
    attachRgbLedGroupUsages(config);
    return config;
}

function resolveDefaultRgbColor(macros) {
    return {
        expression: `HSV(${macros.RGB_MATRIX_DEFAULT_HUE || "0"}, ${macros.RGB_MATRIX_DEFAULT_SAT || "255"}, ${macros.RGB_MATRIX_DEFAULT_VAL || macros.RGB_MATRIX_MAXIMUM_BRIGHTNESS || "255"})`,
        h: macros.RGB_MATRIX_DEFAULT_HUE || "0",
        s: macros.RGB_MATRIX_DEFAULT_SAT || "255",
        v: macros.RGB_MATRIX_DEFAULT_VAL || macros.RGB_MATRIX_MAXIMUM_BRIGHTNESS || "255",
    };
}

function parseLayerColors(text) {
    const initializer = findInitializerBody(text, /layer_colors\s*\[LAYER_COUNT\]\s*=/);
    const rows = [];
    const pattern = /\[([A-Z_][A-Z0-9_]*)\]\s*=\s*\{/g;
    let match;

    while ((match = pattern.exec(initializer.body)) !== null) {
        const layer = match[1];
        const open = initializer.body.indexOf("{", match.index);
        const close = findMatching(initializer.body, open, "{", "}");
        const fields = parseDesignatedFields(initializer.body.slice(open + 1, close));
        rows.push({
            layer,
            color: parseHsv(fields[".color"]),
            mode: normalizeExpr(fields[".mode"] || ""),
        });
        pattern.lastIndex = close + 1;
    }

    return rows;
}

function parsePdModeColors(text) {
    let initializer;
    try {
        initializer = findInitializerBody(text, /pd_mode_colors\s*\[\]\s*=/);
    } catch {
        return [];
    }

    return splitTopLevelWithRanges(initializer.body)
        .map((item) => initializer.body.slice(item.start, item.end).trim())
        .map(trimOuterInitializer)
        .filter(Boolean)
        .map((entry) => {
            const fields = parseDesignatedFields(entry);
            return {
                pointingMode: normalizeExpr(fields[".pointing_mode"] || ""),
                color: parseHsv(fields[".color"]),
                locality: normalizeExpr(fields[".locality"] || ""),
            };
        })
        .filter((row) => row.pointingMode);
}

function parseRgbLedGroupTable(text, tableName, ownerField, macros = parseRgbLedGroupMacros(text)) {
    let body;
    try {
        body = findCallBody(text, new RegExp(`${escapeRegex(tableName)}\\s*\\[\\]\\s*=\\s*RGB_LED_GROUP_TABLE`));
    } catch {
        return [];
    }

    return splitTopLevelWithRanges(body)
        .map((item) => stripComments(body.slice(item.start, item.end)).trim())
        .map(trimOuterInitializer)
        .filter(Boolean)
        .map((entry) => {
            const fields = parseDesignatedFields(entry);
            const ledGroup = parseLedGroupExpression(fields[".led_group"] || "", macros);
            const ledGroupExpression = normalizeExpr(fields[".led_group"] || "");
            const row = {
                color: parseHsv(fields[".color"]),
                ledGroup: ledGroupExpression,
                ledIndices: ledGroup,
                ledGroupKind: macros[ledGroupExpression] ? "reusable" : "inline",
            };
            if (ownerField) {
                row.owner = normalizeExpr(fields[ownerField] || "");
                row.ownerField = ownerField.slice(1);
            }
            return row;
        })
        .filter((row) => row.ledGroup || row.owner || row.color.expression);
}

function parseRgbLedGroupMacros(text) {
    return Object.fromEntries(parseRgbReusableLedGroups(text).map((group) => [group.name, group.ledIndices]));
}

function parseRgbReusableLedGroups(text) {
    const groups = [];
    const pattern = /^\s*#\s*define\s+(RGB_LED_GROUP_[A-Z0-9_]+)\s+RGB_LED_GROUP\s*\(([^)]*)\)/gm;
    let match;
    while ((match = pattern.exec(text)) !== null) {
        const ledIndices = splitTopLevel(match[2]).map(normalizeExpr).filter(Boolean);
        groups.push({
            name: match[1],
            expression: `RGB_LED_GROUP(${ledIndices.join(", ")})`,
            ledIndices,
            usageCount: 0,
            usages: [],
        });
    }
    return groups;
}

function attachRgbLedGroupUsages(config) {
    const groups = Object.fromEntries((config.ledGroups || []).map((group) => [group.name, group]));
    const tables = [
        ["layer", config.layerLedGroups || []],
        ["pdMode", config.pdModeLedGroups || []],
        ["combo", config.comboFeedbackLedGroups || []],
        ["keyBehavior", config.keyBehaviorFeedbackLedGroups || []],
    ];

    for (const [target, rows] of tables) {
        for (const row of rows) {
            const group = groups[row.ledGroup];
            if (!group) {
                continue;
            }
            group.usages.push({
                target,
                owner: row.owner || "",
                color: row.color?.expression || "",
            });
        }
    }

    for (const group of config.ledGroups || []) {
        group.usageCount = group.usages.length;
    }
}

function parseLedGroupExpression(value, macros) {
    const expression = normalizeExpr(value || "");
    if (!expression) {
        return [];
    }
    if (macros[expression]) {
        return macros[expression];
    }
    const match = expression.match(/^RGB_LED_GROUP\s*\(([\s\S]*)\)$/);
    if (!match) {
        return [];
    }
    return splitTopLevel(match[1]).map(normalizeExpr).filter(Boolean);
}

function parseSimpleColorStruct(text, pattern, fieldNames) {
    try {
        const initializer = findInitializerBody(text, pattern);
        const fields = parseDesignatedFields(initializer.body);
        const result = {};
        for (const field of fieldNames) {
            const value = fields[field];
            if (!value) {
                continue;
            }
            result[field.slice(1)] = value.includes("HSV(") ? parseHsv(value) : normalizeExpr(value);
        }
        return result;
    } catch {
        return undefined;
    }
}

function parseKeyBehaviorFeedback(text) {
    let initializer;
    try {
        initializer = findInitializerBody(text, /key_behavior_feedback_colors\s*=/);
    } catch {
        return undefined;
    }

    const fields = parseDesignatedFields(initializer.body);
    let branchColors = [];
    try {
        branchColors = splitTopLevel(findCallBody(initializer.body, /RGB_TAP_BRANCH_COLORS\s*/))
            .map(stripComments)
            .map(parseHsv)
            .filter((color) => color.expression);
    } catch {
        branchColors = [];
    }
    return {
        tapPendingColor: parseHsv(fields[".tap_pending_color"]),
        tapBranchColors: branchColors,
        branchConfirmMode: normalizeExpr(fields[".branch_confirm_mode"] || ""),
        tapCommittedColor: parseHsv(fields[".tap_committed_color"]),
        holdActiveColor: parseHsv(fields[".hold_active_color"]),
        longHoldActiveColor: parseHsv(fields[".long_hold_active_color"]),
        tapCommitMode: normalizeExpr(fields[".tap_commit_mode"] || ""),
        locality: normalizeExpr(fields[".locality"] || ""),
    };
}

function parseHsv(value) {
    const expression = normalizeExpr(value || "");
    const match = expression.match(/^HSV\s*\(([\s\S]*)\)$/);
    if (!match) {
        return {
            expression,
            h: "",
            s: "",
            v: "",
        };
    }

    const args = splitTopLevel(match[1]).map(normalizeExpr);
    return {
        expression,
        h: args[0] || "",
        s: args[1] || "",
        v: args[2] || "",
    };
}

async function patchLayoutKeys(root, target, layer, changes) {
    const context = await readLayoutSlotContext(root, target, layer);
    const normalizedChanges = normalizeLayoutKeyChanges(changes, knownLayoutKeyTokens(context.text));
    if (!normalizedChanges.length) {
        return;
    }

    const replacements = normalizedChanges.map((change) => {
        const token = layoutSlotToken(context, change.layoutIndex);
        return {
            start: token.start,
            end: token.end,
            value: change.keycode,
        };
    }).sort((left, right) => right.start - left.start);

    let next = context.text;
    for (const replacement of replacements) {
        next = replaceRange(next, replacement.start, replacement.end, replacement.value);
    }
    await writeText(context.filePath, next);
}

async function patchLayoutKeyGroups(root, target, groups) {
    const normalizedGroups = normalizeLayoutKeyGroups(groups);
    if (!normalizedGroups.length) {
        return;
    }

    const keymapPath = profileTargetPaths(root, target).keymap;
    let text = await fs.readFile(keymapPath, "utf8");
    const next = patchLayoutKeyGroupsInText(keymapPath, text, normalizedGroups);
    if (next !== text) {
        await writeText(keymapPath, next);
    }
}

function normalizeLayoutKeyGroups(groups) {
    const byLayer = new Map();
    for (const group of Array.isArray(groups) ? groups : []) {
        const layer = normalizeExpr(group?.layer || "");
        const changes = Array.isArray(group?.changes) ? group.changes : [];
        if (!layer || !changes.length) continue;
        assertSafeIdentifier(layer, "layer");
        byLayer.set(layer, (byLayer.get(layer) || []).concat(changes));
    }
    return Array.from(byLayer.entries()).map(([layer, changes]) => ({ layer, changes }));
}

function patchLayoutKeyGroupsInText(filePath, text, normalizedGroups) {
    const knownTokens = knownLayoutKeyTokens(text);
    const replacements = [];
    for (const group of normalizedGroups) {
        const context = layoutSlotContextFromText(filePath, text, group.layer);
        const normalizedChanges = normalizeLayoutKeyChanges(group.changes, knownTokens);
        for (const change of normalizedChanges) {
            const token = layoutSlotToken(context, change.layoutIndex);
            replacements.push({
                start: token.start,
                end: token.end,
                value: change.keycode,
            });
        }
    }
    if (!replacements.length) {
        return text;
    }

    replacements.sort((left, right) => right.start - left.start);
    for (const replacement of replacements) {
        text = replaceRange(text, replacement.start, replacement.end, replacement.value);
    }
    return text;
}

async function applyAllChanges(root, target, message) {
    const normalizedAdds = normalizeLayerAdds(message?.adds);
    const normalizedDeletes = normalizeLayerDeletes(message?.deletes);
    const layoutGroups = normalizeLayoutKeyGroups(message?.layoutGroups);
    if (!normalizedAdds.length && !normalizedDeletes.length && !layoutGroups.length) {
        return;
    }
    if (!normalizedAdds.length && !normalizedDeletes.length) {
        await patchLayoutKeyGroups(root, target, layoutGroups);
        return;
    }

    const paths = profileTargetPaths(root, target);
    const configPath = paths.config;
    const keymapPath = paths.keymap;
    const rgbPath = paths.rgb;
    let configText = await fs.readFile(configPath, "utf8");
    let keymapText = await fs.readFile(keymapPath, "utf8");
    let rgbText = await fs.readFile(rgbPath, "utf8");

    validateLayerChangeRequest(normalizedAdds, normalizedDeletes, keymapText);

    for (const layer of normalizedAdds) {
        configText = insertLayerEnumEntry(configText, layer.name);
        keymapText = insertKeymapLayerBlock(keymapText, layer.name, layer.keycodes);
        rgbText = insertLayerColorEntry(rgbText, layer);
    }

    if (layoutGroups.length) {
        keymapText = patchLayoutKeyGroupsInText(keymapPath, keymapText, layoutGroups);
    }

    for (const layer of normalizedDeletes) {
        configText = removeLayerEnumEntry(configText, layer);
        keymapText = removeKeymapLayerBlock(keymapText, layer);
        rgbText = removeLayerColorEntry(rgbText, layer);
        rgbText = removeLayerLedGroupRows(rgbText, layer);
    }

    for (const layer of normalizedDeletes) {
        const remaining = remainingLayerReferences(layer, [configText, keymapText, rgbText]);
        if (remaining.length) {
            throw new Error(`Cannot delete ${layer}; references remain in ${remaining.join(", ")}.`);
        }
    }

    await writeText(configPath, configText);
    await writeText(keymapPath, keymapText);
    await writeText(rgbPath, rgbText);
}

function validateLayerChangeRequest(normalizedAdds, normalizedDeletes, keymapText) {
    const duplicateAdd = duplicateLayerName(normalizedAdds.map((layer) => layer.name));
    if (duplicateAdd) {
        throw new Error(`Layer ${duplicateAdd} is staged more than once.`);
    }
    for (const layer of normalizedAdds) {
        if (normalizedDeletes.includes(layer.name)) {
            throw new Error(`Layer ${layer.name} cannot be added and deleted in the same apply.`);
        }
    }

    const existingLayerNames = new Set(parseLayers(keymapText).map((layer) => layer.name));
    for (const layer of normalizedDeletes) {
        if (layer === "LAYER_BASE") {
            throw new Error("LAYER_BASE cannot be deleted.");
        }
        if (!existingLayerNames.has(layer)) {
            throw new Error(`Cannot delete missing layer ${layer}.`);
        }
    }
    for (const layer of normalizedAdds) {
        if (existingLayerNames.has(layer.name)) {
            throw new Error(`Layer ${layer.name} already exists.`);
        }
    }
}

async function applyLayerChanges(root, target, adds, deletes) {
    const normalizedAdds = normalizeLayerAdds(adds);
    const normalizedDeletes = normalizeLayerDeletes(deletes);
    if (!normalizedAdds.length && !normalizedDeletes.length) {
        return;
    }

    const paths = profileTargetPaths(root, target);
    const configPath = paths.config;
    const keymapPath = paths.keymap;
    const rgbPath = paths.rgb;
    let configText = await fs.readFile(configPath, "utf8");
    let keymapText = await fs.readFile(keymapPath, "utf8");
    let rgbText = await fs.readFile(rgbPath, "utf8");

    validateLayerChangeRequest(normalizedAdds, normalizedDeletes, keymapText);

    for (const layer of normalizedDeletes) {
        configText = removeLayerEnumEntry(configText, layer);
        keymapText = removeKeymapLayerBlock(keymapText, layer);
        rgbText = removeLayerColorEntry(rgbText, layer);
        rgbText = removeLayerLedGroupRows(rgbText, layer);
    }
    for (const layer of normalizedAdds) {
        configText = insertLayerEnumEntry(configText, layer.name);
        keymapText = insertKeymapLayerBlock(keymapText, layer.name, layer.keycodes);
        rgbText = insertLayerColorEntry(rgbText, layer);
    }

    for (const layer of normalizedDeletes) {
        const remaining = remainingLayerReferences(layer, [configText, keymapText, rgbText]);
        if (remaining.length) {
            throw new Error(`Cannot delete ${layer}; references remain in ${remaining.join(", ")}.`);
        }
    }

    await writeText(configPath, configText);
    await writeText(keymapPath, keymapText);
    await writeText(rgbPath, rgbText);
}

function normalizeLayerAdds(adds) {
    return (Array.isArray(adds) ? adds : []).map((layer) => {
        const name = normalizeLayerName(layer?.name);
        assertSafeIdentifier(name, "layer");
        const keycodes = (Array.isArray(layer?.keycodes) ? layer.keycodes : [])
            .map((keycode) => normalizeUserKeyExpression(keycode || "_______"));
        if (keycodes.length !== LAYOUT_SLOT_COUNT) {
            throw new Error(`${name} expected ${LAYOUT_SLOT_COUNT} layout entries, got ${keycodes.length}`);
        }
        for (const keycode of keycodes) {
            assertSafeExpression(keycode, `${name} keycode`);
            assertLayoutKeyExpression(keycode, `${name} keycode`);
        }

        const hue = normalizeExpr(layer?.color?.h ?? "0");
        const sat = normalizeExpr(layer?.color?.s ?? "255");
        const val = normalizeExpr(layer?.color?.v ?? "RGB_MATRIX_MAXIMUM_BRIGHTNESS");
        assertUint8Channel(hue, `${name} hue`);
        assertUint8Channel(sat, `${name} saturation`);
        assertSafeExpression(val, `${name} value`);
        const mode = normalizeExpr(layer?.color?.mode || "KEYS_MAPPED_ON_THIS_LAYER_ONLY");
        assertAllowed(mode, LAYER_COLOR_MODES, `${name} layer color mode`);
        return { name, keycodes, color: { h: hue, s: sat, v: val }, mode };
    });
}

function normalizeLayerDeletes(deletes) {
    return uniqueStrings((Array.isArray(deletes) ? deletes : [])
        .map((layer) => normalizeLayerName(layer))
        .filter(Boolean));
}

function normalizeLayerName(value) {
    const normalized = String(value || "").trim().toUpperCase().replace(/[^A-Z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
    if (!normalized) return "";
    return normalized.startsWith("LAYER_") ? normalized : `LAYER_${normalized}`;
}

function duplicateLayerName(names) {
    const seen = new Set();
    for (const name of names || []) {
        if (seen.has(name)) return name;
        seen.add(name);
    }
    return "";
}

function insertLayerEnumEntry(text, layer) {
    assertSafeIdentifier(layer, "layer");
    const body = findEnumBody(text, /enum\s+charybdis_keymap_layers\s*\{/);
    if (new RegExp(`\\b${escapeRegex(layer)}\\b`).test(body.body)) {
        throw new Error(`Layer ${layer} already exists in config.h.`);
    }
    const sentinel = /^(\s*)LAYER_COUNT\b/m.exec(body.body);
    if (!sentinel) {
        throw new Error("Could not find LAYER_COUNT in layer enum.");
    }
    const insertAt = body.bodyStart + sentinel.index;
    const indent = sentinel[1] || "    ";
    return replaceRange(text, insertAt, insertAt, `${indent}${layer},\n`);
}

function removeLayerEnumEntry(text, layer) {
    assertSafeIdentifier(layer, "layer");
    const body = findEnumBody(text, /enum\s+charybdis_keymap_layers\s*\{/);
    const pattern = new RegExp(`^\\s*${escapeRegex(layer)}\\b[^\\n]*(?:\\n|$)`, "m");
    const match = pattern.exec(body.body);
    if (!match) {
        throw new Error(`Could not find ${layer} in layer enum.`);
    }
    return replaceRange(text, body.bodyStart + match.index, body.bodyStart + match.index + match[0].length, "");
}

function insertKeymapLayerBlock(text, layer, keycodes) {
    assertSafeIdentifier(layer, "layer");
    const array = findInitializerBody(text, /keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=/);
    if (new RegExp(`\\[${escapeRegex(layer)}\\]\\s*=`).test(array.body)) {
        throw new Error(`Layer ${layer} already exists in keymap.c.`);
    }
    const clang = /(?:^|\n)\s*\/\/\s*clang-format on\b/.exec(array.body);
    const insertAt = clang ? array.bodyStart + clang.index + (clang[0].startsWith("\n") ? 1 : 0) : array.bodyEnd;
    return replaceRange(text, insertAt, insertAt, formatKeymapLayerBlock(layer, keycodes));
}

function removeKeymapLayerBlock(text, layer) {
    assertSafeIdentifier(layer, "layer");
    const array = findInitializerBody(text, /keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=/);
    const call = findLayerLayoutCall(array.body, layer);
    const entryStart = text.lastIndexOf("\n", array.bodyStart + call.matchStart) + 1;
    let entryEnd = array.bodyStart + call.argsEnd + 1;
    while (entryEnd < text.length && /\s/.test(text[entryEnd])) entryEnd += 1;
    if (text[entryEnd] === ",") entryEnd += 1;
    if (text[entryEnd] === "\r") entryEnd += 1;
    if (text[entryEnd] === "\n") entryEnd += 1;
    return replaceRange(text, entryStart, entryEnd, "");
}

function formatKeymapLayerBlock(layer, keycodes) {
    const rows = [
        keycodes.slice(0, 12),
        keycodes.slice(12, 24),
        keycodes.slice(24, 36),
        keycodes.slice(36, 48),
        keycodes.slice(48, 53),
        keycodes.slice(53, 56),
    ];
    const lines = [`    [${layer}] = LAYOUT(`];
    lines.push("  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮");
    lines.push(formatKeymapMainRow(rows[0]));
    lines.push("  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤");
    lines.push(formatKeymapMainRow(rows[1]));
    lines.push("  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤");
    lines.push(formatKeymapMainRow(rows[2]));
    lines.push("  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤");
    lines.push(formatKeymapMainRow(rows[3]));
    lines.push("  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯");
    lines.push(formatKeymapThumbRow(rows[4], 75));
    lines.push(formatKeymapThumbRow(rows[5], 94, false));
    lines.push("  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯");
    lines.push("    ),");
    lines.push("");
    return lines.join("\n");
}

function formatKeymapMainRow(keys) {
    const left = " ".repeat(18) + keys[0] + keys.slice(1, 6).map((keycode) => "," + keycode.padStart(18, " ")).join("");
    const right = keys.slice(6);
    return left + ",   " + right[0].padStart(18, " ") + right.slice(1).map((keycode) => "," + keycode.padStart(18, " ")).join("") + ",";
}

function formatKeymapThumbRow(keys, indent, trailingComma = true) {
    const suffix = trailingComma ? "," : "";
    return " ".repeat(indent) + keys[0] + keys.slice(1).map((keycode, index) => {
        const gap = (keys.length === 5 && index === 2) || (keys.length === 3 && index === 1) ? ",   " : ",";
        return gap + keycode.padStart(18, " ");
    }).join("") + suffix;
}

function insertLayerColorEntry(text, layer) {
    assertSafeIdentifier(layer.name, "layer");
    const initializer = findInitializerBody(text, /layer_colors\s*\[LAYER_COUNT\]\s*=/);
    if (new RegExp(`\\[${escapeRegex(layer.name)}\\]\\s*=`).test(initializer.body)) {
        throw new Error(`Layer ${layer.name} already exists in layer_colors[].`);
    }
    const entry = `    [${layer.name}] =
        {
            .color = HSV(${layer.color.h}, ${layer.color.s}, ${layer.color.v}),
            .mode  = ${layer.mode},
        },
`;
    return replaceRange(text, initializer.bodyEnd, initializer.bodyEnd, entry);
}

function removeLayerColorEntry(text, layer) {
    return removeDesignatedInitializerEntry(text, /layer_colors\s*\[LAYER_COUNT\]\s*=/, `[${layer}]`);
}

function removeDesignatedInitializerEntry(text, initializerPattern, designator) {
    const initializer = findInitializerBody(text, initializerPattern);
    const entries = splitTopLevelWithRanges(initializer.body);
    const entry = entries.find((candidate) => initializer.body.slice(candidate.start, candidate.end).trim().startsWith(`${designator} =`));
    if (!entry) {
        throw new Error(`Could not find ${designator} entry.`);
    }
    const start = text.lastIndexOf("\n", initializer.bodyStart + entry.start) + 1;
    let end = initializer.bodyStart + entry.end;
    while (end < text.length && /\s/.test(text[end])) end += 1;
    if (text[end] === ",") end += 1;
    if (text[end] === "\r") end += 1;
    if (text[end] === "\n") end += 1;
    return replaceRange(text, start, end, "");
}

function removeLayerLedGroupRows(text, layer) {
    const range = findCallRange(text, /layer_led_groups_data\s*\[\]\s*=\s*RGB_LED_GROUP_TABLE\s*/);
    const rows = splitTopLevelWithRanges(range.body)
        .filter((row) => {
            const source = range.body.slice(row.start, row.end).trim();
            if (!source || source.startsWith("//") || source.startsWith("/*")) return false;
            return new RegExp(`\\.layer\\s*=\\s*${escapeRegex(layer)}\\b`).test(source);
        })
        .sort((left, right) => right.start - left.start);
    let next = text;
    for (const row of rows) {
        let start = next.lastIndexOf("\n", range.bodyStart + row.start) + 1;
        let end = range.bodyStart + row.end;
        while (end < next.length && /\s/.test(next[end])) end += 1;
        if (next[end] === ",") end += 1;
        if (next[end] === "\r") end += 1;
        if (next[end] === "\n") end += 1;
        next = replaceRange(next, start, end, "");
    }
    return next;
}

function remainingLayerReferences(layer, texts, labels = [KEYMAP_CONFIG_RELATIVE_PATH, KEYMAP_RELATIVE_PATH, RGB_RELATIVE_PATH]) {
    return texts.map((text, index) => ({ text, label: labels[index] }))
        .filter((entry) => new RegExp(`\\b${escapeRegex(layer)}\\b`).test(maskCommentsPreserveLength(entry.text)))
        .map((entry) => entry.label);
}

function normalizeLayoutKeyChanges(changes, knownTokens = new Set()) {
    const byIndex = new Map();
    for (const change of Array.isArray(changes) ? changes : []) {
        const layoutIndex = Number(change?.layoutIndex);
        assertLayoutIndex(layoutIndex);
        const keycode = normalizeUserKeyExpression(change?.keycode || "");
        assertSafeExpression(keycode, "keycode");
        assertLayoutKeyExpression(keycode, "layout keycode", knownTokens);
        byIndex.set(layoutIndex, normalizeExpr(keycode));
    }
    return Array.from(byIndex.entries()).map(([layoutIndex, keycode]) => ({ layoutIndex, keycode }));
}

function knownLayoutKeyTokens(keymapText) {
    const tokens = new Set(Object.keys(QMK_KEY_LABELS));
    tokens.add("_______");
    tokens.add("XXXXXXX");

    try {
        for (const layer of parseLayers(keymapText)) {
            for (const position of layer.positions || []) {
                collectLayoutKeyTokens(position.keycode, tokens);
            }
        }
    } catch {
        // The caller already validates the active LAYOUT block. Known-token
        // collection is only a guard against accepting random clipboard text.
    }

    return tokens;
}

function collectLayoutKeyTokens(expression, tokens) {
    for (const match of String(expression || "").matchAll(/\b[A-Z][A-Z0-9_]*\b/g)) {
        tokens.add(match[0]);
    }
}

async function readLayoutSlotContext(root, target, layer) {
    const filePath = profileTargetPaths(root, target).keymap;
    const text = await fs.readFile(filePath, "utf8");
    return layoutSlotContextFromText(filePath, text, layer);
}

function layoutSlotContextFromText(filePath, text, layer) {
    const array = findInitializerBody(text, /keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=/);
    const layerCall = findLayerLayoutCall(array.body, layer);
    const argsBody = array.body.slice(layerCall.argsStart, layerCall.argsEnd);
    const items = splitTopLevelWithRanges(argsBody);
    if (items.length !== LAYOUT_SLOT_COUNT) {
        throw new Error(`${layer} expected ${LAYOUT_SLOT_COUNT} layout entries, got ${items.length}`);
    }

    return { filePath, text, array, layerCall, argsBody, items };
}

function layoutSlotToken(context, layoutIndex) {
    assertLayoutIndex(layoutIndex);
    const item = context.items[layoutIndex];
    const token = trimCodeRange(context.argsBody, item.start, item.end);
    return {
        start: context.array.bodyStart + context.layerCall.argsStart + token.start,
        end: context.array.bodyStart + context.layerCall.argsStart + token.end,
        text: context.argsBody.slice(token.start, token.end),
    };
}

function assertLayoutIndex(layoutIndex) {
    if (!Number.isInteger(layoutIndex) || layoutIndex < 0 || layoutIndex >= LAYOUT_SLOT_COUNT) {
        throw new Error(`Invalid layout index: ${layoutIndex}`);
    }
}

async function patchLayerColor(root, target, layer, hue, sat, val, mode) {
    assertSafeIdentifier(layer, "layer");
    assertSafeHsv(hue, sat, val);
    assertAllowed(mode, LAYER_COLOR_MODES, "layer color mode");

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /layer_colors\s*\[LAYER_COUNT\]\s*=/);
    const entry = findDesignatedEntry(initializer.body, `[${layer}]`);
    let next = patchFieldExpressionInRange(
        text,
        initializer.bodyStart + entry.start,
        initializer.bodyStart + entry.end,
        ".mode",
        mode
    );
    next = patchFieldExpressionInRange(
        next,
        initializer.bodyStart + entry.start,
        initializer.bodyStart + entry.end,
        ".color",
        `HSV(${hue}, ${sat}, ${val})`
    );
    await writeText(filePath, next);
}

async function patchPdModeColor(root, target, pointingMode, hue, sat, val, locality) {
    assertSafeIdentifier(pointingMode, "pointing mode");
    assertSafeHsv(hue, sat, val);
    assertAllowed(locality, RGB_LOCALITIES, "RGB locality");

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /pd_mode_colors\s*\[\]\s*=/);
    const entry = findStructEntryByField(initializer.body, ".pointing_mode", pointingMode);
    let next = patchFieldExpressionInRange(
        text,
        initializer.bodyStart + entry.start,
        initializer.bodyStart + entry.end,
        ".locality",
        locality
    );
    next = patchFieldExpressionInRange(
        next,
        initializer.bodyStart + entry.start,
        initializer.bodyStart + entry.end,
        ".color",
        `HSV(${hue}, ${sat}, ${val})`
    );
    await writeText(filePath, next);
}

async function patchAutomouseFade(root, target, mode, hue, sat, val) {
    assertAllowed(mode, AUTOMOUSE_FADE_MODES, "auto-mouse fade mode");
    assertSafeHsv(hue, sat, val);

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /automouse_fade_end_config\s*=/);
    let next = patchFieldExpressionInRange(text, initializer.bodyStart, initializer.bodyEnd, ".mode", mode);
    next = patchFieldInInitializer(next, /automouse_fade_end_config\s*=/, ".end_color", hsvExpression(hue, sat, val));
    await writeText(filePath, next);
}

async function patchComboFeedback(root, target, hue, sat, val, locality) {
    assertSafeHsv(hue, sat, val);
    assertAllowed(locality, RGB_LOCALITIES, "combo feedback locality");

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /combo_feedback_colors\s*=/);
    let next = patchFieldExpressionInRange(text, initializer.bodyStart, initializer.bodyEnd, ".locality", locality);
    next = patchFieldInInitializer(next, /combo_feedback_colors\s*=/, ".color", hsvExpression(hue, sat, val));
    await writeText(filePath, next);
}

async function patchKeyBehaviorFeedback(root, target, config) {
    const colors = {
        tapPendingColor: normalizeHsvRequest(config?.tapPendingColor, "tap pending color"),
        tapCommittedColor: normalizeHsvRequest(config?.tapCommittedColor, "tap committed color"),
        holdActiveColor: normalizeHsvRequest(config?.holdActiveColor, "hold active color"),
        longHoldActiveColor: normalizeHsvRequest(config?.longHoldActiveColor, "long-hold active color"),
        tapBranchColors: Array.isArray(config?.tapBranchColors)
            ? config.tapBranchColors.map((color, index) => normalizeHsvRequest(color, `tap count ${index + 2} branch color`))
            : [],
    };
    const branchConfirmMode = normalizeExpr(config?.branchConfirmMode || "");
    const tapCommitMode = normalizeExpr(config?.tapCommitMode || "");
    const locality = normalizeExpr(config?.locality || "");
    assertAllowed(branchConfirmMode, KEY_FEEDBACK_BRANCH_CONFIRM_MODES, "branch confirm mode");
    assertAllowed(tapCommitMode, KEY_FEEDBACK_TAP_COMMIT_MODES, "tap commit mode");
    assertAllowed(locality, RGB_LOCALITIES, "key behavior feedback locality");

    const filePath = profileTargetPaths(root, target).rgb;
    let text = await fs.readFile(filePath, "utf8");
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_pending_color", colors.tapPendingColor.expression);
    text = patchRgbTapBranchColorsInInitializer(text, /key_behavior_feedback_colors\s*=/, colors.tapBranchColors);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".branch_confirm_mode", branchConfirmMode);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_committed_color", colors.tapCommittedColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_commit_mode", tapCommitMode);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".hold_active_color", colors.holdActiveColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".long_hold_active_color", colors.longHoldActiveColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".locality", locality);
    await writeText(filePath, text);
}

async function patchConfigDefaults(root, target, fields) {
    const normalizedFields = normalizeConfigDefaultRequests(fields);
    if (!normalizedFields.length) {
        return;
    }

    const filePath = profileTargetPaths(root, target).config;
    let text = await fs.readFile(filePath, "utf8");
    for (const field of normalizedFields) {
        text = field.kind === "toggle"
            ? patchConfigToggleMacroInText(text, field.macro, field.enabled)
            : patchConfigValueMacroInText(text, field.macro, field.value);
    }
    await writeText(filePath, text);
}

function normalizeConfigDefaultRequests(fields) {
    return (Array.isArray(fields) ? fields : []).map((field) => {
        const macro = normalizeExpr(field?.macro || "");
        const descriptor = CONFIG_DEFAULT_FIELD_BY_MACRO.get(macro);
        if (!descriptor) {
            throw new Error(`Unsupported config default: ${macro}`);
        }
        if (descriptor.kind === "toggle") {
            return {
                macro,
                kind: descriptor.kind,
                enabled: Boolean(field?.enabled),
            };
        }
        const value = normalizeExpr(field?.value || "");
        assertConfigDefaultValue(descriptor, value);
        return {
            macro,
            kind: descriptor.kind,
            value,
        };
    });
}

function assertConfigDefaultValue(descriptor, value) {
    if (!value) {
        throw new Error(`${descriptor.label} cannot be empty.`);
    }

    switch (descriptor.validate) {
        case "timing-ms":
            if (!keyBehaviorTimingInRange(value, false)) {
                throw new Error(`${descriptor.label} must be a positive integer from 1 to ${KEY_BEHAVIOR_TIMING_MAX_MS} ms.`);
            }
            return;
        case "positive-int":
            if (!/^\d+$/.test(value) || Number(value) <= 0) {
                throw new Error(`${descriptor.label} must be a positive integer.`);
            }
            return;
        case "nonnegative-int":
            if (!/^\d+$/.test(value)) {
                throw new Error(`${descriptor.label} must be zero or a positive integer.`);
            }
            return;
        case "uint8":
            assertUint8Channel(value, descriptor.label);
            return;
        case "identifier":
            assertSafeIdentifier(value, descriptor.label);
            return;
        case "layer":
            assertSafeIdentifier(value, descriptor.label);
            if (!value.startsWith("LAYER_")) {
                throw new Error(`${descriptor.label} must be a LAYER_* identifier.`);
            }
            return;
        case "safe-expression":
            assertSafeExpression(value, descriptor.label);
            return;
        default:
            assertSafeExpression(value, descriptor.label);
    }
}

function patchConfigValueMacroInText(text, macro, value) {
    const line = findConfigDefineLine(text, macro, false);
    if (!line) {
        throw new Error(`Could not find active #define ${macro} in config.h.`);
    }

    const source = line.text;
    const match = source.match(new RegExp(`^(\\s*#\\s*define\\s+${escapeRegex(macro)}\\b)(\\s+)([\\s\\S]*)$`));
    if (!match) {
        throw new Error(`Could not parse #define ${macro}.`);
    }

    const code = stripInlineLineComment(source);
    const valueStart = match[1].length + match[2].length;
    const valueEnd = code.replace(/\s+$/g, "").length;
    if (valueEnd < valueStart) {
        throw new Error(`Could not patch #define ${macro}.`);
    }
    return replaceRange(text, line.start + valueStart, line.start + valueEnd, value);
}

function patchConfigToggleMacroInText(text, macro, enabled) {
    const line = findConfigDefineLine(text, macro, true);
    if (!line) {
        throw new Error(`Could not find #define ${macro} in config.h.`);
    }
    if (line.active === enabled) {
        return text;
    }

    if (enabled) {
        const uncommented = line.text.replace(/^(\s*)\/\/\s*/, "$1");
        return replaceRange(text, line.start, line.end, uncommented);
    }

    const commented = line.text.replace(/^(\s*)/, "$1// ");
    return replaceRange(text, line.start, line.end, commented);
}

function findConfigDefineLine(text, macro, includeCommented) {
    const activePattern = new RegExp(`^([^\\S\\r\\n]*#\\s*define\\s+${escapeRegex(macro)}\\b[^\\r\\n]*)`, "m");
    const active = activePattern.exec(text);
    if (active) {
        return {
            start: active.index,
            end: active.index + active[1].length,
            text: active[1],
            active: true,
        };
    }
    if (!includeCommented) {
        return undefined;
    }

    const commentedPattern = new RegExp(`^([^\\S\\r\\n]*//\\s*#\\s*define\\s+${escapeRegex(macro)}\\b[^\\r\\n]*)`, "m");
    const commented = commentedPattern.exec(text);
    if (!commented) {
        return undefined;
    }
    return {
        start: commented.index,
        end: commented.index + commented[1].length,
        text: commented[1],
        active: false,
    };
}

function normalizeHsvRequest(color, label) {
    const hue = color?.hue;
    const sat = color?.sat;
    const val = color?.val;
    assertSafeHsv(hue, sat, val);
    return {
        hue: normalizeExpr(hue),
        sat: normalizeExpr(sat),
        val: normalizeExpr(val),
        expression: hsvExpression(hue, sat, val),
    };
}

function hsvExpression(hue, sat, val) {
    return `HSV(${normalizeExpr(hue)}, ${normalizeExpr(sat)}, ${normalizeExpr(val)})`;
}

async function saveRgbReusableLedGroup(root, target, group) {
    const originalName = normalizeExpr(group?.originalName || "");
    const name = normalizeExpr(group?.name || "");
    assertSafeRgbLedGroupName(name);

    const ledIndices = normalizeLedIndices(group?.ledIndices);
    if (!ledIndices.length) {
        throw new Error("Select at least one LED for the reusable RGB group.");
    }

    const filePath = profileTargetPaths(root, target).rgb;
    let text = await fs.readFile(filePath, "utf8");
    const groups = parseRgbReusableLedGroups(text);
    const existingNames = new Set(groups.map((entry) => entry.name));

    if (originalName) {
        assertSafeRgbLedGroupName(originalName);
        if (!existingNames.has(originalName)) {
            throw new Error(`Reusable RGB LED group not found: ${originalName}`);
        }
        if (name !== originalName && existingNames.has(name)) {
            throw new Error(`Reusable RGB LED group already exists: ${name}`);
        }
        text = replaceRgbReusableLedGroupDefine(text, originalName, name, ledIndices);
        if (name !== originalName) {
            text = text.replace(new RegExp(`\\b${escapeRegex(originalName)}\\b`, "g"), name);
        }
    } else if (existingNames.has(name)) {
        throw new Error(`Reusable RGB LED group already exists: ${name}`);
    } else {
        text = insertRgbReusableLedGroupDefine(text, name, ledIndices);
    }

    await writeText(filePath, text);
}

async function deleteRgbReusableLedGroup(root, target, name) {
    name = normalizeExpr(name || "");
    assertSafeRgbLedGroupName(name);

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const rgb = parseRgbConfig(text);
    const group = (rgb.ledGroups || []).find((entry) => entry.name === name);
    if (!group) {
        throw new Error(`Reusable RGB LED group not found: ${name}`);
    }
    if (group.usageCount > 0) {
        throw new Error(`Cannot delete ${name}; it is used by ${group.usageCount} active LED group row(s).`);
    }

    await writeText(filePath, removeRgbReusableLedGroupDefine(text, name));
}

function assertSafeRgbLedGroupName(name) {
    assertSafeIdentifier(name, "reusable RGB LED group name");
    if (!String(name).startsWith("RGB_LED_GROUP_")) {
        throw new Error(`Reusable RGB LED group names must start with RGB_LED_GROUP_: ${name}`);
    }
}

function rgbReusableLedGroupDefineLine(name, ledIndices) {
    return `#    define ${name} RGB_LED_GROUP(${ledIndices.join(", ")})`;
}

function rgbReusableLedGroupDefineRange(text, name) {
    const pattern = new RegExp(`^[ \\t]*#\\s*define\\s+${escapeRegex(name)}\\s+RGB_LED_GROUP\\s*\\([^\\n]*\\)(?:\\s*//[^\\n]*)?`, "m");
    const match = pattern.exec(text);
    if (!match) {
        throw new Error(`Reusable RGB LED group define not found: ${name}`);
    }
    let end = match.index + match[0].length;
    if (text[end] === "\r" && text[end + 1] === "\n") {
        end += 2;
    } else if (text[end] === "\n") {
        end += 1;
    }
    return { start: match.index, end };
}

function replaceRgbReusableLedGroupDefine(text, originalName, name, ledIndices) {
    const range = rgbReusableLedGroupDefineRange(text, originalName);
    const trailingNewline = text.slice(range.start, range.end).endsWith("\n") ? "\n" : "";
    return replaceRange(text, range.start, range.end, rgbReusableLedGroupDefineLine(name, ledIndices) + trailingNewline);
}

function removeRgbReusableLedGroupDefine(text, name) {
    const range = rgbReusableLedGroupDefineRange(text, name);
    return replaceRange(text, range.start, range.end, "");
}

function insertRgbReusableLedGroupDefine(text, name, ledIndices) {
    const pattern = /^[ \t]*#\s*define\s+RGB_LED_GROUP_[A-Z0-9_]+\s+RGB_LED_GROUP\s*\([^\n]*\)(?:\s*\/\/[^\n]*)?/gm;
    let match;
    let last;
    while ((match = pattern.exec(text)) !== null) {
        last = match;
    }
    if (!last) {
        throw new Error("Could not find reusable RGB LED group define block in rgb_config.c.");
    }
    let insertAt = last.index + last[0].length;
    if (text[insertAt] === "\r" && text[insertAt + 1] === "\n") {
        insertAt += 2;
    } else if (text[insertAt] === "\n") {
        insertAt += 1;
    }
    return replaceRange(text, insertAt, insertAt, `${rgbReusableLedGroupDefineLine(name, ledIndices)}\n`);
}

async function appendRgbLedGroup(root, target, group) {
    const groupTarget = normalizeExpr(group?.target || "");
    const config = RGB_LED_GROUP_TARGETS[groupTarget];
    if (!config) {
        throw new Error(`Invalid RGB LED group target: ${groupTarget}`);
    }

    assertSafeHsv(group?.hue, group?.sat, group?.val);

    const filePath = profileTargetPaths(root, target).rgb;
    const text = await fs.readFile(filePath, "utf8");
    const reusableGroupName = normalizeExpr(group?.ledGroupName || "");
    let ledGroupExpression;
    if (reusableGroupName) {
        assertSafeRgbLedGroupName(reusableGroupName);
        const reusableGroups = parseRgbReusableLedGroups(text).map((entry) => entry.name);
        if (!reusableGroups.includes(reusableGroupName)) {
            throw new Error(`Reusable RGB LED group not found: ${reusableGroupName}`);
        }
        ledGroupExpression = reusableGroupName;
    } else {
        const ledIndices = normalizeLedIndices(group?.ledIndices);
        if (!ledIndices.length) {
            throw new Error("Select at least one LED for the RGB group.");
        }
        ledGroupExpression = `RGB_LED_GROUP(${ledIndices.join(", ")})`;
    }

    const fields = [];
    if (config.ownerField) {
        const owner = normalizeExpr(group?.owner || "");
        assertSafeIdentifier(owner, config.ownerLabel);
        fields.push(`${config.ownerField} = ${owner}`);
    }
    fields.push(`.color = HSV(${normalizeExpr(group?.hue)}, ${normalizeExpr(group?.sat)}, ${normalizeExpr(group?.val)})`);
    fields.push(`.led_group = ${ledGroupExpression}`);

    const call = findCallRange(text, new RegExp(`${escapeRegex(config.tableName)}\\s*\\[\\]\\s*=\\s*RGB_LED_GROUP_TABLE`));
    const prefix = call.body.endsWith("\n") ? "" : "\n";
    const insertion = `${prefix}    { ${fields.join(", ")} },\n`;
    await writeText(filePath, replaceRange(text, call.bodyEnd, call.bodyEnd, insertion));
}

function normalizeLedIndices(values) {
    if (!Array.isArray(values)) {
        throw new Error("LED group indices must be an array.");
    }

    const seen = new Set();
    const result = [];
    for (const value of values) {
        const index = Number(value);
        if (!Number.isInteger(index) || index < 0 || index > 56) {
            throw new Error(`Invalid LED index: ${value}`);
        }
        if (!seen.has(index)) {
            seen.add(index);
            result.push(index);
        }
    }
    return result;
}

async function patchViaMacro(root, target, keycode, payload) {
    assertSafeIdentifier(keycode, "VIA macro keycode");

    const filePath = profileTargetPaths(root, target).keymap;
    const text = await fs.readFile(filePath, "utf8");
    const block = findMacroDefinitionBlock(text, "VIA_MACROS");
    const escapedKeycode = escapeRegex(keycode);
    const pattern = new RegExp(`(^[ \\t]*MACRO\\(\\s*${escapedKeycode}\\s*,\\s*)"(?:\\\\.|[^"\\\\])*"(\\s*\\)(?:\\s*\\\\)?\\s*$)`, "m");
    const relative = block.text.replace(pattern, `$1${cStringLiteral(String(payload || ""))}$2`);
    if (relative === block.text) {
        throw new Error(`Could not find ${keycode} in VIA_MACROS(MACRO).`);
    }
    await writeText(filePath, replaceRange(text, block.start, block.end, relative));
}

async function appendCombo(root, target, output, inputs) {
    const filePath = profileTargetPaths(root, target).keymap;
    let text = await fs.readFile(filePath, "utf8");
    const {output: normalizedOutput, inputList} = normalizeComboRequest(text, output, inputs);

    text = removeAuthoredEmptyTableFlag(text, "NOAH_KEYMAP_EMPTY_COMBOS");
    const block = findMacroDefinitionBlock(text, "COMBOS");
    const lines = block.text.split(/\r?\n/);
    const insertLine = lines.findIndex((line, index) => index > 0 && line.includes("/* COMBO("));
    const insertionIndex = insertLine === -1 ? Math.max(lines.length - 1, 1) : insertLine;
    const row = renderComboMacroRow(normalizedOutput, inputList);
    lines.splice(insertionIndex, 0, row);
    await writeText(filePath, replaceRange(text, block.start, block.end, lines.join("\n")));
}

async function saveCombo(root, target, originalOutput, originalInputs, output, inputs) {
    const filePath = profileTargetPaths(root, target).keymap;
    const text = await fs.readFile(filePath, "utf8");
    const next = normalizeComboRequest(text, output, inputs);
    const original = normalizeComboIdentity(originalOutput, originalInputs);
    const block = findMacroDefinitionBlock(text, "COMBOS");
    const lines = block.text.split(/\r?\n/);
    const index = lines.findIndex((line) => {
        const row = parseComboInvocationLine(line);
        return row && row.output === original.output && comboInputSignature(row.inputs) === comboInputSignature(original.inputList);
    });
    if (index === -1) {
        throw new Error("Could not find the original combo row to update.");
    }

    lines[index] = renderComboMacroRow(next.output, next.inputList);
    await writeText(filePath, replaceRange(text, block.start, block.end, lines.join("\n")));
}

function normalizeComboRequest(text, output, inputs) {
    const knownTokens = knownLayoutKeyTokens(text);
    const normalizedOutput = normalizeUserKeyExpression(output || "");
    const inputList = normalizeComboInputList(inputs);
    assertSafeExpression(normalizedOutput, "combo output");
    assertLayoutKeyExpression(normalizedOutput, "combo output", knownTokens);
    if (inputList.length < 2) {
        throw new Error("Combo inputs must include at least two keycodes.");
    }
    for (const input of inputList) {
        assertSafeExpression(input, "combo input");
        assertLayoutKeyExpression(input, "combo input", knownTokens);
    }
    const duplicateInput = duplicateComboInput(inputList);
    if (duplicateInput) {
        throw new Error(`Combo inputs must be unique; ${duplicateInput} appears more than once.`);
    }
    return {output: normalizedOutput, inputList};
}

function normalizeComboIdentity(output, inputs) {
    return {
        output: normalizeUserKeyExpression(output || ""),
        inputList: normalizeComboInputList(inputs),
    };
}

function normalizeComboInputList(inputs) {
    return splitTopLevel(String(inputs || ""))
        .map(normalizeUserKeyExpression)
        .filter(Boolean);
}

function renderComboMacroRow(output, inputList) {
    return `    COMBO(${output}, (${inputList.join(", ")}))                          \\`;
}

function parseComboInvocationLine(rawLine) {
    const line = stripInlineLineComment(rawLine).trim();
    if (!line.startsWith("COMBO(")) {
        return undefined;
    }
    const open = line.indexOf("(");
    const close = findMatching(line, open, "(", ")");
    const row = splitTopLevel(line.slice(open + 1, close)).map(normalizeExpr);
    const inputs = (row[1] || "").replace(/^\(/, "").replace(/\)$/, "");
    return {
        output: normalizeExpr(row[0] || ""),
        inputs: splitTopLevel(inputs).map(normalizeExpr).filter(Boolean),
    };
}

function comboInputSignature(inputs) {
    return (inputs || []).map(normalizeExpr).filter(Boolean).sort().join("\u0000");
}

function duplicateComboInput(inputs) {
    const seen = new Set();
    for (const input of inputs || []) {
        const normalized = normalizeExpr(input);
        if (!normalized) continue;
        if (seen.has(normalized)) return normalized;
        seen.add(normalized);
    }
    return "";
}

function removeAuthoredEmptyTableFlag(text, macro) {
    const pattern = new RegExp(`^[ \\t]*#\\s*define\\s+${escapeRegex(macro)}\\b[^\\r\\n]*(?:\\r?\\n)?`, "m");
    return text.replace(pattern, "");
}

async function appendKeyBehavior(root, target, behavior) {
    const row = renderBehaviorRowFromRequest(behavior);
    const filePath = profileTargetPaths(root, target).keymap;
    let text = await fs.readFile(filePath, "utf8");
    text = removeAuthoredEmptyTableFlag(text, "NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS");
    await writeText(filePath, appendKeyBehaviorRow(text, row));
}

async function saveKeyBehavior(root, target, behavior) {
    const normalizedKeycode = normalizeUserKeyExpression(behavior?.keycode || "");
    assertSafeExpression(normalizedKeycode, "behavior keycode");

    const row = renderBehaviorRowFromRequest({ ...behavior, keycode: normalizedKeycode });
    const filePath = profileTargetPaths(root, target).keymap;
    let text = await fs.readFile(filePath, "utf8");
    text = removeAuthoredEmptyTableFlag(text, "NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS");
    const initializer = findInitializerBody(text, /key_behaviors\s*\[\]\s*=/);
    const qmkKeycodeCatalog = await loadQmkKeycodeCatalog(root).catch(() => fallbackQmkKeycodeCatalog());
    const existing = findKeyBehaviorEntry(initializer.body, normalizedKeycode, qmkKeycodeCatalog.aliases || {});

    if (existing) {
        const range = keyBehaviorEntryReplacementRange(text, initializer, existing);
        await writeText(filePath, replaceRange(text, range.start, range.end, row));
        return;
    }

    await writeText(filePath, appendKeyBehaviorRow(text, row));
}

function appendKeyBehaviorRow(text, row) {
    const initializer = findInitializerBody(text, /key_behaviors\s*\[\]\s*=/);
    const entries = splitTopLevelWithRanges(initializer.body);
    if (entries.length === 1 && isEmptyKeyBehaviorInitializerEntry(initializer.body.slice(entries[0].start, entries[0].end))) {
        const absoluteStart = initializer.bodyStart + entries[0].start;
        const start = text.lastIndexOf("\n", absoluteStart) + 1;
        let end = initializer.bodyStart + entries[0].end;
        while (end < text.length && /[ \t]/.test(text[end])) end += 1;
        if (text[end] === ",") end += 1;
        if (text[end] === "\r" && text[end + 1] === "\n") end += 2;
        else if (text[end] === "\n") end += 1;
        return replaceRange(text, start, end, `${row}\n`);
    }
    return replaceRange(text, initializer.bodyEnd, initializer.bodyEnd, `\n${row}\n`);
}

function isEmptyKeyBehaviorInitializerEntry(value) {
    return /^\{\s*0\s*\}$/.test(stripComments(value).trim());
}

function keyBehaviorEntryReplacementRange(text, initializer, entry) {
    const absoluteEntryStart = initializer.bodyStart + entry.start;
    let start = text.lastIndexOf("\n", absoluteEntryStart) + 1;
    let end = initializer.bodyStart + entry.end;

    while (end < text.length && /[ \t]/.test(text[end])) end += 1;
    if (text[end] === ",") end += 1;

    return {start, end};
}

function findKeyBehaviorEntry(body, keycode, keyAliases = {}) {
    const canonicalKeycode = canonicalKeyExpression(keycode, keyAliases);
    for (const entry of splitTopLevelWithRanges(body)) {
        const text = body.slice(entry.start, entry.end).trim();
        const inner = trimOuterInitializer(text);
        if (!inner) {
            continue;
        }
        const fields = parseDesignatedFields(inner);
        if (canonicalKeyExpression(fields[".keycode"] || "", keyAliases) === canonicalKeycode) {
            return entry;
        }
    }
    return undefined;
}

function renderBehaviorRowFromRequest(behavior) {
    const normalizedKeycode = normalizeUserKeyExpression(behavior?.keycode || "");
    assertSafeExpression(normalizedKeycode, "behavior keycode");

    const tapHoldTerm = normalizeOptionalTerm(behavior?.tapHoldTerm, "tap_hold_term");
    const longerHoldTerm = normalizeOptionalTerm(behavior?.longerHoldTerm, "longer_hold_term");
    const multiTapTerm = normalizeOptionalTerm(behavior?.multiTapTerm, "multi_tap_term");
    const skipRgbBranchConfirm = normalizeOptionalBool(behavior?.skipRgbBranchConfirm);
    const rgbBranchConfirmTerm = skipRgbBranchConfirm ? "" : normalizeOptionalTerm(behavior?.rgbBranchConfirmTerm, "rgb_branch_confirm_term");

    let stepRequests = Array.isArray(behavior?.steps) ? behavior.steps : [];
    if (stepRequests.length === 0) {
        stepRequests = [
            {
                tapCount: 0,
                tap: behavior?.tap,
                hold: behavior?.hold,
                longHold: behavior?.longHold,
            },
        ];
    }

    const steps = [];
    for (const step of stepRequests) {
        const tapCount = Number(step?.tapCount);
        if (!Number.isInteger(tapCount) || tapCount < 0 || tapCount >= TAP_COUNT_NAMES.length) {
            throw new Error(`Invalid tap count: ${step?.tapCount}`);
        }
        const rendered = buildBehaviorStep(tapCount, step?.tap, step?.hold, step?.longHold);
        if (rendered) {
            steps.push(rendered);
        }
    }

    if (steps.length === 0) {
        throw new Error("Add at least one tap, hold, or long-hold action.");
    }

    return renderKeyBehaviorRow(normalizedKeycode, { tapHoldTerm, longerHoldTerm, multiTapTerm, rgbBranchConfirmTerm, skipRgbBranchConfirm }, steps);
}

function normalizeOptionalTerm(value, label) {
    const term = normalizeExpr(value || "");
    if (!term) {
        return "";
    }
    if (keyBehaviorTimingInRange(term, false)) {
        return term;
    }
    throw new Error(`${label} must be a positive integer from 1 to ${KEY_BEHAVIOR_TIMING_MAX_MS} ms when provided.`);
}

function normalizeOptionalBool(value) {
    return value === true || ["true", "1"].includes(normalizeExpr(value || ""));
}

function keyBehaviorTimingInRange(value, allowZero) {
    const normalized = normalizeExpr(value || "");
    return /^\d+$/.test(normalized) && keyBehaviorTimingNumberInRange(normalized, allowZero);
}

function keyBehaviorTimingNumberInRange(value, allowZero) {
    if (!/^\d+$/.test(value || "")) {
        return false;
    }
    const normalized = String(value).replace(/^0+(?=\d)/, "");
    if (!allowZero && normalized === "0") {
        return false;
    }
    const max = String(KEY_BEHAVIOR_TIMING_MAX_MS);
    return normalized.length < max.length || (normalized.length === max.length && normalized <= max);
}

function buildBehaviorStep(tapCount, tap, hold, longHold) {
    const fields = [];
    const tapExpr = buildActionExpression(tap, TAP_HELPERS, "tap");
    const holdExpr = buildActionExpression(hold, HOLD_HELPERS, "hold");
    const longHoldExpr = buildActionExpression(longHold, HOLD_HELPERS, "long hold");
    if (tapExpr) {
        fields.push(`.tap = ${tapExpr}`);
    }
    if (holdExpr) {
        fields.push(`.hold = ${holdExpr}`);
    }
    if (longHoldExpr) {
        fields.push(`.long_hold = ${longHoldExpr}`);
    }
    if (fields.length === 0) {
        return undefined;
    }
    return `[${tapCount}] = {${fields.join(", ")}}`;
}

function buildActionExpression(action, allowedHelpers, label) {
    const helper = normalizeExpr(action?.helper || "");
    const target = normalizeUserKeyExpression(action?.action || "");
    const repeatHz = normalizeExpr(action?.repeatHz || "");
    if (!helper && !target) {
        return "";
    }
    assertAllowed(helper, allowedHelpers.filter(Boolean), `${label} helper`);
    assertSafeExpression(target, `${label} action`);
    if (helper === "REPEAT_WHILE_HELD") {
        if (!/^\d+$/.test(repeatHz)) {
            throw new Error(`${label} repeat Hz must be a positive integer.`);
        }
        return `${helper}(${target}, ${repeatHz})`;
    }
    return `${helper}(${target})`;
}

function renderKeyBehaviorRow(keycode, timings, steps) {
    const timingFields = [];
    if (timings.tapHoldTerm) timingFields.push(`                .tap_hold_term = ${timings.tapHoldTerm},`);
    if (timings.longerHoldTerm) timingFields.push(`                .longer_hold_term = ${timings.longerHoldTerm},`);
    if (timings.multiTapTerm) timingFields.push(`                .multi_tap_term = ${timings.multiTapTerm},`);
    if (timings.rgbBranchConfirmTerm) timingFields.push(`                .rgb_branch_confirm_term = ${timings.rgbBranchConfirmTerm},`);
    if (timings.skipRgbBranchConfirm) timingFields.push("                .skip_rgb_branch_confirm = true,");
    const timing = timingFields.length ? `\n${timingFields.join("\n")}` : "";
    return `            {
                .keycode = ${keycode},${timing}
                .tap_counts =
                    {
                        ${steps.join(",\n                        ")},
                    },
            },`;
}

function findLayerLayoutCall(arrayBody, layer) {
    const pattern = new RegExp(`\\[${escapeRegex(layer)}\\]\\s*=\\s*LAYOUT\\s*\\(`, "g");
    const match = pattern.exec(arrayBody);
    if (!match) {
        throw new Error(`Could not find ${layer} LAYOUT(...) block.`);
    }
    const open = arrayBody.indexOf("(", match.index);
    const close = findMatching(arrayBody, open, "(", ")");
    return {
        matchStart: match.index,
        argsStart: open + 1,
        argsEnd: close,
    };
}

function findMacroDefinitionBlock(text, macroName) {
    const pattern = new RegExp(`#define\\s+${escapeRegex(macroName)}\\s*\\(`);
    const match = pattern.exec(text);
    if (!match) {
        throw new Error(`Could not find #define ${macroName}(...).`);
    }

    const start = text.lastIndexOf("\n", match.index) + 1;
    let cursor = start;
    while (cursor < text.length) {
        const lineEnd = text.indexOf("\n", cursor);
        const end = lineEnd === -1 ? text.length : lineEnd + 1;
        const line = text.slice(cursor, lineEnd === -1 ? text.length : lineEnd);
        cursor = end;
        if (!line.trimEnd().endsWith("\\")) {
            break;
        }
    }

    return {
        start,
        end: cursor,
        text: text.slice(start, cursor),
    };
}

function findInitializerBody(text, pattern) {
    pattern.lastIndex = 0;
    const match = pattern.exec(text);
    if (!match) {
        throw new Error(`Could not find initializer for ${pattern}.`);
    }

    const open = text.indexOf("{", match.index + match[0].length);
    if (open === -1) {
        throw new Error(`Could not find initializer body for ${pattern}.`);
    }
    const close = findMatching(text, open, "{", "}");
    return {
        bodyStart: open + 1,
        bodyEnd: close,
        body: text.slice(open + 1, close),
    };
}

function findEnumBody(text, pattern) {
    pattern.lastIndex = 0;
    const match = pattern.exec(text);
    if (!match) {
        throw new Error(`Could not find enum for ${pattern}.`);
    }
    const open = text.indexOf("{", match.index + match[0].length - 1);
    if (open === -1) {
        throw new Error(`Could not find enum body for ${pattern}.`);
    }
    const close = findMatching(text, open, "{", "}");
    return {
        bodyStart: open + 1,
        bodyEnd: close,
        body: text.slice(open + 1, close),
    };
}

function findCallBody(text, pattern) {
    return findCallRange(text, pattern).body;
}

function findCallRange(text, pattern) {
    pattern.lastIndex = 0;
    const match = pattern.exec(text);
    if (!match) {
        throw new Error(`Could not find call for ${pattern}.`);
    }

    const open = text.indexOf("(", match.index + match[0].length);
    if (open === -1) {
        throw new Error(`Could not find call body for ${pattern}.`);
    }
    const close = findMatching(text, open, "(", ")");
    return {
        bodyStart: open + 1,
        bodyEnd: close,
        body: text.slice(open + 1, close),
    };
}

function findDesignatedEntry(body, designator) {
    const pattern = new RegExp(`${escapeRegex(designator)}\\s*=\\s*\\{`, "g");
    const match = pattern.exec(body);
    if (!match) {
        throw new Error(`Could not find ${designator} entry.`);
    }
    const open = body.indexOf("{", match.index);
    const close = findMatching(body, open, "{", "}");
    return { start: match.index, end: close + 1 };
}

function findStructEntryByField(body, field, value) {
    const entries = splitTopLevelWithRanges(body);
    for (const entry of entries) {
        const text = body.slice(entry.start, entry.end).trim();
        const inner = trimOuterInitializer(text);
        if (!inner) {
            continue;
        }
        const fields = parseDesignatedFields(inner);
        if (normalizeExpr(fields[field] || "") === value) {
            return entry;
        }
    }
    throw new Error(`Could not find struct row where ${field} is ${value}.`);
}

function patchFieldExpressionInRange(text, start, end, field, replacement) {
    const slice = text.slice(start, end);
    const fieldPattern = new RegExp(`(${escapeRegex(field)}\\s*=\\s*)(HSV\\s*\\([^)]*\\)|[^,\\n}]+)`);
    const match = fieldPattern.exec(slice);
    if (!match) {
        throw new Error(`Could not patch ${field}.`);
    }
    const valueStart = start + match.index + match[1].length;
    const valueEnd = valueStart + match[2].length;
    return replaceRange(text, valueStart, valueEnd, replacement);
}

function patchFieldInInitializer(text, initializerPattern, field, replacement) {
    const initializer = findInitializerBody(text, initializerPattern);
    return patchFieldExpressionInRange(text, initializer.bodyStart, initializer.bodyEnd, field, replacement);
}

function patchRgbTapBranchColorsInInitializer(text, initializerPattern, colors) {
    if (!colors.length) {
        return text;
    }

    const initializer = findInitializerBody(text, initializerPattern);
    const slice = text.slice(initializer.bodyStart, initializer.bodyEnd);
    const pattern = /RGB_TAP_BRANCH_COLORS\s*/;
    const match = pattern.exec(slice);
    if (!match) {
        throw new Error("Could not find RGB_TAP_BRANCH_COLORS(...).");
    }
    const open = slice.indexOf("(", match.index + match[0].length);
    if (open === -1) {
        throw new Error("Could not find RGB_TAP_BRANCH_COLORS(...) arguments.");
    }
    const close = findMatching(slice, open, "(", ")");
    const args = slice.slice(open + 1, close);
    const items = splitTopLevelWithRanges(args);
    if (items.length !== colors.length) {
        throw new Error(`Expected ${items.length} tap-count branch colors, got ${colors.length}.`);
    }

    let next = text;
    for (let index = items.length - 1; index >= 0; index -= 1) {
        const item = items[index];
        const itemText = args.slice(item.start, item.end);
        const hsvMatch = /HSV\s*\([^)]*\)/.exec(itemText);
        if (!hsvMatch) {
            throw new Error(`Could not patch tap count ${index + 2} branch color.`);
        }
        const absoluteStart = initializer.bodyStart + open + 1 + item.start + hsvMatch.index;
        const absoluteEnd = absoluteStart + hsvMatch[0].length;
        next = replaceRange(next, absoluteStart, absoluteEnd, colors[index].expression);
    }
    return next;
}

function parseDesignatedFields(body) {
    const fields = {};
    for (const rawItem of splitTopLevel(body)) {
        const item = stripComments(rawItem).trim();
        if (!item) {
            continue;
        }
        const eq = findTopLevelEquals(item);
        if (eq === -1) {
            continue;
        }
        const key = normalizeExpr(item.slice(0, eq));
        const value = item.slice(eq + 1).trim();
        if (key.startsWith(".")) {
            fields[key] = value;
        }
    }
    return fields;
}

function findTopLevelEquals(text) {
    let depthParen = 0;
    let depthBrace = 0;
    let depthBracket = 0;
    let quote = "";
    let escaped = false;
    let lineComment = false;
    let blockComment = false;

    for (let index = 0; index < text.length; index += 1) {
        const char = text[index];
        const next = text[index + 1];

        if (lineComment) {
            if (char === "\n") {
                lineComment = false;
            }
            continue;
        }
        if (blockComment) {
            if (char === "*" && next === "/") {
                blockComment = false;
                index += 1;
            }
            continue;
        }
        if (quote) {
            if (escaped) {
                escaped = false;
            } else if (char === "\\") {
                escaped = true;
            } else if (char === quote) {
                quote = "";
            }
            continue;
        }

        if (char === "/" && next === "/") {
            lineComment = true;
            index += 1;
            continue;
        }
        if (char === "/" && next === "*") {
            blockComment = true;
            index += 1;
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
            continue;
        }

        if (char === "(") depthParen += 1;
        else if (char === ")") depthParen -= 1;
        else if (char === "{") depthBrace += 1;
        else if (char === "}") depthBrace -= 1;
        else if (char === "[") depthBracket += 1;
        else if (
            char === "=" &&
            depthParen === 0 &&
            depthBrace === 0 &&
            depthBracket === 0
        ) {
            return index;
        }
    }
    return -1;
}

function hasTopLevelDelimiter(text, delimiter = ",") {
    let depthParen = 0;
    let depthBrace = 0;
    let depthBracket = 0;
    let quote = "";
    let escaped = false;
    let lineComment = false;
    let blockComment = false;

    for (let index = 0; index < text.length; index += 1) {
        const char = text[index];
        const next = text[index + 1];

        if (lineComment) {
            if (char === "\n") {
                lineComment = false;
            }
            continue;
        }
        if (blockComment) {
            if (char === "*" && next === "/") {
                blockComment = false;
                index += 1;
            }
            continue;
        }
        if (quote) {
            if (escaped) {
                escaped = false;
            } else if (char === "\\") {
                escaped = true;
            } else if (char === quote) {
                quote = "";
            }
            continue;
        }

        if (char === "/" && next === "/") {
            lineComment = true;
            index += 1;
            continue;
        }
        if (char === "/" && next === "*") {
            blockComment = true;
            index += 1;
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
            continue;
        }

        if (char === "(") depthParen += 1;
        else if (char === ")") depthParen -= 1;
        else if (char === "{") depthBrace += 1;
        else if (char === "}") depthBrace -= 1;
        else if (char === "[") depthBracket += 1;
        else if (char === "]") depthBracket -= 1;
        else if (
            char === delimiter &&
            depthParen === 0 &&
            depthBrace === 0 &&
            depthBracket === 0
        ) {
            return true;
        }
    }
    return false;
}

function splitTopLevel(text) {
    return splitTopLevelWithRanges(text).map((item) => text.slice(item.start, item.end));
}

function splitTopLevelWithRanges(text, delimiter = ",") {
    const items = [];
    let depthParen = 0;
    let depthBrace = 0;
    let depthBracket = 0;
    let start = 0;
    let quote = "";
    let escaped = false;
    let lineComment = false;
    let blockComment = false;

    for (let index = 0; index < text.length; index += 1) {
        const char = text[index];
        const next = text[index + 1];

        if (lineComment) {
            if (char === "\n") {
                lineComment = false;
            }
            continue;
        }

        if (blockComment) {
            if (char === "*" && next === "/") {
                blockComment = false;
                index += 1;
            }
            continue;
        }

        if (quote) {
            if (escaped) {
                escaped = false;
            } else if (char === "\\") {
                escaped = true;
            } else if (char === quote) {
                quote = "";
            }
            continue;
        }

        if (char === "/" && next === "/") {
            lineComment = true;
            index += 1;
            continue;
        }
        if (char === "/" && next === "*") {
            blockComment = true;
            index += 1;
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
            continue;
        }

        if (char === "(") depthParen += 1;
        else if (char === ")") depthParen -= 1;
        else if (char === "{") depthBrace += 1;
        else if (char === "}") depthBrace -= 1;
        else if (char === "[") depthBracket += 1;
        else if (char === "]") depthBracket -= 1;
        else if (
            char === delimiter &&
            depthParen === 0 &&
            depthBrace === 0 &&
            depthBracket === 0
        ) {
            items.push(trimRange(text, start, index));
            start = index + 1;
        }
    }

    items.push(trimRange(text, start, text.length));
    return items.filter((item) => item.start < item.end);
}

function trimRange(text, start, end) {
    while (start < end && /\s/.test(text[start])) start += 1;
    while (end > start && /\s/.test(text[end - 1])) end -= 1;
    return { start, end };
}

function trimCodeRange(text, start, end) {
    const masked = maskCommentsPreserveLength(text.slice(start, end));
    let relativeStart = 0;
    let relativeEnd = masked.length;
    while (relativeStart < relativeEnd && /\s/.test(masked[relativeStart])) relativeStart += 1;
    while (relativeEnd > relativeStart && /\s/.test(masked[relativeEnd - 1])) relativeEnd -= 1;
    return {
        start: start + relativeStart,
        end: start + relativeEnd,
    };
}

function maskCommentsPreserveLength(text) {
    let output = "";
    let lineComment = false;
    let blockComment = false;
    let quote = "";
    let escaped = false;

    for (let index = 0; index < text.length; index += 1) {
        const char = text[index];
        const next = text[index + 1];

        if (lineComment) {
            if (char === "\n") {
                lineComment = false;
                output += "\n";
            } else {
                output += " ";
            }
            continue;
        }

        if (blockComment) {
            if (char === "*" && next === "/") {
                output += "  ";
                blockComment = false;
                index += 1;
            } else {
                output += char === "\n" ? "\n" : " ";
            }
            continue;
        }

        if (quote) {
            output += char;
            if (escaped) {
                escaped = false;
            } else if (char === "\\") {
                escaped = true;
            } else if (char === quote) {
                quote = "";
            }
            continue;
        }

        if (char === "/" && next === "/") {
            output += "  ";
            lineComment = true;
            index += 1;
            continue;
        }
        if (char === "/" && next === "*") {
            output += "  ";
            blockComment = true;
            index += 1;
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
        }
        output += char;
    }

    return output;
}

function stripInlineLineComment(text) {
    let quote = "";
    let escaped = false;
    for (let index = 0; index < text.length - 1; index += 1) {
        const char = text[index];
        const next = text[index + 1];
        if (quote) {
            if (escaped) escaped = false;
            else if (char === "\\") escaped = true;
            else if (char === quote) quote = "";
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
            continue;
        }
        if (char === "/" && next === "/") {
            return text.slice(0, index);
        }
    }
    return text;
}

function stripComments(text) {
    return String(text || "")
        .replace(/\/\*[\s\S]*?\*\//g, "")
        .replace(/\/\/.*$/gm, "");
}

function findMatching(text, openIndex, openChar, closeChar) {
    let depth = 0;
    let quote = "";
    let escaped = false;
    let lineComment = false;
    let blockComment = false;

    for (let index = openIndex; index < text.length; index += 1) {
        const char = text[index];
        const next = text[index + 1];

        if (lineComment) {
            if (char === "\n") lineComment = false;
            continue;
        }
        if (blockComment) {
            if (char === "*" && next === "/") {
                blockComment = false;
                index += 1;
            }
            continue;
        }
        if (quote) {
            if (escaped) escaped = false;
            else if (char === "\\") escaped = true;
            else if (char === quote) quote = "";
            continue;
        }

        if (char === "/" && next === "/") {
            lineComment = true;
            index += 1;
            continue;
        }
        if (char === "/" && next === "*") {
            blockComment = true;
            index += 1;
            continue;
        }
        if (char === "\"" || char === "'") {
            quote = char;
            continue;
        }

        if (char === openChar) {
            depth += 1;
        } else if (char === closeChar) {
            depth -= 1;
            if (depth === 0) {
                return index;
            }
        }
    }

    throw new Error(`No matching ${closeChar} found.`);
}

function trimOuterInitializer(text) {
    const normalized = String(text || "").trim();
    const first = normalized.indexOf("{");
    const last = normalized.lastIndexOf("}");
    if (first === -1 || last === -1 || last <= first) {
        return "";
    }
    return normalized.slice(first + 1, last).trim();
}

function normalizeExpr(value) {
    return String(value || "").replace(/\s+/g, " ").replace(/\s*,\s*/g, ", ").trim();
}

function normalizeUserKeyExpression(value) {
    const raw = String(value || "").trim();
    if (!raw) {
        return "";
    }

    const compact = normalizeExpr(raw);
    const alias = USER_KEY_ALIASES[raw] || USER_KEY_ALIASES[raw.toLowerCase()] || USER_KEY_ALIASES[compact.toLowerCase()];
    if (alias) {
        return alias;
    }

    const chord = normalizeFriendlyChord(compact);
    if (chord) {
        return chord;
    }

    if (/^[a-zA-Z]$/.test(raw)) {
        return `KC_${raw.toUpperCase()}`;
    }

    if (/^\d$/.test(raw)) {
        return `KC_${raw}`;
    }

    if (/^[a-z][a-z0-9_]*$/i.test(compact) && compact.includes("_")) {
        return compact.toUpperCase();
    }

    return compact;
}

function authoredInternalKeyExpression(value) {
    const normalized = normalizeExpr(value);
    const alias = USER_KEY_ALIASES[normalized] || USER_KEY_ALIASES[normalized.toLowerCase()];
    return alias === "_______" || alias === "XXXXXXX" ? alias : "";
}

function normalizeFriendlyChord(value) {
    const parts = String(value || "")
        .split("+")
        .map((part) => part.trim())
        .filter(Boolean);
    if (parts.length < 2) {
        return "";
    }

    const key = normalizeUserKeyExpression(parts[parts.length - 1]);
    if (!key) {
        return "";
    }

    const modifiers = [];
    for (const part of parts.slice(0, -1)) {
        const modifier = normalizeFriendlyModifier(part);
        if (!modifier) {
            return "";
        }
        if (!modifiers.includes(modifier)) {
            modifiers.push(modifier);
        }
    }

    const wrapper = wrapperForModifiers(modifiers);
    if (!wrapper) {
        const wrappers = wrappersForModifierLabels(modifiers);
        if (wrappers.length !== modifiers.length) {
            return "";
        }
        return wrappers.reduceRight((expression, candidate) => `${candidate}(${expression})`, key);
    }
    return `${wrapper}(${key})`;
}

function normalizeFriendlyModifier(value) {
    const normalized = String(value || "").toLowerCase().replace(/[\s_-]+/g, "");
    return {
        ctrl: "Ctrl",
        control: "Ctrl",
        lctrl: "Ctrl",
        leftctrl: "Ctrl",
        shift: "Shift",
        lshift: "Shift",
        leftshift: "Shift",
        alt: "Alt",
        option: "Alt",
        lalt: "Alt",
        leftalt: "Alt",
        ralt: "Right Alt",
        rightalt: "Right Alt",
        cmd: "Cmd",
        command: "Cmd",
        gui: "Cmd",
        win: "Cmd",
        meta: "Cmd",
        lgui: "Cmd",
        leftgui: "Cmd",
        rctrl: "Right Ctrl",
        rightctrl: "Right Ctrl",
        rshift: "Right Shift",
        rightshift: "Right Shift",
        rgui: "Right Cmd",
        rightgui: "Right Cmd",
        rightcmd: "Right Cmd",
    }[normalized] || "";
}

function wrapperForModifiers(modifiers) {
    const key = modifiers.map(normalizeFriendlyModifier).filter(Boolean).sort().join("+");
    for (const [wrapper, labels] of Object.entries(MOD_WRAPPER_LABELS)) {
        const candidate = labels.map(normalizeFriendlyModifier).filter(Boolean).sort().join("+");
        if (candidate === key) {
            return wrapper;
        }
    }
    return "";
}

function wrappersForModifierLabels(modifiers) {
    return modifiers.map((modifier) => {
        switch (normalizeFriendlyModifier(modifier)) {
            case "Ctrl": return "C";
            case "Shift": return "S";
            case "Alt": return "A";
            case "Cmd": return "G";
            case "Right Ctrl": return "RCTL";
            case "Right Shift": return "RSFT";
            case "Right Alt": return "RALT";
            case "Right Cmd": return "RGUI";
            default: return "";
        }
    }).filter(Boolean);
}

function displayKeycode(keycode) {
    const normalized = normalizeExpr(keycode);
    return displayKeyExpression(normalized);
}

function editLabelForKeycode(keycode) {
    const normalized = normalizeExpr(keycode);
    const simple = QMK_KEY_LABELS[normalized];
    return simple || editableKeyExpression(normalized);
}

function displayKeyExpression(expression) {
    const normalized = normalizeExpr(expression);
    const internal = authoredInternalKeyExpression(normalized);
    if (internal) {
        return internal;
    }
    if (normalized === "_______") {
        return normalized;
    }
    if (QMK_KEY_LABELS[normalized]) {
        return QMK_KEY_LABELS[normalized];
    }

    let match = normalized.match(/^LT\(LAYER_([^,]+),\s*(.+)\)$/);
    if (match) {
        return `${displayKeyExpression(match[2])}, hold ${titleCase(match[1])}`;
    }

    match = normalized.match(/^MO\(LAYER_([^)]+)\)$/);
    if (match) {
        return `Hold ${titleCase(match[1])}`;
    }

    match = normalized.match(/^LOCK_LAYER\(LAYER_([^)]+)\)$/);
    if (match) {
        return `Lock ${titleCase(match[1])}`;
    }

    match = normalized.match(/^([A-Z][A-Z0-9_]*)\((.+)\)$/);
    if (match) {
        const modifiers = MOD_WRAPPER_LABELS[match[1]];
        if (modifiers) {
            return `${modifiers.join("+")}+${displayKeyExpression(match[2])}`;
        }
    }

    match = normalized.match(/^VIA_MACRO_(\d+)$/);
    if (match) {
        return `VIA Macro ${match[1]}`;
    }

    match = normalized.match(/^MACRO_(\d+)$/);
    if (match) {
        return `Macro ${match[1]}`;
    }

    if (normalized.endsWith("_MODE")) {
        return titleCase(normalized.replace(/_MODE$/, ""));
    }
    if (normalized.endsWith("_MODE_LOCK")) {
        return `${titleCase(normalized.replace(/_MODE_LOCK$/, ""))} Lock`;
    }
    if (normalized.endsWith("_LOCK")) {
        return `${titleCase(normalized.replace(/_LOCK$/, ""))} Lock`;
    }

    if (normalized.startsWith("KC_")) {
        return titleCase(normalized.slice(3));
    }

    return normalized;
}

function editableKeyExpression(expression) {
    const normalized = normalizeExpr(expression);
    const match = normalized.match(/^([A-Z][A-Z0-9_]*)\((.+)\)$/);
    if (match && MOD_WRAPPER_LABELS[match[1]]) {
        return displayKeyExpression(normalized);
    }
    return normalized;
}

function titleCase(value) {
    return String(value || "")
        .toLowerCase()
        .split("_")
        .filter(Boolean)
        .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
        .join(" ");
}

function parseCString(value) {
    const normalized = normalizeExpr(value);
    if (!normalized.startsWith("\"")) {
        return normalized;
    }
    try {
        return JSON.parse(normalized);
    } catch {
        return normalized.slice(1, -1);
    }
}

function cStringLiteral(value) {
    return JSON.stringify(String(value));
}

function replaceRange(text, start, end, replacement) {
    return text.slice(0, start) + replacement + text.slice(end);
}

async function writeText(filePath, text) {
    await fs.writeFile(filePath, text);
}

function assertSafeIdentifier(value, label) {
    if (!/^[A-Z_][A-Z0-9_]*$/.test(String(value || ""))) {
        throw new Error(`Invalid ${label}: ${value}`);
    }
}

function assertSafeExpression(value, label) {
    const normalized = normalizeExpr(value);
    if (!normalized || /[;"{}#\n\r]/.test(normalized)) {
        throw new Error(`Invalid ${label}: ${value}`);
    }
}

function assertLayoutKeyExpression(value, label, knownTokens = new Set()) {
    const normalized = normalizeExpr(value);
    if (hasTopLevelDelimiter(normalized, ",")) {
        throw new Error(`Invalid ${label}: expected one keycode expression, got a comma-separated list.`);
    }
    if (!isLayoutKeyExpression(normalized, knownTokens)) {
        throw new Error(`Invalid ${label}: expected a keycode, alias, or QMK key expression.`);
    }
}

function isLayoutKeyExpression(value, knownTokens = new Set()) {
    if (value === "_______" || value === "XXXXXXX") {
        return true;
    }
    if (/^[A-Z_][A-Z0-9_]*$/.test(value)) {
        return knownTokens.has(value) || value.includes("_");
    }
    return isLayoutKeyCallExpression(value, knownTokens);
}

function isLayoutKeyCallExpression(value, knownTokens = new Set()) {
    const match = String(value || "").match(/^([A-Z][A-Z0-9_]*)\s*\(/);
    if (!match) {
        return false;
    }
    const open = value.indexOf("(", match[1].length);
    const close = findMatching(value, open, "(", ")");
    if (close !== value.length - 1) {
        return false;
    }
    const name = match[1];
    return knownTokens.has(name) || LAYOUT_KEY_CALL_FUNCTIONS.includes(name) || Boolean(MOD_WRAPPER_LABELS[name]) || name.includes("_");
}

function assertSafeHsv(hue, sat, val) {
    assertUint8Channel(hue, "HSV hue");
    assertUint8Channel(sat, "HSV saturation");
    const value = normalizeExpr(val);
    if (/^\d+$/.test(value)) {
        assertUint8Channel(value, "HSV value");
        return;
    }
    if (!/^[A-Z_][A-Z0-9_]*(?:\s*[-+*/]\s*(?:\d+|[A-Z_][A-Z0-9_]*))*$/.test(value)) {
        throw new Error(`HSV value must be an integer from 0 to 255 or a safe constant expression: ${val}`);
    }
}

function assertUint8Channel(value, label) {
    const text = normalizeExpr(value);
    if (!/^\d+$/.test(text)) {
        throw new Error(`${label} must be an integer from 0 to 255: ${value}`);
    }
    const number = Number(text);
    if (!Number.isInteger(number) || number < 0 || number > 255) {
        throw new Error(`${label} must be an integer from 0 to 255: ${value}`);
    }
}

function assertAllowed(value, allowed, label) {
    if (!allowed.includes(value)) {
        throw new Error(`Invalid ${label}: ${value}`);
    }
}

function escapeRegex(value) {
    return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function getStudioHtml() {
    const nonce = getNonce();
    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'nonce-${nonce}' 'unsafe-inline'; script-src 'nonce-${nonce}';">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Charybdis Profile Studio</title>
    <style nonce="${nonce}">
        :root {
            --bg: #1f2428;
            --panel: #282f34;
            --panel-2: #323a40;
            --text: #e7ecef;
            --muted: #a8b2b8;
            --line: #46525a;
            --accent: #31c6a4;
            --warn: #f2b84b;
            --danger: #ff6b6b;
            --key: #3a444b;
            --key-active: #245d55;
        }
        * { box-sizing: border-box; }
        [hidden] {
            display: none !important;
        }
        body {
            margin: 0;
            background: var(--bg);
            color: var(--text);
            font: 13px/1.4 var(--vscode-font-family, system-ui, sans-serif);
        }
        header {
            position: sticky;
            top: 0;
            z-index: 2;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
            padding: 14px 18px;
            border-bottom: 1px solid var(--line);
            background: #20262a;
        }
        .header-title {
            min-width: 0;
        }
        .header-actions {
            display: grid;
            gap: 7px;
            justify-items: end;
            min-width: min(900px, 100%);
        }
        .header-action-row {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            align-items: center;
            justify-content: flex-end;
        }
        .profile-picker-label {
            color: var(--muted);
            font-size: 11px;
            font-weight: 650;
        }
        .profile-picker select {
            flex: 0 1 320px;
            min-width: 220px;
            width: 320px;
            max-width: 42vw;
        }
        .header-button-group {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            align-items: center;
            justify-content: flex-end;
        }
        h1, h2, h3 { margin: 0; font-weight: 650; }
        h1 { font-size: 18px; }
        h2 { font-size: 15px; margin-bottom: 10px; }
        h3 { font-size: 13px; margin-bottom: 8px; color: var(--muted); }
        button, input, select, textarea {
            border: 1px solid var(--line);
            border-radius: 6px;
            background: #20262a;
            color: var(--text);
            font: inherit;
        }
        button {
            cursor: pointer;
            padding: 7px 10px;
        }
        button.primary {
            border-color: #2aa88e;
            background: #217a6a;
        }
        button.dirty {
            border-color: var(--warn);
            background: #7a5b1f;
            color: #fff4d2;
            box-shadow: 0 0 0 1px rgba(242, 184, 75, 0.28);
        }
        button:disabled {
            border-color: rgba(168, 178, 184, 0.26);
            background: rgba(32, 38, 42, 0.58);
            color: rgba(168, 178, 184, 0.58);
            box-shadow: none;
            cursor: not-allowed;
        }
        button:not(:disabled):hover { border-color: var(--accent); }
        input, select, textarea {
            min-height: 32px;
            padding: 5px 8px;
            width: 100%;
        }
        textarea {
            min-height: 96px;
            resize: vertical;
            line-height: 1.45;
        }
        input:not(:disabled), select:not(:disabled), textarea:not(:disabled) {
            border-color: #60707a;
            background: #20262a;
            box-shadow: inset 0 0 0 1px rgba(49, 198, 164, 0.08);
        }
        input.invalid, select.invalid, textarea.invalid {
            border-color: var(--danger);
            box-shadow: inset 0 0 0 1px rgba(255, 107, 107, 0.36), 0 0 0 1px rgba(255, 107, 107, 0.24);
        }
        input:disabled, select:disabled, textarea:disabled {
            border-style: dashed;
            border-color: rgba(168, 178, 184, 0.38);
            background: rgba(32, 38, 42, 0.46);
            color: rgba(168, 178, 184, 0.72);
            cursor: not-allowed;
            opacity: 1;
        }
        input:disabled::selection {
            background: transparent;
        }
        textarea.monospace {
            font-family: var(--vscode-editor-font-family, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace);
        }
        main {
            display: block;
            padding: 14px;
        }
        section, details.panel {
            border: 1px solid var(--line);
            border-radius: 8px;
            background: var(--panel);
            padding: 14px;
            min-width: 0;
        }
        details.panel > summary {
            cursor: pointer;
            font-size: 15px;
            font-weight: 650;
            list-style-position: inside;
        }
        details.panel > summary h2,
        details.panel > summary h3 {
            display: inline;
            margin-left: 4px;
        }
        details.panel > .panel-body {
            margin-top: 12px;
        }
        section.panel > h2 {
            margin: 0 0 12px;
        }
        .panel > .panel-body {
            min-width: 0;
        }
        .status-popup {
            position: fixed;
            top: 66px;
            right: 18px;
            z-index: 5;
            display: grid;
            gap: 8px;
            width: min(520px, calc(100vw - 36px));
            max-height: min(360px, calc(100vh - 92px));
            overflow: auto;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: #20262a;
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.36);
            padding: 12px;
        }
        .status-popup-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 10px;
        }
        .status-popup-title {
            color: var(--muted);
            font-size: 12px;
            font-weight: 650;
        }
        .status-popup-dismiss {
            position: relative;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 28px;
            height: 28px;
            min-width: 28px;
            padding: 0;
            color: var(--muted);
            line-height: 1;
        }
        .status-popup-dismiss::before,
        .status-popup-dismiss::after {
            content: "";
            position: absolute;
            width: 12px;
            height: 2px;
            border-radius: 999px;
            background: currentColor;
        }
        .status-popup-dismiss::before {
            transform: rotate(45deg);
        }
        .status-popup-dismiss::after {
            transform: rotate(-45deg);
        }
        .status-popup-dismiss:hover {
            color: var(--text);
            border-color: var(--accent);
        }
        .status-popup-body {
            display: grid;
            gap: 6px;
        }
        .modal-backdrop {
            position: fixed;
            inset: 0;
            z-index: 10;
            display: grid;
            place-items: center;
            padding: 18px;
            background: rgba(9, 12, 14, 0.72);
        }
        .confirm-dialog {
            display: grid;
            gap: 14px;
            width: min(560px, 100%);
            max-height: min(680px, calc(100vh - 36px));
            overflow: auto;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: var(--panel);
            box-shadow: 0 18px 48px rgba(0, 0, 0, 0.46);
            padding: 16px;
        }
        .confirm-dialog h2 {
            margin: 0;
        }
        .confirm-dialog p {
            margin: 0;
            color: var(--muted);
        }
        .confirm-list {
            display: grid;
            gap: 8px;
            margin: 0;
            padding-left: 18px;
        }
        .confirm-list li {
            padding-left: 2px;
        }
        .confirm-actions {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            justify-content: flex-end;
        }
        .stack { display: grid; gap: 14px; }
        .view-tabs {
            display: flex;
            flex-wrap: nowrap;
            gap: 0;
            align-items: flex-end;
            margin: 12px 0 14px;
            border-bottom: 1px solid var(--line);
            overflow-x: auto;
            overflow-y: hidden;
            scrollbar-width: none;
        }
        .view-tabs::-webkit-scrollbar {
            display: none;
        }
        .view-tab {
            min-width: 112px;
            border: 1px solid transparent;
            border-bottom: 0;
            border-radius: 7px 7px 0 0;
            background: transparent;
            color: var(--muted);
            padding: 8px 16px 9px;
            text-align: center;
            white-space: nowrap;
            position: relative;
        }
        .view-tab.active {
            border-color: var(--accent);
            border-bottom-color: #1f5d52;
            background: #1f5d52;
            color: var(--text);
            box-shadow: inset 0 -3px 0 var(--accent);
            font-weight: 700;
            margin-bottom: -1px;
            z-index: 1;
        }
        .view-tab.dirty {
            color: #fff4d2;
        }
        .view-tab.dirty::after {
            content: "";
            position: absolute;
            top: 7px;
            right: 10px;
            width: 7px;
            height: 7px;
            border-radius: 999px;
            background: var(--warn);
            box-shadow: 0 0 0 2px var(--bg);
        }
        .view-tab.dirty:not(.active):hover {
            border-color: rgba(242, 184, 75, 0.7);
        }
        .view-tab.active.dirty {
            border-color: var(--accent);
            border-bottom-color: #1f5d52;
            color: var(--text);
            box-shadow: inset 0 -3px 0 var(--accent);
        }
        .view-tab.active.dirty::after {
            box-shadow: 0 0 0 2px #1f5d52;
        }
        .tabs {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            margin-bottom: 12px;
        }
        .tab.active {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .layer-tabs {
            align-items: center;
            margin-bottom: 8px;
        }
        .layer-tab.pending-add {
            border-color: var(--warn);
            color: #fff4d2;
        }
        .layer-tab.pending-delete {
            border-color: var(--danger);
            color: #ffd8d8;
            text-decoration: line-through;
        }
        .layer-tab.dirty {
            border-color: var(--warn);
            color: #fff4d2;
            box-shadow: inset 0 2px 0 var(--warn);
        }
        .layer-tab.dirty:not(.active) {
            background: rgba(122, 91, 31, 0.42);
        }
        .layer-tab.active.dirty {
            background: #6d501e;
        }
        .layer-tab-action {
            min-width: 34px;
            padding-inline: 10px;
            font-weight: 700;
        }
        .layer-flow-card {
            display: grid;
            gap: 10px;
            margin: 0 0 12px;
            padding: 10px;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: rgba(23, 28, 32, 0.34);
        }
        .layer-flow-row {
            display: grid;
            grid-template-columns: minmax(220px, 1fr) repeat(2, minmax(110px, auto));
            gap: 8px;
            align-items: end;
        }
        .layer-flow-row label {
            grid-template-rows: auto auto;
        }
        .layer-flow-actions {
            display: grid;
            grid-template-columns: minmax(0, 1fr) minmax(120px, auto) minmax(180px, auto);
            gap: 8px;
            align-items: center;
        }
        .layer-flow-summary {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            min-width: 0;
        }
        .layer-flow-card .macro-chip.delete {
            border-color: rgba(255, 107, 107, 0.42);
            color: #ffd8d8;
        }
        @media (max-width: 860px) {
            .layer-flow-row,
            .layer-flow-actions {
                grid-template-columns: 1fr;
            }
            header {
                align-items: stretch;
                flex-direction: column;
            }
            .header-actions,
            .toolbar,
            .profile-picker,
            .header-action-row {
                justify-content: stretch;
                width: 100%;
            }
            .profile-picker select,
            .header-button-group,
            .header-action-row button {
                width: 100%;
            }
        }
        .board {
            overflow-x: auto;
            display: grid;
            min-width: 0;
        }
        .rgb-builder-card {
            display: grid;
            gap: 12px;
        }
        .rgb-builder-workspace {
            display: grid;
            gap: 14px;
        }
        .rgb-builder-subsection-title {
            margin: 0;
            font-size: 14px;
            line-height: 1.25;
        }
        .rgb-builder-sidecar {
            align-content: stretch;
        }
        .layout-with-key-editor.rgb-builder-layout {
            margin-bottom: 0;
        }
        .rgb-builder-sidecar .form-grid.four {
            grid-template-columns: 1fr;
        }
        .rgb-builder-sidecar button.primary {
            width: 100%;
        }
        .rgb-builder-sidecar .toolbar {
            align-items: stretch;
        }
        .rgb-builder-sidecar .rgb-selected-list {
            align-items: flex-start;
        }
        .layout-with-key-editor {
            display: grid;
            grid-template-columns: minmax(760px, 1fr) minmax(420px, 520px);
            gap: 14px;
            align-items: stretch;
            margin-bottom: 14px;
        }
        .layout-selected-key-column {
            display: grid;
            align-content: stretch;
            justify-items: stretch;
            gap: 12px;
            min-height: 100%;
        }
        .layout-sidecar-stack {
            display: grid;
            align-content: start;
            gap: 12px;
            width: 100%;
        }
        .selected-key-edit-card {
            display: grid;
            gap: 12px;
            width: 100%;
        }
        .selected-key-edit-card h3 {
            margin: 0;
        }
        .selected-key-edit-fields {
            display: grid;
            gap: 12px;
        }
        .selected-key-edit-card button.primary {
            width: 100%;
        }
        .keyboard-svg {
            display: block;
            min-width: 760px;
            max-width: 1120px;
            width: 100%;
            height: auto;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: #2f3336;
        }
        .layout-board-card {
            position: relative;
            grid-template-rows: auto minmax(0, 1fr);
            min-height: 620px;
            height: 100%;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: #2f3336;
        }
        .layout-board-header {
            padding: 22px 64px 0 32px;
        }
        .layout-board-title {
            margin: 0;
            color: #dbe6e8;
            font-size: 24px;
            font-weight: 650;
            line-height: 1.2;
        }
        .layout-board-subtitle {
            margin: 8px 0 0;
            color: #a8b2b8;
            font-size: 13px;
        }
        .layout-board-info {
            position: absolute;
            top: 18px;
            right: 22px;
            display: grid;
            place-items: center;
            width: 26px;
            height: 26px;
            border: 1px solid rgba(219, 230, 232, 0.62);
            border-radius: 999px;
            padding: 0;
            background: rgba(21, 26, 29, 0.42);
            color: #dbe6e8;
            font-size: 13px;
            font-weight: 750;
            line-height: 1;
        }
        .layout-board-info:hover,
        .layout-board-info:focus-visible {
            border-color: var(--accent);
            color: #ffffff;
            background: rgba(49, 198, 164, 0.16);
        }
        .layout-board-stage {
            display: grid;
            place-items: center;
            min-height: 0;
            padding: 18px 32px 34px;
        }
        .layout-board-svg {
            border: 0;
            border-radius: 0;
            background: transparent;
        }
        .layout-board-footer {
            position: absolute;
            left: 32px;
            right: 32px;
            bottom: 22px;
            z-index: 1;
            display: flex;
            flex-direction: column;
            align-items: flex-start;
            gap: 8px;
        }
        .layout-board-apply button {
            min-width: 190px;
        }
        .layout-board-notice {
            color: var(--warn);
            font-weight: 650;
        }
        .svg-key {
            cursor: pointer;
        }
        .layout-board-svg .svg-key {
            cursor: grab;
        }
        body.layout-key-dragging .layout-board-svg .svg-key {
            cursor: grabbing;
        }
        .layout-board-svg .svg-key.drag-source > :not(rect:first-of-type) {
            opacity: 0;
        }
        .layout-board-svg .svg-key.drag-source > rect:first-of-type {
            fill: rgba(32, 38, 42, 0.22);
            stroke: var(--warn);
            stroke-dasharray: 5 4;
            stroke-width: 2.4;
        }
        .layout-board-svg .svg-key.layout-drag-ghost {
            opacity: 0.96;
            pointer-events: none;
            filter: drop-shadow(0 10px 16px rgba(0, 0, 0, 0.42));
        }
        .layout-board-svg .svg-key.layout-drag-ghost rect:first-of-type {
            stroke: var(--accent);
            stroke-width: 3;
        }
        .layout-board-svg .svg-key.drag-target rect {
            stroke: var(--warn);
            stroke-width: 3;
        }
        .layout-board-svg .svg-key.pending rect {
            stroke: var(--warn);
            stroke-dasharray: 6 4;
            stroke-width: 3;
        }
        .svg-key rect {
            stroke-width: 1.4;
        }
        .svg-key.selected rect {
            stroke: var(--accent);
            stroke-width: 3;
        }
        .svg-key.combo-input-selected rect {
            stroke: var(--warn);
            stroke-width: 3;
        }
        .svg-key.rgb-selected rect {
            stroke: #ffffff;
            stroke-width: 3;
        }
        .svg-key.rgb-defined rect {
            stroke: #f0c857;
            stroke-width: 3;
        }
        .svg-key.rgb-selected.rgb-defined rect {
            stroke: #ffffff;
        }
        .svg-key.rgb-all-preview text,
        .extra-led.rgb-all-preview text {
            paint-order: stroke;
            stroke: rgba(0, 0, 0, 0.58);
            stroke-width: 3px;
            stroke-linejoin: round;
        }
        .svg-key text {
            font-family: var(--vscode-font-family, system-ui, sans-serif);
            text-anchor: middle;
            dominant-baseline: middle;
            pointer-events: none;
        }
        .svg-key .alternate-output-label {
            opacity: 0.56;
            font-weight: 650;
        }
        .svg-key .dual-role-separator-line {
            opacity: 0.42;
        }
        .svg-key .dual-role-hold-label {
            opacity: 0.78;
            font-weight: 700;
        }
        .extra-led {
            cursor: pointer;
        }
        .extra-led circle {
            stroke-width: 2;
        }
        .extra-led.rgb-selected circle {
            stroke: #ffffff;
            stroke-width: 3;
        }
        .extra-led.rgb-defined circle {
            stroke: #f0c857;
            stroke-width: 3;
        }
        .extra-led.rgb-selected.rgb-defined circle {
            stroke: #ffffff;
        }
        .source-pill {
            display: inline-block;
            max-width: 100%;
            padding: 4px 7px;
            border: 1px solid var(--line);
            border-radius: 999px;
            color: var(--muted);
            overflow-wrap: anywhere;
        }
        .layout-combo-actions {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 8px;
        }
        .layout-combo-actions button.active {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .layout-combo-actions .combo-pick-toggle:not(.active) {
            border-color: #d45b5b;
            background: #8a3030;
            color: #fff0f0;
        }
        .layout-combo-selected-list {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            min-height: 28px;
            align-items: center;
        }
        .layout-combo-selected-list button {
            display: inline-flex;
            align-items: center;
            gap: 5px;
            max-width: 100%;
            padding: 4px 7px;
            font-size: 11px;
        }
        .macro-builder-grid {
            display: grid;
            grid-template-columns: minmax(210px, 260px) minmax(0, 1fr);
            gap: 14px;
            align-items: start;
        }
        .macro-builder-summary {
            margin: 2px 0 14px;
            padding-bottom: 10px;
            border-bottom: 1px solid rgba(70, 82, 90, 0.52);
        }
        .macro-slot-browser {
            display: grid;
            grid-template-rows: auto minmax(0, 1fr);
            gap: 10px;
            min-width: 0;
            min-height: 0;
            overflow: hidden;
        }
        .macro-slot-browser h3 {
            margin-bottom: 0;
        }
        .macro-slot-list {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(96px, 1fr));
            align-content: start;
            gap: 6px;
            min-height: 0;
            overflow: auto;
            padding-right: 2px;
        }
        .macro-slot-button {
            display: grid;
            gap: 4px;
            min-height: 48px;
            padding: 7px 8px;
            text-align: left;
        }
        .macro-slot-button.active {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .macro-slot-button.empty {
            color: var(--muted);
        }
        .macro-slot-button.dirty {
            border-color: var(--warn);
            background: #584720;
        }
        .macro-slot-title {
            font-weight: 650;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .macro-slot-state {
            color: var(--muted);
            font-size: 11px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .macro-builder-main {
            display: grid;
            gap: 12px;
            min-width: 0;
        }
        .macro-editor-card,
        .macro-composer-card,
        .macro-recorder-card,
        .macro-preview-card {
            display: grid;
            gap: 12px;
        }
        .macro-tool-row {
            display: grid;
            grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
            gap: 12px;
            align-items: stretch;
        }
        .macro-builder-head {
            display: flex;
            align-items: start;
            justify-content: space-between;
            gap: 12px;
        }
        .macro-builder-head button {
            min-width: 132px;
        }
        .macro-payload-textarea {
            min-height: 132px;
        }
        .macro-stat-row {
            display: flex;
            flex-wrap: wrap;
            gap: 7px;
            align-items: center;
        }
        .macro-chip {
            display: inline-flex;
            align-items: center;
            gap: 5px;
            max-width: 100%;
            border: 1px solid var(--line);
            border-radius: 999px;
            padding: 3px 7px;
            color: var(--muted);
            font-size: 11px;
            overflow-wrap: anywhere;
        }
        .macro-chip strong {
            color: var(--text);
            font-size: 12px;
        }
        .macro-chip.warning {
            border-color: rgba(242, 184, 75, 0.58);
            color: var(--warn);
        }
        .macro-composer-grid {
            display: grid;
            grid-template-columns: minmax(150px, 220px) minmax(0, 1fr) minmax(220px, auto);
            gap: 10px;
            align-items: end;
        }
        .macro-step-fields {
            display: grid;
            grid-template-columns: 1fr;
            gap: 10px;
            align-items: end;
        }
        .macro-step-fields label {
            margin: 0;
        }
        .macro-composer-actions {
            display: flex;
            gap: 8px;
            justify-content: flex-end;
            align-items: end;
        }
        .macro-composer-actions button {
            min-width: 104px;
        }
        .macro-tool-row .macro-composer-grid {
            grid-template-columns: 1fr;
            align-items: stretch;
        }
        .macro-sidecar-state-spacer {
            visibility: hidden;
        }
        .macro-tool-row .macro-composer-actions {
            justify-content: stretch;
        }
        .macro-tool-row .macro-composer-actions button {
            flex: 1 1 0;
        }
        .macro-recorder-controls {
            display: grid;
            grid-template-columns: 1fr;
            gap: 10px;
            align-items: stretch;
            justify-content: stretch;
        }
        .macro-recorder-controls label {
            margin: 0;
        }
        .macro-recorder-primary-row {
            display: grid;
            grid-template-columns: minmax(0, 1fr) minmax(120px, 160px);
            gap: 10px;
            align-items: end;
        }
        .macro-recorder-primary-row label {
            margin: 0;
        }
        .macro-recorder-delay-fields {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 8px;
            align-items: end;
        }
        .macro-recorder-delay-fields label {
            margin: 0;
        }
        .macro-recorder-delay-fields.inactive {
            opacity: 0.44;
            pointer-events: none;
        }
        .macro-recorder-toggle {
            position: relative;
            display: grid;
            grid-template-rows: auto 38px 14px;
            align-items: start;
            justify-items: center;
            gap: 4px;
            min-height: 0;
            width: 100%;
            margin: 0;
            color: var(--muted);
        }
        .macro-recorder-toggle input[type='checkbox'] {
            position: absolute;
            width: 1px;
            height: 1px;
            min-height: 0;
            margin: 0;
            padding: 0;
            border: 0;
            clip-path: inset(50%);
            opacity: 0;
            pointer-events: none;
        }
        .macro-recorder-toggle-track {
            position: relative;
            align-self: center;
            width: 42px;
            height: 24px;
            border: 1px solid var(--line);
            border-radius: 999px;
            background: #20262a;
            box-shadow: inset 0 0 0 1px rgba(49, 198, 164, 0.08);
            transition: background 120ms ease, border-color 120ms ease;
        }
        .macro-recorder-toggle-track::after {
            content: "";
            position: absolute;
            top: 3px;
            left: 3px;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--muted);
            transition: transform 120ms ease, background 120ms ease;
        }
        .macro-recorder-toggle input[type='checkbox']:checked + .macro-recorder-toggle-track {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .macro-recorder-toggle input[type='checkbox']:checked + .macro-recorder-toggle-track::after {
            transform: translateX(18px);
            background: var(--text);
        }
        .macro-recorder-toggle input[type='checkbox']:focus-visible + .macro-recorder-toggle-track {
            outline: 1px solid var(--accent);
            outline-offset: 2px;
        }
        .macro-recorder-toggle-label {
            min-width: 0;
            text-align: center;
            white-space: nowrap;
            font-size: 11px;
        }
        .macro-recorder-actions {
            display: flex;
            justify-content: stretch;
            align-items: center;
            gap: 8px;
            justify-self: stretch;
            width: 100%;
        }
        .macro-recorder-actions button {
            flex: 1 1 0;
            min-height: 32px;
            min-width: 124px;
            padding-block: 5px;
        }
        .macro-recorder-actions button.primary {
            min-width: 170px;
        }
        .macro-recorder-actions button.record-action {
            border-color: #d45b5b;
            background: #8a3030;
            color: #fff0f0;
        }
        .macro-recorder-state {
            overflow-wrap: anywhere;
        }
        .macro-recorder-dot {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--danger);
            box-shadow: 0 0 0 4px rgba(217, 83, 79, 0.18);
        }
        .macro-preview-list {
            display: grid;
            gap: 6px;
        }
        .macro-preview-step {
            display: grid;
            grid-template-columns: 84px minmax(0, 1fr);
            gap: 8px;
            align-items: start;
            border: 1px solid rgba(70, 82, 90, 0.74);
            border-radius: 6px;
            padding: 7px 8px;
            background: #20282d;
        }
        .macro-preview-kind {
            color: var(--accent);
            font-size: 11px;
            font-weight: 650;
            text-transform: uppercase;
        }
        .macro-preview-detail {
            min-width: 0;
            overflow-wrap: anywhere;
        }
        .muted { color: var(--muted); }
        .notice { color: var(--accent); }
        .error { color: var(--danger); }
        .field-error {
            display: block;
            margin-top: 4px;
            min-height: 14px;
            color: var(--danger);
            font-size: 11px;
            line-height: 1.25;
        }
        .warning { color: var(--warn); }
        .form-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 10px;
            align-items: end;
        }
        .form-grid.three { grid-template-columns: repeat(3, minmax(0, 1fr)); }
        .form-grid.four { grid-template-columns: repeat(4, minmax(0, 1fr)); }
        label {
            display: grid;
            grid-template-rows: auto auto 14px;
            gap: 4px;
            min-width: 0;
            color: var(--muted);
        }
        label span { font-size: 11px; }
        [data-tooltip] {
            cursor: help;
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            border-bottom: 1px solid var(--line);
            padding: 6px 5px;
            vertical-align: top;
            text-align: left;
        }
        th { color: var(--muted); font-weight: 600; }
        .table-cell-stack {
            display: inline-grid;
            justify-items: start;
            gap: 5px;
            max-width: 100%;
        }
        .table-cell-actions {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            margin-top: 5px;
        }
        code {
            color: #d7f9ef;
            overflow-wrap: anywhere;
        }
        .card-list {
            display: grid;
            gap: 10px;
        }
        .card {
            border: 1px solid var(--line);
            border-radius: 8px;
            padding: 10px;
            background: var(--panel-2);
        }
        .config-default-section {
            display: grid;
            gap: 12px;
        }
        .config-default-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 10px;
            align-items: stretch;
        }
        .config-default-field {
            display: grid;
            grid-template-rows: auto 1fr;
            gap: 9px;
            min-width: 0;
        }
        .config-default-field-head {
            display: grid;
            gap: 3px;
            min-width: 0;
        }
        .config-default-field-title {
            font-weight: 650;
        }
        .config-default-field-hint {
            color: var(--muted);
            font-size: 11px;
        }
        .config-default-field label {
            grid-template-rows: auto auto 14px;
        }
        .config-default-field .toggle-inline {
            align-self: end;
            margin-top: 20px;
        }
        .behavior-step > summary,
        .collapsible-card > summary {
            cursor: pointer;
            list-style-position: inside;
        }
        .behavior-step > summary h3,
        .collapsible-card > summary h3 {
            display: inline;
            margin-left: 4px;
        }
        .behavior-branch-grid {
            display: grid;
            grid-template-columns: repeat(5, minmax(340px, 1fr));
            gap: 14px;
            align-items: start;
            overflow-x: auto;
            padding-bottom: 8px;
            margin-top: 14px;
        }
        .behavior-step {
            min-width: 0;
            padding: 12px;
        }
        .behavior-step-actions {
            display: grid;
            gap: 16px;
            margin-top: 14px;
        }
        .behavior-action-editor {
            gap: 10px;
            padding-top: 14px;
            border-top: 1px solid var(--line);
        }
        .behavior-action-editor:first-child {
            padding-top: 0;
            border-top: 0;
        }
        .behavior-action-editor select,
        .behavior-action-editor input {
            font-size: 12px;
        }
        .behavior-action-editor .input-with-button {
            grid-template-columns: minmax(0, 1fr) minmax(96px, auto);
            gap: 10px;
        }
        .behavior-helper-control {
            display: grid;
            gap: 8px;
        }
        .behavior-helper-select {
            display: grid;
            grid-template-rows: auto auto;
            gap: 4px;
            min-width: 0;
            color: var(--muted);
        }
        .toggle-inline {
            position: relative;
            display: grid;
            grid-template-columns: 34px minmax(0, 1fr);
            grid-template-rows: auto;
            align-items: center;
            gap: 8px;
            min-height: 24px;
            color: var(--text);
            cursor: pointer;
        }
        .toggle-inline input[type="checkbox"] {
            position: absolute;
            width: 1px;
            height: 1px;
            min-height: 0;
            padding: 0;
            margin: 0;
            opacity: 0;
            pointer-events: none;
        }
        .toggle-switch {
            position: relative;
            width: 34px;
            height: 18px;
            border: 1px solid #60707a;
            border-radius: 999px;
            background: #20262a;
            box-shadow: inset 0 0 0 1px rgba(49, 198, 164, 0.08);
            transition: background 120ms ease, border-color 120ms ease;
        }
        .toggle-switch::after {
            content: "";
            position: absolute;
            top: 1px;
            left: 1px;
            width: 14px;
            height: 14px;
            border-radius: 999px;
            background: var(--muted);
            transition: transform 120ms ease, background 120ms ease;
        }
        .toggle-inline input[type="checkbox"]:checked + .toggle-switch {
            border-color: var(--accent);
            background: #217a6a;
        }
        .toggle-inline input[type="checkbox"]:checked + .toggle-switch::after {
            transform: translateX(16px);
            background: #f7f7f4;
        }
        .toggle-inline input[type="checkbox"]:focus-visible + .toggle-switch {
            outline: 2px solid var(--accent);
            outline-offset: 2px;
        }
        .toggle-label {
            font-size: 12px;
            font-weight: 650;
        }
        .selected-behavior-editor {
            display: grid;
            gap: 14px;
        }
        .selected-behavior-editor h3 {
            margin: 0;
        }
        .rgb-subsection > summary {
            cursor: pointer;
            list-style-position: inside;
        }
        .rgb-summary {
            display: inline-grid;
            grid-template-columns: minmax(110px, 180px) 84px minmax(0, 1fr);
            align-items: center;
            gap: 10px;
            width: calc(100% - 22px);
            margin-left: 4px;
            vertical-align: middle;
        }
        .rgb-summary-title {
            color: var(--text);
            font-weight: 650;
            min-width: 0;
        }
        .rgb-summary-meta {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            align-items: center;
            min-width: 0;
        }
        .rgb-summary-swatch {
            display: block;
            width: 80px;
            height: 26px;
        }
        .rgb-summary code,
        .rgb-summary-meta code {
            min-width: 0;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            overflow-wrap: normal;
        }
        .rgb-subsection-body {
            margin-top: 10px;
        }
        .color-control {
            display: grid;
            gap: 8px;
        }
        .color-row {
            display: grid;
            grid-template-columns: 58px repeat(3, minmax(0, 1fr));
            gap: 10px;
            align-items: end;
        }
        input[type="color"] {
            min-height: 34px;
            padding: 2px;
        }
        .rgb-selected-list {
            display: flex;
            flex-wrap: wrap;
            gap: 5px;
            align-items: center;
        }
        .rgb-selected-list code {
            border: 1px solid var(--line);
            border-radius: 999px;
            padding: 2px 6px;
        }
        .rgb-led-list-label {
            color: var(--muted);
            font-size: 11px;
            font-weight: 600;
            margin-right: 2px;
        }
        .rgb-defined-list code {
            border-color: rgba(240, 200, 87, 0.76);
        }
        .rgb-all-color-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 8px;
        }
        .rgb-all-color-chip {
            display: grid;
            grid-template-columns: auto minmax(0, 1fr);
            gap: 3px 8px;
            align-items: center;
            border: 1px solid var(--line);
            border-radius: 6px;
            padding: 8px;
            background: #20282d;
        }
        .rgb-all-color-chip code {
            grid-column: 2;
            min-width: 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .inline-swatch {
            display: inline-block;
            width: 34px;
            height: 16px;
            vertical-align: middle;
            margin-right: 6px;
        }
        .tooltip {
            position: fixed;
            z-index: 50;
            max-width: min(460px, calc(100vw - 24px));
            padding: 7px 9px;
            border: 1px solid #6b7c85;
            border-radius: 6px;
            background: #151a1d;
            color: var(--text);
            font-size: 12px;
            line-height: 1.38;
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.36);
            pointer-events: none;
            white-space: pre-wrap;
            overflow-wrap: break-word;
        }
        .tooltip.layout-key-tooltip {
            width: min(440px, calc(100vw - 24px));
            max-width: min(440px, calc(100vw - 24px));
            padding: 0;
            white-space: normal;
            overflow-wrap: normal;
        }
        .layout-key-hover-card {
            display: grid;
            gap: 10px;
            padding: 10px;
        }
        .layout-key-hover-head {
            display: grid;
            gap: 5px;
            padding-bottom: 8px;
            border-bottom: 1px solid rgba(107, 124, 133, 0.58);
        }
        .layout-key-hover-title-row,
        .layout-key-combo-flow {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            align-items: center;
            min-width: 0;
        }
        .layout-key-hover-title {
            color: #f5f7f4;
            font-size: 14px;
            font-weight: 750;
            min-width: 0;
        }
        .layout-key-index-pill,
        .layout-key-combo-badge {
            border: 1px solid rgba(49, 198, 164, 0.7);
            border-radius: 999px;
            padding: 1px 7px;
            background: rgba(49, 198, 164, 0.12);
            color: #a8f0de;
            font-size: 11px;
            font-weight: 700;
        }
        .layout-key-hover-code,
        .layout-key-chip {
            display: inline-block;
            max-width: 100%;
            min-width: 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .layout-key-hover-code {
            color: #b7c6cc;
            font-size: 11px;
        }
        .layout-key-section {
            display: grid;
            gap: 7px;
        }
        .layout-key-section-body,
        .layout-key-nested-block {
            display: grid;
            min-width: 0;
            border-left: 1px solid rgba(107, 124, 133, 0.58);
        }
        .layout-key-section-body {
            gap: 7px;
            margin-left: 7px;
            padding-left: 10px;
        }
        .layout-key-nested-block {
            gap: 6px;
            margin-left: 30px;
            padding-left: 10px;
            border-left-color: rgba(49, 198, 164, 0.44);
        }
        .layout-key-section-title {
            color: #cfdadd;
            font-size: 11px;
            font-weight: 750;
        }
        .layout-key-behavior-title {
            color: #edf4f1;
            font-weight: 650;
        }
        .layout-key-branches {
            display: grid;
            gap: 5px;
        }
        .layout-key-branch {
            display: grid;
            grid-template-columns: 32px minmax(0, 1fr);
            gap: 8px;
            align-items: start;
        }
        .layout-key-branch-count {
            justify-self: start;
            border: 1px solid rgba(168, 178, 184, 0.5);
            border-radius: 999px;
            padding: 1px 6px;
            color: #dbe6e8;
            font-size: 11px;
            font-weight: 750;
        }
        .layout-key-actions {
            display: grid;
            gap: 5px;
            min-width: 0;
        }
        .layout-key-action {
            display: grid;
            grid-template-columns: 96px minmax(0, 1fr);
            gap: 8px;
            align-items: start;
            min-width: 0;
            padding: 5px 0 5px 8px;
            border-left: 3px solid var(--behavior-color, rgba(143, 176, 187, 0.84));
            border-radius: 0 5px 5px 0;
            background: linear-gradient(90deg, var(--behavior-color-wash, rgba(143, 176, 187, 0.1)), rgba(21, 26, 29, 0));
        }
        .layout-key-action.action-tap {
            --behavior-color: var(--layout-key-tap-color, #00d084);
            --behavior-text-color: var(--layout-key-tap-text, #f7f7f4);
            --behavior-color-wash: var(--layout-key-tap-wash, rgba(0, 208, 132, 0.13));
        }
        .layout-key-action.action-hold {
            --behavior-color: var(--layout-key-hold-color, #ff8a00);
            --behavior-text-color: var(--layout-key-hold-text, #18201d);
            --behavior-color-wash: var(--layout-key-hold-wash, rgba(255, 138, 0, 0.13));
        }
        .layout-key-action.action-long-hold {
            --behavior-color: var(--layout-key-long-hold-color, #3094ff);
            --behavior-text-color: var(--layout-key-long-hold-text, #f7f7f4);
            --behavior-color-wash: var(--layout-key-long-hold-wash, rgba(48, 148, 255, 0.13));
        }
        .layout-key-action-meta {
            display: flex;
            flex-wrap: wrap;
            gap: 4px;
            align-items: center;
            min-width: 0;
        }
        .layout-key-stage-chip {
            display: inline-flex;
            align-items: center;
            min-width: 0;
            max-width: 100%;
            border-radius: 999px;
            padding: 1px 6px;
            font-size: 11px;
            font-weight: 700;
            line-height: 1.35;
            white-space: nowrap;
        }
        .layout-key-stage-chip {
            border: 1px solid var(--behavior-color, rgba(143, 176, 187, 0.84));
            background: var(--behavior-color, rgba(143, 176, 187, 0.84));
            color: var(--behavior-text-color, #101719);
        }
        .layout-key-action-main {
            display: grid;
            gap: 2px;
            min-width: 0;
        }
        .layout-key-action-output {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            align-items: baseline;
            min-width: 0;
        }
        .layout-key-action-target {
            min-width: 0;
            overflow-wrap: break-word;
            color: #f0f6f2;
            font-weight: 650;
        }
        .layout-key-action-lifecycle {
            color: #b4c4c9;
            font-size: 11px;
            overflow-wrap: break-word;
        }
        .layout-key-macro-preview {
            display: grid;
            gap: 5px;
            min-width: 0;
            margin-top: 1px;
            padding: 6px 7px;
            border: 1px solid rgba(107, 124, 133, 0.35);
            border-left: 2px solid var(--behavior-color, rgba(143, 176, 187, 0.84));
            border-radius: 5px;
            background: rgba(11, 15, 17, 0.42);
        }
        .layout-key-macro-head {
            display: flex;
            flex-wrap: wrap;
            gap: 5px;
            align-items: center;
            min-width: 0;
            color: #dce7e8;
            font-size: 11px;
            font-weight: 750;
        }
        .layout-key-macro-code {
            color: #93a8ae;
            font-size: 10px;
            font-weight: 650;
        }
        .layout-key-macro-steps {
            display: grid;
            gap: 3px;
            min-width: 0;
        }
        .layout-key-macro-step {
            display: grid;
            grid-template-columns: 56px minmax(0, 1fr);
            gap: 5px;
            min-width: 0;
            align-items: baseline;
        }
        .layout-key-macro-kind {
            color: #9fb0b6;
            font-size: 10px;
            font-weight: 750;
            text-transform: uppercase;
        }
        .layout-key-macro-detail {
            min-width: 0;
            overflow-wrap: break-word;
            color: #d8e3e4;
            font-size: 11px;
        }
        .layout-key-macro-more,
        .layout-key-macro-empty {
            color: #9caeb4;
            font-size: 11px;
        }
        .layout-key-combo-list {
            display: grid;
            gap: 8px;
        }
        .layout-key-combo {
            display: grid;
            gap: 6px;
            padding-top: 7px;
            border-top: 1px solid rgba(107, 124, 133, 0.35);
        }
        .layout-key-combo:first-child {
            padding-top: 0;
            border-top: 0;
        }
        .layout-key-combo-badge {
            border-color: rgba(245, 245, 243, 0.65);
            background: rgba(245, 245, 243, 0.08);
            color: #f5f5f3;
        }
        .layout-key-chip {
            border: 1px solid rgba(107, 124, 133, 0.72);
            border-radius: 999px;
            padding: 2px 7px;
            background: #20282d;
            color: #eef4f1;
            font-size: 11px;
            font-weight: 650;
        }
        .layout-key-chip.output {
            border-color: rgba(49, 198, 164, 0.66);
            background: rgba(49, 198, 164, 0.12);
        }
        .layout-key-combo-arrow,
        .layout-key-combo-plus,
        .layout-key-empty {
            color: #9aa9b0;
        }
        .modal-backdrop {
            position: fixed;
            inset: 0;
            z-index: 40;
            display: grid;
            place-items: center;
            padding: 12px;
            background: rgba(7, 10, 12, 0.58);
        }
        .key-picker {
            display: grid;
            grid-template-rows: auto auto minmax(0, 1fr) auto;
            gap: 12px;
            width: min(1280px, calc(100vw - 24px));
            max-height: calc(100vh - 24px);
            border: 1px solid var(--line);
            border-radius: 8px;
            background: var(--panel);
            box-shadow: 0 18px 48px rgba(0, 0, 0, 0.44);
            padding: 12px;
        }
        .key-picker-head,
        .key-picker-actions {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
        }
        .key-picker-body {
            display: grid;
            grid-template-columns: 150px minmax(0, 1fr);
            gap: 12px;
            min-height: 0;
        }
        .key-picker-tabs,
        .key-picker-section {
            min-height: 0;
            overflow: auto;
        }
        .key-picker-tabs {
            display: grid;
            align-content: start;
            gap: 6px;
        }
        .key-picker-tab {
            text-align: left;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .key-picker-tab.active {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .key-picker-grid {
            display: grid;
            gap: 6px;
        }
        .key-picker-row {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
        }
        .key-picker-key {
            flex: 0 0 auto;
            width: max-content;
            min-width: max(46px, calc(var(--key-units, 1) * 42px));
            min-height: 38px;
            padding: 6px 8px;
            white-space: nowrap;
            overflow: visible;
            text-align: center;
        }
        .key-picker-keyboard {
            display: grid;
            gap: 8px;
            min-width: 0;
        }
        .key-picker-grid:not(.key-picker-keyboard) .key-picker-row {
            display: flex;
            flex-wrap: wrap;
            align-items: stretch;
            gap: 6px;
        }
        .key-picker-grid:not(.key-picker-keyboard) .key-picker-key {
            flex: 0 0 auto;
            justify-content: center;
        }
        .key-picker-spacer {
            flex: 0 0 calc(var(--key-units, 1) * 42px);
            min-height: 1px;
        }
        .key-picker-search {
            width: 100%;
            margin-bottom: 8px;
        }
        .key-picker-keyboard-svg-wrap {
            overflow: auto;
            padding: 8px;
            border: 1px solid var(--line);
            border-radius: 8px;
            background: #20262a;
        }
        .key-picker-keyboard-svg {
            display: block;
            width: max(100%, 1120px);
            height: auto;
        }
        .key-picker-svg-key {
            cursor: pointer;
            outline: none;
        }
        .key-picker-svg-key rect {
            fill: #1c2226;
            stroke: #60707a;
            stroke-width: 2;
        }
        .key-picker-svg-key:hover rect,
        .key-picker-svg-key:focus rect {
            stroke: var(--accent);
        }
        .key-picker-svg-key.selected rect {
            fill: #1f5d52;
            stroke: var(--accent);
            stroke-width: 3;
        }
        .key-picker-svg-key text {
            fill: var(--text);
            font-family: var(--vscode-font-family, system-ui, sans-serif);
            font-size: 15px;
            font-weight: 650;
            text-anchor: middle;
            dominant-baseline: middle;
            pointer-events: none;
        }
        .key-picker-svg-key .secondary {
            fill: var(--muted);
            font-size: 12px;
            font-weight: 600;
        }
        .key-picker-empty {
            padding: 14px;
            border: 1px dashed rgba(168, 178, 184, 0.34);
            border-radius: 6px;
            background: rgba(32, 38, 42, 0.42);
        }
        .key-picker-key.selected,
        .key-picker-mod.selected {
            border-color: var(--accent);
            background: #1f5d52;
        }
        .key-picker-mods,
        .key-picker-selection {
            display: flex;
            flex-wrap: wrap;
            gap: 6px;
            align-items: center;
        }
        .key-picker-expression {
            display: block;
            min-height: 32px;
            padding: 7px 9px;
            border: 1px solid var(--line);
            border-radius: 6px;
            background: #20262a;
        }
        .input-with-button {
            display: grid;
            grid-template-columns: minmax(0, 1fr) minmax(96px, auto);
            gap: 8px;
        }
        .input-with-button button {
            min-height: 38px;
            padding-inline: 8px;
            font-size: 11px;
            font-weight: 650;
            white-space: nowrap;
        }
        .toolbar {
            display: flex;
            gap: 8px;
            align-items: center;
            flex-wrap: wrap;
        }
        @media (max-width: 1240px) {
            .view-tab { min-width: 0; }
            .rgb-builder-layout,
            .layout-with-key-editor {
                grid-template-columns: 1fr;
            }
            .macro-builder-grid,
            .macro-tool-row,
            .macro-composer-grid,
            .macro-recorder-controls,
            .macro-recorder-delay-fields {
                grid-template-columns: 1fr;
            }
            .macro-recorder-primary-row {
                grid-template-columns: minmax(0, 1fr) minmax(72px, 92px);
            }
            .macro-recorder-actions {
                justify-content: stretch;
                justify-self: stretch;
            }
            .macro-slot-list {
                max-height: 280px;
            }
            .macro-composer-actions {
                justify-content: start;
            }
            .macro-step-fields {
                grid-template-columns: 1fr;
            }
            .layout-board-card {
                height: auto;
                min-height: 0;
            }
            .layout-selected-key-column {
                padding: 8px 0 0;
            }
        }
    </style>
    <style id="behaviorColorStyle" nonce="${nonce}"></style>
</head>
<body>
    <header>
        <div class="header-title">
            <h1>Charybdis Profile Studio</h1>
            <div id="subtitle" class="muted">Loading keymap.c, config.h, and rgb_config.c</div>
        </div>
        <div class="header-actions">
            <div class="header-action-row profile-picker">
                <span class="profile-picker-label">Profile</span>
                <select id="profileSelect" aria-label="Profile"></select>
                <div class="header-button-group">
                    <button id="createProfile">New profile</button>
                    <button id="cloneProfile">Clone</button>
                    <button id="renameProfile">Rename</button>
                    <button id="deleteProfile">Delete</button>
                </div>
            </div>
            <div class="header-action-row toolbar">
                <span class="profile-picker-label">Source</span>
                <div class="header-button-group">
                    <button id="openKeymap">keymap.c</button>
                    <button id="openRgb">rgb_config.c</button>
                    <button id="openConfig">config.h</button>
                    <button id="applyAll" class="primary dirty" hidden disabled>Apply all</button>
                    <button id="reload" class="primary">Reload source</button>
                </div>
            </div>
            <div class="header-action-row">
                <span class="profile-picker-label">Profile overview</span>
                <button id="generateProfileDocs">Create overview doc</button>
            </div>
            <div class="header-action-row">
                <span class="profile-picker-label">Firmware</span>
                <button id="compileFirmware" class="primary">Compile left + right</button>
            </div>
        </div>
    </header>
    <main id="app"></main>
    <div id="tooltip" class="tooltip" hidden></div>
    <div id="keyPickerHost"></div>
    <script nonce="${nonce}">
${getClientScript()}
    </script>
</body>
</html>`;
}

function getClientScript() {
    return `
(function () {
    const vscode = acquireVsCodeApi();
    let model = undefined;
    let activeLayer = undefined;
    let selectedKey = 0;
    let activeBehaviorKeycode = "";
    let activeView = "layout";
    let viewDrafts = {};
    let rgbGroupTarget = "layer";
    let rgbGroupOwner = "";
    let rgbSelectedLeds = [];
    let rgbLedGroupSource = "inline";
    let rgbReusableGroupDraftName = "";
    let rgbReusableGroupOriginalName = "";
    let rgbBuilderColor = undefined;
    let layoutComboPicking = false;
    let layoutComboSelection = [];
    let layoutComboOutput = "";
    let layoutComboInputs = "";
    let activeLayoutComboOriginal = undefined;
    let activeLayoutComboOriginalSource = "";
    let activeMacroKeycode = "";
    let macroDrafts = {};
    let macroRecording = false;
    let macroRecordDelays = true;
    let macroRecorderMode = "compact";
    let macroRecorderDelayThreshold = "30";
    let macroRecorderDelayRound = "10";
    let macroRecordedEvents = [];
    let macroRecordingPrefix = "";
    let macroRecorderLastEventAt = 0;
    let macroRecorderHeldKeys = {};
    let macroRecorderNotice = "";
    let macroRecorderHistoryStart = "";
    let pendingLayoutEdits = {};
    let pendingLayerAdds = [];
    let pendingLayerDeletes = [];
    let layerAddOpen = false;
    let layerDraftName = "";
    let copiedLayoutKey = "";
    let layoutDragState = undefined;
    let suppressNextLayoutClick = false;
    let lastLayoutKeyClick = { index: undefined, time: 0 };
    let keyPicker = undefined;
    let notice = "";
    let dismissedStatusSignature = "";
    let layoutNotice = "";
    let localUndoStack = [];
    let localRedoStack = [];
    let currentLocalSnapshot = "";
    let restoringLocalSnapshot = false;
    let macroSlotHeightFrame = 0;
    const localHistoryLimit = 100;
    const layoutSlotCount = ${LAYOUT_SLOT_COUNT};
    const keyBehaviorTimingMaxMs = ${KEY_BEHAVIOR_TIMING_MAX_MS};
    const tapCountNames = ${JSON.stringify(TAP_COUNT_NAMES)};
    const views = [
        ["layout", "Layout"],
        ["macros", "Macros"],
        ["rgb", "RGB"],
        ["defaults", "Defaults"]
    ];
    // Tooltip copy should say what the control affects, where it writes or stages data, and any non-obvious fallback semantics.
    const headerTooltips = {
        profileSelect: "Choose which keymap folder Profile Studio edits. Switching profiles discards uncommitted Studio edits.",
        createProfile: "Create a new keymap folder from the starter Profile Studio template and register it in qmk.json.",
        cloneProfile: "Copy the active keymap folder to a new profile and register the clone in qmk.json.",
        renameProfile: "Move the active non-default profile folder to a new keymap name and update qmk.json.",
        deleteProfile: "Delete the active non-default profile folder and remove its qmk.json build target.",
        openKeymap: "Open keymap.c beside the studio so you can inspect or hand-edit the source.",
        openRgb: "Open rgb_config.c beside the studio so you can inspect or hand-edit the source.",
        openConfig: "Open config.h beside the studio so you can inspect layer enum and timing settings.",
        generateProfileDocs: "Create or refresh the generated profile overview Markdown and assets from the active profile source files on disk.",
        compileFirmware: "Compile separate left and right UF2 firmware files for the active profile.",
        applyAll: "Write all staged Studio changes, including layer structure and staged layout edits.",
        reload: "Reload keymap.c, config.h, and rgb_config.c from disk, discarding uncommitted Studio edits."
    };
    const viewTooltips = {
        layout: "Edit layer keys and behavior rows using the physical keyboard layout as the filter.",
        macros: "Build, record, preview, and edit VIA macro payload strings in keymap.c.",
        rgb: "Edit rgb_config.c layer colors, feedback colors, locality, and LED group tables.",
        defaults: "Edit config.h key timing, pointer speed, pointing-mode speed, sniping, auto-mouse, base lighting, and lighting feedback defaults."
    };
    const panelTooltips = {
        Status: "Parser messages, write status, and warnings from the current studio model.",
        Layout: "Physical keyboard preview, layer tabs, selected-key editor, combo builder, and selected-key behavior editor for the active layer.",
        "Layer Overview": "Read-only summary of behavior rows, macros, combos, and pointing modes reachable from keys on the active layer.",
        "Macro Builder": "Build, record, preview, and write payload strings for the VIA_MACROS(MACRO) table in keymap.c. Payload edits stay local until Apply macro.",
        "Combo Builder": "Create or update COMBOS(COMBO) rows in keymap.c. Output chooses what the combo emits; inputs are physical key slots from the layout.",
        "RGB LED Group Builder": "Select physical LEDs and append a row to one of the rgb_config.c LED group tables. Reusable groups define LED sets; table rows decide where and how they are used.",
        "Layer Colors": "Edit layer_colors[] HSV values and render mode for each layer. LED group rows can override specific LEDs.",
        "Layer LED Groups": "Inspect layer_led_groups_data[] rows that override specific LEDs for one layer or all layers.",
        "Auto-mouse Fade": "Edit the destination color and fade mode used as auto-mouse approaches its timeout.",
        "Pointing-mode Colors": "Edit pd_mode_colors[] HSV values and locality for each active pointing mode.",
        "Pointing-mode LED Groups": "Inspect pd_mode_led_groups_data[] rows that override specific LEDs for one pointing mode or all pointing modes.",
        "Combo Feedback": "Edit combo feedback color and locality shown while combo member keys are active.",
        "Combo Feedback LED Groups": "Inspect combo_feedback_led_groups_data[] rows that override the combo feedback footprint.",
        "Key Behavior Feedback": "Edit key-behavior feedback colors, locality, and policy for tap, hold, long-hold, and tap-count branch states.",
        "Key Behavior Feedback LED Groups": "Inspect key_behavior_feedback_led_groups_data[] rows that override specific feedback semantics or all feedback groups.",
        "Key Timing": "Edit config.h timing defaults for QMK dual-role keys, combos, and custom key_behaviors[] tap, hold, and multi-tap handling.",
        "Normal Pointer Speed": "Edit config.h normal pointer DPI/CPI ladder minimum and step size.",
        "Pointing Mode Speeds": "Edit config.h pointer DPI/CPI used while drag-scroll or pointing modes are active. Pointing-mode overrides of 0 fall back to normal pointer speed.",
        "Sniping": "Edit config.h sniping sensitivity ladder and automatic sniping layer trigger.",
        "Auto-mouse": "Edit config.h auto-mouse enablement, destination layer, and timeout after pointing movement.",
        "Base Lighting": "Edit config.h base RGB Matrix mode, color, brightness cap, and inactivity timeout.",
        "Lighting Feedback": "Edit config.h RGB feedback stage toggles, visible feedback timing, auto-mouse fade timing, and LED refresh cadence."
    };
    const actionTooltips = {
        applyKey: "Stage the selected key value as a pending layout edit. Use Apply layout change to write staged edits to keymap.c.",
        saveSelectedBehavior: "Create or replace the key_behaviors[] row for this selected keycode in keymap.c.",
        editComboOutputBehavior: "Load this combo output keycode into the behavior editor so its key_behaviors[] row can be created or edited.",
        editLayoutCombo: "Load this existing combo into the combo builder so its output or physical input keys can be edited.",
        addBehavior: "Append a simple key_behaviors[] row to keymap.c.",
        updateLayerColor: "Write this layer HSV color and render mode back to layer_colors[] in rgb_config.c.",
        updatePdModeColor: "Write this pointing-mode HSV color and locality back to pd_mode_colors[] in rgb_config.c.",
        updateAutomouseFade: "Write this auto-mouse fade destination color and fade mode back to rgb_config.c.",
        updateComboFeedback: "Write combo feedback HSV color and locality back to rgb_config.c.",
        updateKeyBehaviorFeedback: "Write all key behavior feedback colors, locality, and policy fields back to rgb_config.c.",
        updateConfigDefaults: "Write these default settings back to config.h.",
        saveRgbReusableLedGroup: "Create, update, or rename a reusable RGB_LED_GROUP_* definition in rgb_config.c using the currently selected LEDs.",
        deleteRgbReusableLedGroup: "Delete this reusable RGB_LED_GROUP_* definition. Used groups are disabled because table rows still reference them.",
        editReusableLedGroup: "Load this reusable LED group into the editor so its name or LED membership can be changed.",
        useReusableLedGroup: "Use this reusable LED set as the LED group source for the row builder above.",
        clearReusableLedGroupDraft: "Clear the reusable LED group editor without changing rgb_config.c.",
        showAddLayerDraft: "Open the staged new-layer form. Nothing is written until Apply layer changes.",
        cancelLayerDraft: "Close the new-layer form without staging a layer.",
        stageLayerDraft: "Stage a fully transparent layer with a random RGB color.",
        deleteLayerDraft: "Stage deletion of the active layer. Apply will fail if firmware references still use it.",
        discardLayerChanges: "Discard staged layer additions and deletions.",
        applyLayerChanges: "Write staged layer additions and deletions to config.h, keymap.c, and rgb_config.c.",
        addRgbLedGroup: "Append a new row to the selected RGB LED group table using the chosen owner, LED group source, and HSV color.",
        clearRgbSelection: "Remove all inline LED selections from the group builder. Reusable group definitions are not changed.",
        toggleRgbTrackball: "Add or remove the trackball LED index 56 from the current inline LED selection.",
        updateViaMacro: "Write the selected VIA macro payload draft back to the VIA_MACROS(MACRO) row in keymap.c.",
        selectMacroSlot: "Select this VIA macro slot for editing. Unsaved drafts in other slots are kept locally.",
        insertMacroStep: "Insert the configured step at the cursor in the selected payload draft.",
        clearMacroPayload: "Clear the selected macro payload draft. Nothing is written to keymap.c until Apply macro.",
        startMacroRecording: "Start capturing browser keydown and keyup events and append the generated payload to the selected macro draft.",
        stopMacroRecording: "Stop recording and keep the generated payload in the selected macro draft.",
        clearMacroRecording: "Clear the current recording take and restore the payload captured when recording started.",
        dismissStatus: "Dismiss the current status popup until the status changes.",
        addCombo: "Append a COMBOS(COMBO) row using the entered output keycode and input key expressions.",
        addLayoutCombo: "Append a combo row, or replace the loaded combo row, using the selected physical layout slots as inputs.",
        applyLayoutChanges: "Write pending layout drag/drop and paste edits back to keymap.c.",
        toggleLayoutComboPicking: "Switch the layout board into combo input picking mode; click keys on the board to add or remove inputs.",
        toggleLayoutComboKey: "Add or remove this key from the pending layout combo.",
        clearLayoutComboSelection: "Clear the pending layout combo input keys.",
        selectKey: "Select this physical key. Double-click to pick a keycode, drag onto another key to swap, or use copy/paste between selected keys.",
        toggleRgbLed: "Add or remove this physical LED from the new RGB group.",
        openKeyPicker: "Open a VIA-style keycode picker with sections, QWERTY keys, modifiers, and OK/Cancel confirmation."
    };
    const writeActions = new Set([
        "applyKey",
        "saveSelectedBehavior",
        "addBehavior",
        "updateLayerColor",
        "updatePdModeColor",
        "updateAutomouseFade",
        "updateComboFeedback",
        "updateKeyBehaviorFeedback",
        "updateConfigDefaults",
        "saveRgbReusableLedGroup",
        "deleteRgbReusableLedGroup",
        "addRgbLedGroup",
        "updateViaMacro",
        "addCombo",
        "addLayoutCombo"
    ]);
    const fieldTooltips = {
        layer: "The active firmware layer shown in the board preview. Switch layers with the tabs above the board.",
        "layout index": "The physical LAYOUT() slot index for the selected key. It is fixed by the keyboard geometry.",
        key: "User-facing key label or expression to stage for the selected LAYOUT() slot, for example A, Enter, Space, _______, LT(LAYER_NAV, Slash), or Shift+Esc.",
        source: "The raw C expression currently stored for this row or selected source slot.",
        tap_hold_term: "Optional tap-vs-hold boundary for this key behavior row. Valid range: 1-65535 ms; empty uses the displayed default.",
        longer_hold_term: "Optional hold-vs-long-hold boundary for this key behavior row. Valid range: 1-65535 ms; empty uses the displayed default.",
        multi_tap_term: "Optional repeated-tap window for this key behavior row. Valid range: 1-65535 ms; empty uses the displayed default.",
        rgb_branch_confirm_term: "Optional RGB-visible confirmation window for committed non-base tap-count branches. Valid range: 1-65535 ms; empty uses the displayed default.",
        skip_rgb_branch_confirm: "Skip the RGB branch-confirm window for this behavior row. The key output still runs; only the visual pause is removed.",
        tap: "Enable the tap-tier action for this tap-count branch. When enabled, choose a helper and action below.",
        hold: "Enable the hold-tier action that can run after the tap-hold term for this branch.",
        "long hold": "Enable the long-hold-tier action that can run after the longer-hold term for this branch.",
        table: "Choose which rgb_config.c LED group table receives the new row: layer, pointing mode, combo feedback, or key-behavior feedback.",
        owner: "Owner for the selected LED group table. It decides which layer, pointing mode, or feedback semantic the row applies to; combo feedback has one shared owner.",
        "pointing mode": "Pointing mode whose RGB color or LED group row is being edited.",
        semantic: "Key-behavior feedback state that owns this LED group, such as tap pending, tap committed, hold active, or all feedback groups.",
        "group name": "Reusable RGB_LED_GROUP_* definition name near the LED map in rgb_config.c. The name stores only LED membership, not color or owner.",
        "LED group": "Choose a reusable RGB_LED_GROUP_* LED set, or use the current inline LED selection from the board.",
        mode: "Select how this RGB row behaves. For layers it controls which keys are painted; for auto-mouse it controls how the fade destination is reached.",
        locality: "Choose where this feedback paints: both halves, one half, the triggering key half, or only the triggering keys.",
        "branch confirm mode": "Choose which tap-count branches show a branch-confirm feedback window before their action finishes.",
        "tap commit mode": "Choose which key-behavior taps show tap-commit feedback after the tap action commits.",
        picker: "Pick an approximate RGB color. The studio converts the browser color into QMK HSV channels.",
        h: "QMK HSV hue channel, 0-255. Hue chooses the color family.",
        s: "QMK HSV saturation channel, 0-255. 0 is white/gray; 255 is fully saturated.",
        v: "QMK HSV value/brightness channel. Use 0-255 or a safe constant such as RGB_MATRIX_MAXIMUM_BRIGHTNESS.",
        output: "The key or action produced by this combo, written as the combo output in keymap.c.",
        "output behavior": "The key behavior row that runs after this combo emits its output keycode, if one exists.",
        inputs: "Comma-separated physical combo input key expressions, such as D, F. Use the layout picker to choose slots from the active layer.",
        slot: "The VIA macro keycode slot. Selecting a slot changes the editor target but does not write files.",
        payload: "Raw VIA macro payload string for the selected slot. Plain ASCII types text; {KC_A} taps keys; {KC_LGUI,KC_SPC} taps chords; {+KC_A}/{-KC_A} hold and release; {250} waits milliseconds.",
        "step type": "Choose which payload fragment the step builder inserts: tap/chord, text, key down, key up, or delay.",
        text: "Plain ASCII text inserted directly into the macro payload. Braces are reserved for macro commands.",
        "macro keys": "One or more raw macro keycodes to tap together, such as KC_A, KC_LGUI, KC_SPC. Friendly chords are converted before insertion.",
        "macro key": "One raw macro keycode used by a key-down or key-up command.",
        "delay ms": "Milliseconds to wait before the next macro step. Inserted as a {number} delay command.",
        "record mode": "Compact turns matching down/up pairs into taps or chords and printable keys into text when possible. Exact preserves explicit key-down and key-up commands.",
        "record delays": "Insert delay commands from elapsed time between recorded key events. Turning this off records only key events.",
        "delay threshold ms": "Only elapsed gaps at or above this many milliseconds become delay commands.",
        "delay round ms": "Round recorded delays to this many milliseconds before applying the threshold.",
        leds: "Physical RGB LED indices contained in this row or reusable group.",
        "led group": "Authored LED group expression in rgb_config.c. Reusable groups reference RGB_LED_GROUP_*; inline rows spell out RGB_LED_GROUP(...).",
        color: "HSV color expression used by this row. HSV(0, 0, 0) means inherit the owning stage color for LED group rows.",
        default: "The current value written for this config.h macro.",
        setting: "The user-facing name for this config.h default.",
        macro: "The exact config.h #define patched by this control.",
        rgb: "The RGB color and locality associated with this reachable pointing mode.",
        badge: "The small badge shown on the layout preview for this combo.",
        behavior: "The key behavior row attached to this keycode.",
        steps: "Actions for each tap-count branch in this behavior row.",
        "key on layer": "Keys on the active layer that use this behavior row.",
        "reachable via": "The visible key or behavior action that can reach this pointing mode."
    };
    const qmkKeyLabels = ${JSON.stringify(QMK_KEY_LABELS)};
    const qmkKeyAliases = ${JSON.stringify(qmkKeyAliasesFromEntries(fallbackQmkKeycodeCatalog().entries))};
    const shiftedKeyOutputLabels = ${JSON.stringify(SHIFTED_KEY_OUTPUT_LABELS)};
    const shiftedKeyBaseLabels = ${JSON.stringify(SHIFTED_KEY_BASE_LABELS)};
    let macroPayloadKeycodes = new Set();
    const qmkKeycodeSectionGroups = ${JSON.stringify(QMK_KEYCODE_SECTION_GROUPS)};
    const keyPickerKeyboardSvgLayout = ${JSON.stringify(KEY_PICKER_KEYBOARD_SVG_LAYOUT)};
    const modWrapperLabels = ${JSON.stringify(MOD_WRAPPER_LABELS)};
    const layoutKeyCallFunctions = new Set(${JSON.stringify(LAYOUT_KEY_CALL_FUNCTIONS)});
    const userKeyAliases = ${JSON.stringify(USER_KEY_ALIASES)};
    const rgbLayerAllGroups = ${JSON.stringify(RGB_LAYER_GROUP_ALL)};
    const rgbPdModeAllGroups = ${JSON.stringify(RGB_PD_MODE_GROUP_ALL)};
    const keyBehaviorAllGroups = ${JSON.stringify(KEY_FEEDBACK_GROUP_ALL)};
    const rgbLocalities = ${JSON.stringify(RGB_LOCALITIES)};
    const automouseFadeModes = ${JSON.stringify(AUTOMOUSE_FADE_MODES)};
    const keyFeedbackTapCommitModes = ${JSON.stringify(KEY_FEEDBACK_TAP_COMMIT_MODES)};
    const keyFeedbackBranchConfirmModes = ${JSON.stringify(KEY_FEEDBACK_BRANCH_CONFIRM_MODES)};
    const keyBehaviorRgbSemantics = [
        keyBehaviorAllGroups,
        "KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH",
        "KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED",
        "KEY_FEEDBACK_GROUP_TAP_COMMITTED",
        "KEY_FEEDBACK_GROUP_HOLD_ACTIVE",
        "KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE"
    ];
    const keyBehaviorRgbSemanticLabels = {
        KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH: "Tap pending",
        KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED: "Committed tap-count branch",
        KEY_FEEDBACK_GROUP_TAP_COMMITTED: "Tap committed",
        KEY_FEEDBACK_GROUP_HOLD_ACTIVE: "Hold active",
        KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: "Long hold active"
    };
    keyBehaviorRgbSemanticLabels[keyBehaviorAllGroups] = "All feedback groups";
    const keyPickerModifiers = ["Ctrl", "Shift", "Alt", "Cmd", "Right Ctrl", "Right Shift", "Right Alt", "Right Cmd"];
    const keyPickerLayerTapPrefix = "__LT_LAYER__:";
    const macroRecorderEventCodeMap = {};
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("").forEach((letter) => {
        macroRecorderEventCodeMap["Key" + letter] = "KC_" + letter;
    });
    "0123456789".split("").forEach((digit) => {
        macroRecorderEventCodeMap["Digit" + digit] = "KC_" + digit;
        macroRecorderEventCodeMap["Numpad" + digit] = "KC_P" + digit;
    });
    for (let index = 1; index <= 24; index += 1) {
        macroRecorderEventCodeMap["F" + index] = "KC_F" + index;
    }
    Object.assign(macroRecorderEventCodeMap, {
        AltLeft: "KC_LALT",
        AltRight: "KC_RALT",
        ArrowDown: "KC_DOWN",
        ArrowLeft: "KC_LEFT",
        ArrowRight: "KC_RGHT",
        ArrowUp: "KC_UP",
        AudioVolumeDown: "KC_VOLD",
        AudioVolumeMute: "KC_MUTE",
        AudioVolumeUp: "KC_VOLU",
        Backquote: "KC_GRV",
        Backslash: "KC_BSLS",
        Backspace: "KC_BSPC",
        BracketLeft: "KC_LBRC",
        BracketRight: "KC_RBRC",
        CapsLock: "KC_CAPS",
        Comma: "KC_COMM",
        ContextMenu: "KC_APP",
        ControlLeft: "KC_LCTL",
        ControlRight: "KC_RCTL",
        Delete: "KC_DEL",
        End: "KC_END",
        Enter: "KC_ENT",
        Equal: "KC_EQL",
        Escape: "KC_ESC",
        Help: "KC_HELP",
        Home: "KC_HOME",
        Insert: "KC_INS",
        MediaPlayPause: "KC_MPLY",
        MediaStop: "KC_MSTP",
        MediaTrackNext: "KC_MNXT",
        MediaTrackPrevious: "KC_MPRV",
        MetaLeft: "KC_LGUI",
        MetaRight: "KC_RGUI",
        Minus: "KC_MINS",
        NumLock: "KC_NUM",
        NumpadAdd: "KC_PPLS",
        NumpadComma: "KC_PCMM",
        NumpadDecimal: "KC_PDOT",
        NumpadDivide: "KC_PSLS",
        NumpadEnter: "KC_PENT",
        NumpadEqual: "KC_PEQL",
        NumpadMultiply: "KC_PAST",
        NumpadSubtract: "KC_PMNS",
        OSLeft: "KC_LGUI",
        OSRight: "KC_RGUI",
        PageDown: "KC_PGDN",
        PageUp: "KC_PGUP",
        Pause: "KC_PAUS",
        Period: "KC_DOT",
        PrintScreen: "KC_PSCR",
        Quote: "KC_QUOT",
        ScrollLock: "KC_SCRL",
        Semicolon: "KC_SCLN",
        ShiftLeft: "KC_LSFT",
        ShiftRight: "KC_RSFT",
        Slash: "KC_SLSH",
        Space: "KC_SPC",
        Tab: "KC_TAB"
    });
    const macroRecorderKeyFallbackMap = {
        " ": "KC_SPC",
        ArrowDown: "KC_DOWN",
        ArrowLeft: "KC_LEFT",
        ArrowRight: "KC_RGHT",
        ArrowUp: "KC_UP",
        Backspace: "KC_BSPC",
        CapsLock: "KC_CAPS",
        Delete: "KC_DEL",
        End: "KC_END",
        Enter: "KC_ENT",
        Escape: "KC_ESC",
        Home: "KC_HOME",
        Insert: "KC_INS",
        PageDown: "KC_PGDN",
        PageUp: "KC_PGUP",
        Tab: "KC_TAB"
    };
    const macroRecorderTextKeyMap = {};
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("").forEach((letter) => {
        macroRecorderTextKeyMap["KC_" + letter] = [letter.toLowerCase(), letter];
    });
    Object.assign(macroRecorderTextKeyMap, {
        KC_0: ["0", ")"],
        KC_1: ["1", "!"],
        KC_2: ["2", "@"],
        KC_3: ["3", "#"],
        KC_4: ["4", "$"],
        KC_5: ["5", "%"],
        KC_6: ["6", "^"],
        KC_7: ["7", "&"],
        KC_8: ["8", "*"],
        KC_9: ["9", "("],
        KC_BSLS: ["\\\\", "|"],
        KC_COMM: [",", "<"],
        KC_DOT: [".", ">"],
        KC_EQL: ["=", "+"],
        KC_GRV: ["\`", "~"],
        KC_LBRC: ["[", "{"],
        KC_MINS: ["-", "_"],
        KC_QUOT: ["'", '"'],
        KC_RBRC: ["]", "}"],
        KC_SCLN: [";", ":"],
        KC_SLSH: ["/", "?"],
        KC_SPC: [" ", " "]
    });
    const keyPickerSections = [
        {
            id: "qwerty",
            label: "Keyboard",
            kind: "keyboard",
            layout: keyPickerKeyboardSvgLayout
        },
        {
            id: "symbols",
            label: "Symbols",
            rows: [
                ["KC_EXLM", "KC_AT", "KC_HASH", "KC_DLR", "KC_PERC", "KC_CIRC", "KC_AMPR", "KC_ASTR", "KC_LPRN", "KC_RPRN"],
                ["KC_UNDS", "KC_PLUS", "KC_LCBR", "KC_RCBR", "KC_PIPE", "KC_COLN", "KC_DQUO", "KC_LABK", "KC_RABK", "KC_TILD"],
                ["KC_MINS", "KC_EQL", "KC_LBRC", "KC_RBRC", "KC_BSLS", "KC_SCLN", "KC_QUOT", "KC_COMM", "KC_DOT", "KC_SLSH"]
            ]
        },
        {
            id: "navigation",
            label: "Navigation",
            rows: [
                ["Left", "Down", "Up", "Right"],
                ["KC_HOME", "KC_END", "KC_PGUP", "KC_PGDN", "KC_INS", "KC_DEL"],
                ["KC_PSCR", "KC_SCRL", "KC_PAUS", "KC_CAPS", "KC_NUM"]
            ]
        },
        {
            id: "numpad",
            label: "Numpad",
            rows: [
                ["KC_NUM", "KC_PSLS", "KC_PAST", "KC_PMNS"],
                ["KC_P7", "KC_P8", "KC_P9", "KC_PPLS"],
                ["KC_P4", "KC_P5", "KC_P6", "KC_PENT"],
                ["KC_P1", "KC_P2", "KC_P3", "KC_PEQL"],
                ["KC_P0", "KC_PDOT", "KC_PCMM"]
            ]
        },
        {
            id: "keyboard-extras",
            label: "More keys",
            rows: [
                ["KC_F13", "KC_F14", "KC_F15", "KC_F16", "KC_F17", "KC_F18"],
                ["KC_F19", "KC_F20", "KC_F21", "KC_F22", "KC_F23", "KC_F24"],
                ["KC_INT1", "KC_INT2", "KC_INT3", "KC_INT4", "KC_INT5", "KC_INT6", "KC_INT7", "KC_INT8", "KC_INT9"],
                ["KC_LNG1", "KC_LNG2", "KC_LNG3", "KC_LNG4", "KC_LNG5", "KC_LNG6", "KC_LNG7", "KC_LNG8", "KC_LNG9"],
                ["KC_EXEC", "KC_HELP", "KC_MENU", "KC_SLCT", "KC_STOP", "KC_AGIN"],
                ["KC_UNDO", "KC_CUT", "KC_COPY", "KC_PSTE", "KC_FIND", "KC_ERAS"],
                ["KC_LCAP", "KC_LNUM", "KC_LSCR", "KC_SYRQ", "KC_CNCL", "KC_CLR"],
                ["KC_NUBS", "KC_KB_POWER", "KC_SEPR", "KC_OUT", "KC_OPER", "KC_CLAG", "KC_CRSL", "KC_EXSL"]
            ]
        },
        {
            id: "mouse",
            label: "Mouse",
            rows: [
                ["KC_MS_U", "KC_MS_D", "KC_MS_L", "KC_MS_R"],
                ["KC_BTN1", "KC_BTN2", "KC_BTN3", "KC_BTN4", "KC_BTN5"],
                ["KC_BTN6", "KC_BTN7", "KC_BTN8"],
                ["KC_WH_U", "KC_WH_D", "KC_WH_L", "KC_WH_R"],
                ["KC_ACL0", "KC_ACL1", "KC_ACL2"]
            ]
        },
        {
            id: "layers",
            label: "Layers",
            rows: []
        },
        {
            id: "modes",
            label: "PD modes",
            rows: []
        },
        {
            id: "macros",
            label: "Macros",
            rows: []
        },
        {
            id: "custom",
            label: "Custom",
            rows: [
                ["_______", "XXXXXXX"]
            ]
        }
    ];
    const layoutToLedIndex = {
        0: 0, 1: 7, 2: 8, 3: 15, 4: 16, 5: 20,
        12: 1, 13: 6, 14: 9, 15: 14, 16: 17, 17: 21,
        24: 2, 25: 5, 26: 10, 27: 13, 28: 18, 29: 22,
        36: 3, 37: 4, 38: 11, 39: 12, 40: 19, 41: 23,
        6: 49, 7: 45, 8: 44, 9: 37, 10: 36, 11: 29,
        18: 50, 19: 46, 20: 43, 21: 38, 22: 35, 23: 30,
        30: 51, 31: 47, 32: 42, 33: 39, 34: 34, 35: 31,
        42: 52, 43: 48, 44: 41, 45: 40, 46: 33, 47: 32,
        48: 26, 49: 27, 50: 28, 51: 53, 52: 54, 53: 25, 54: 24, 55: 55
    };
    const keyboardGeometry = {
        width: 1120,
        height: 620,
        keyWidth: 58,
        keyHeight: 58,
        radius: 7,
        yOffset: 54,
        rowStep: 64,
        layoutViewBox: { x: 0, y: 86, width: 1120, height: 510 },
        leftX: [36, 102, 168, 234, 300, 366],
        leftTopY: [118, 118, 78, 54, 78, 78],
        rightX: [698, 764, 830, 896, 962, 1028],
        rightTopY: [78, 78, 54, 78, 118, 118],
        thumbs: {
            48: { x: 328, y: 354, angle: 0 },
            49: { x: 396, y: 350, angle: 10 },
            50: { x: 468, y: 358, angle: 17 },
            51: { x: 576, y: 358, angle: -17 },
            52: { x: 648, y: 350, angle: -10 },
            53: { x: 398, y: 432, angle: 9 },
            54: { x: 468, y: 446, angle: 15 },
            55: { x: 578, y: 432, angle: -15 }
        }
    };

    const app = document.getElementById("app");
    const subtitle = document.getElementById("subtitle");
    const tooltip = document.getElementById("tooltip");
    const keyPickerHost = document.getElementById("keyPickerHost");
    const behaviorColorStyle = document.getElementById("behaviorColorStyle");
    let activeTooltipTarget = undefined;

    document.getElementById("applyAll").addEventListener("click", () => {
        applyAllStagedChanges();
    });
    document.getElementById("reload").addEventListener("click", () => {
        discardLocalDraftState();
        post({ type: "refresh" });
    });
    document.getElementById("profileSelect").addEventListener("change", (event) => {
        discardLocalDraftState();
        post({ type: "selectProfile", profileId: event.target.value || "" });
    });
    document.getElementById("createProfile").addEventListener("click", () => {
        discardLocalDraftState();
        post({ type: "requestCreateProfile" });
    });
    document.getElementById("cloneProfile").addEventListener("click", () => {
        discardLocalDraftState();
        post({ type: "requestCloneProfile" });
    });
    document.getElementById("renameProfile").addEventListener("click", () => {
        discardLocalDraftState();
        post({ type: "requestRenameProfile" });
    });
    document.getElementById("deleteProfile").addEventListener("click", () => {
        discardLocalDraftState();
        post({ type: "requestDeleteProfile" });
    });
    document.getElementById("openKeymap").addEventListener("click", () => vscode.postMessage({ type: "openSource", file: "keymap" }));
    document.getElementById("openRgb").addEventListener("click", () => vscode.postMessage({ type: "openSource", file: "rgb" }));
    document.getElementById("openConfig").addEventListener("click", () => vscode.postMessage({ type: "openSource", file: "config" }));
    document.getElementById("generateProfileDocs").addEventListener("click", () => {
        requestProfileDocsGeneration();
    });
    document.getElementById("compileFirmware").addEventListener("click", () => {
        requestFirmwareCompile();
    });
    document.addEventListener("pointerover", (event) => {
        const target = tooltipTarget(event.target);
        if (target) showTooltip(target, event);
    });
    document.addEventListener("pointermove", (event) => {
        if (activeTooltipTarget) positionTooltip(event.clientX, event.clientY);
    });
    document.addEventListener("pointerout", (event) => {
        if (activeTooltipTarget && !activeTooltipTarget.contains(event.relatedTarget)) {
            hideTooltip();
        }
    });
    document.addEventListener("focusin", (event) => {
        const target = tooltipTarget(event.target);
        if (target) showTooltip(target);
    });
    document.addEventListener("focusout", hideTooltip);
    keyPickerHost.addEventListener("click", (event) => {
        const target = event.target.closest("[data-picker-action]");
        if (!target) return;
        const action = target.dataset.pickerAction;
        if (action === "section") {
            keyPicker.section = target.dataset.section;
            renderKeyPicker();
        } else if (action === "modifier") {
            togglePickerModifier(target.dataset.modifier);
        } else if (action === "key") {
            choosePickerKey(target.dataset.value);
        } else if (action === "removeKey") {
            removePickerKey(Number(target.dataset.index));
        } else if (action === "clearLayerTap") {
            keyPicker.layerTapLayer = "";
            renderKeyPicker();
        } else if (action === "clear") {
            keyPicker.mods = [];
            keyPicker.keys = [];
            keyPicker.layerTapLayer = "";
            renderKeyPicker();
        } else if (action === "cancel") {
            closeKeyPicker();
        } else if (action === "ok") {
            confirmKeyPicker();
        }
    });
    keyPickerHost.addEventListener("keydown", (event) => {
        const target = event.target.closest("[data-picker-action='key']");
        if (!target || (event.key !== "Enter" && event.key !== " ")) return;
        event.preventDefault();
        choosePickerKey(target.dataset.value);
    });
    window.addEventListener("keydown", handleMacroRecorderKeyEvent, true);
    window.addEventListener("keyup", handleMacroRecorderKeyEvent, true);
    window.addEventListener("resize", scheduleMacroSlotBrowserHeightSync);
    document.addEventListener("keydown", (event) => {
        if (!(event.metaKey || event.ctrlKey) || event.altKey) return;
        const key = String(event.key || "").toLowerCase();
        if (!keyPicker && key === "c" && canUseLayoutClipboard(event.target)) {
            copySelectedLayoutKey();
            return;
        }
        const wantsUndo = key === "z" && !event.shiftKey;
        const wantsRedo = key === "y" || (key === "z" && event.shiftKey);
        if (!wantsUndo && !wantsRedo) return;
        if (keyPicker) return;
        if (wantsUndo && !localUndoStack.length) return;
        if (wantsRedo && !localRedoStack.length) return;
        event.preventDefault();
        if (wantsUndo) {
            undoLocalEdit();
        } else {
            redoLocalEdit();
        }
    }, true);
    document.addEventListener("copy", (event) => {
        if (!canUseLayoutClipboard(event.target)) return;
        const copied = copySelectedLayoutKey();
        if (!copied) return;
        event.clipboardData?.setData("text/plain", copied);
        event.preventDefault();
    });
    document.addEventListener("paste", (event) => {
        if (!canUseLayoutClipboard(event.target)) return;
        const text = event.clipboardData?.getData("text/plain") || copiedLayoutKey;
        if (!text) return;
        event.preventDefault();
        const before = currentLocalSnapshot || serializeLocalState();
        if (pasteLayoutKey(text)) {
            commitLocalHistory(before);
        }
    });
    keyPickerHost.addEventListener("input", (event) => {
        if (!keyPicker || !event.target.matches("[data-picker-search]")) return;
        keyPicker.search = event.target.value || "";
        const sectionHost = keyPickerHost.querySelector(".key-picker-section");
        if (!sectionHost) {
            renderKeyPicker();
            return;
        }
        sectionHost.innerHTML = renderKeyPickerSection();
        hydrateTooltips();
        const input = sectionHost.querySelector("[data-picker-search]");
        if (input) {
            input.focus();
            input.setSelectionRange(input.value.length, input.value.length);
        }
    });

    window.addEventListener("message", (event) => {
        if (event.data.type === "model") {
            clearFloatingStatus();
            viewDrafts = {};
            model = event.data.model;
            Object.assign(qmkKeyLabels, model.qmkKeyLabels || {});
            Object.assign(qmkKeyAliases, model.qmkKeycodeAliases || {});
            macroPayloadKeycodes = new Set(model.macroPayloadKeycodes || []);
            notice = event.data.notice || "";
            layoutNotice = "";
            if (event.data.clearedRgbReusableGroupDraft) {
                rgbReusableGroupDraftName = "";
                rgbReusableGroupOriginalName = "";
            }
            if (event.data.appliedLayerChanges) {
                const appliedLayerNames = new Set(pendingLayerAdds.map((layer) => layer.name).concat(pendingLayerDeletes));
                pendingLayerAdds = [];
                pendingLayerDeletes = [];
                pendingLayoutEdits = Object.fromEntries(
                    Object.entries(pendingLayoutEdits).filter(([layerName]) => !appliedLayerNames.has(layerName))
                );
                layerAddOpen = false;
                layerDraftName = "";
            }
            if (event.data.activeLayer) {
                activeLayer = event.data.activeLayer;
            }
            reconcilePendingLayerStructure();
            normalizeActiveLayer();
            reconcilePendingLayoutEdits();
            normalizeLayoutComboState();
            normalizeMacroBuilderState();
            normalizeRgbGroupState();
            render();
            resetLocalHistory();
        }
        if (event.data.type === "error") {
            clearFloatingStatus();
            notice = event.data.message || "Unknown error";
            layoutNotice = "";
            render();
            resetLocalHistory();
        }
        if (event.data.type === "compileResult") {
            showFloatingStatus(event.data.error || event.data.notice || "Firmware compile finished.", Boolean(event.data.error));
        }
        if (event.data.type === "docsResult") {
            showFloatingStatus(event.data.error || event.data.notice || "Profile overview docs generated.", Boolean(event.data.error), "Profile overview");
        }
    });

    app.addEventListener("pointerdown", (event) => {
        const target = event.target.closest(".layout-board-svg .svg-key[data-index]");
        if (!canDragLayoutKey(event, target)) return;
        layoutDragState = {
            pointerId: event.pointerId,
            sourceIndex: Number(target.dataset.index),
            startX: event.clientX,
            startY: event.clientY,
            startSvg: layoutSvgPoint(target.ownerSVGElement, event.clientX, event.clientY),
            dragging: false,
            targetIndex: undefined
        };
        try {
            target.setPointerCapture?.(event.pointerId);
        } catch {
            // Pointer capture is a best-effort enhancement for SVG nodes.
        }
    });
    document.addEventListener("pointermove", updateLayoutKeyDrag);
    document.addEventListener("pointerup", finishLayoutKeyDrag);
    document.addEventListener("pointercancel", cancelLayoutKeyDrag);

    app.addEventListener("click", (event) => {
        if (suppressNextLayoutClick) {
            suppressNextLayoutClick = false;
            if (event.target.closest(".layout-board-svg .svg-key")) {
                event.preventDefault();
                event.stopPropagation();
                return;
            }
        }
        const target = event.target.closest("[data-action]");
        if (!target) return;
        const action = target.dataset.action;
        if (writeActions.has(action) && !validateWriteTarget(target)) {
            return;
        }
        if (action === "showAddLayerDraft") {
            const before = currentLocalSnapshot || serializeLocalState();
            layerAddOpen = true;
            layerDraftName = nextLayerDraftName();
            render();
            commitLocalHistory(before);
        } else if (action === "cancelLayerDraft") {
            const before = currentLocalSnapshot || serializeLocalState();
            layerAddOpen = false;
            layerDraftName = "";
            render();
            commitLocalHistory(before);
        } else if (action === "stageLayerDraft") {
            const before = currentLocalSnapshot || serializeLocalState();
            if (stageNewLayerDraft(target)) {
                commitLocalHistory(before);
            }
        } else if (action === "deleteLayerDraft") {
            const before = currentLocalSnapshot || serializeLocalState();
            if (stageDeleteActiveLayer()) {
                commitLocalHistory(before);
            }
        } else if (action === "discardLayerChanges") {
            const before = currentLocalSnapshot || serializeLocalState();
            discardLayerStructureDrafts();
            render();
            commitLocalHistory(before);
        } else if (action === "applyLayerChanges") {
            if (!hasPendingLayerChanges()) return;
            post({ type: "applyLayerChanges", ...layerStructurePayload(), activeLayer });
        } else if (action === "selectLayer") {
            activeLayer = target.dataset.layer;
            selectedKey = 0;
            activeBehaviorKeycode = "";
            layoutNotice = "";
            layoutComboPicking = false;
            layoutComboSelection = [];
            layoutComboOutput = "";
            layoutComboInputs = "";
            clearLayoutComboOriginal();
            syncRgbLayerOwnerToActiveLayer();
            lastLayoutKeyClick = { index: undefined, time: 0 };
            render();
            resetLocalHistory();
        } else if (action === "selectView") {
            storeActiveViewDraft();
            if (activeView !== target.dataset.view) {
                stopMacroRecording(true);
            }
            activeView = target.dataset.view;
            layoutNotice = "";
            lastLayoutKeyClick = { index: undefined, time: 0 };
            normalizeMacroBuilderState();
            render();
            resetLocalHistory();
        } else if (action === "selectMacroSlot") {
            if (target.dataset.keycode && target.dataset.keycode !== activeMacroKeycode) {
                stopMacroRecording(true);
                clearMacroRecorderSession(false);
            }
            const before = currentLocalSnapshot || serializeLocalState();
            captureActiveMacroDraft();
            activeMacroKeycode = target.dataset.keycode || activeMacroKeycode;
            normalizeMacroBuilderState();
            render();
            commitLocalHistory(before);
        } else if (action === "selectKey") {
            const index = Number(target.dataset.index);
            const now = Date.now();
            const isDoubleClick = lastLayoutKeyClick.index === index && now - lastLayoutKeyClick.time < 450;
            selectedKey = index;
            activeBehaviorKeycode = "";
            lastLayoutKeyClick = { index, time: isDoubleClick ? 0 : now };
            render();
            if (isDoubleClick) {
                openLayoutKeyPicker(index);
            }
            currentLocalSnapshot = serializeLocalState();
        } else if (action === "editComboOutputBehavior") {
            activeBehaviorKeycode = target.dataset.keycode || "";
            layoutComboPicking = false;
            lastLayoutKeyClick = { index: undefined, time: 0 };
            render();
            app.querySelector(".selected-behavior-editor")?.scrollIntoView({ block: "start", behavior: "smooth" });
            currentLocalSnapshot = serializeLocalState();
        } else if (action === "editLayoutCombo") {
            const combo = layerCombos(currentLayer()).find((row) => row.badge === target.dataset.badge);
            if (combo) {
                loadLayoutComboIntoBuilder(combo);
                render();
                document.getElementById("layoutComboBuilder")?.scrollIntoView({ block: "start", behavior: "smooth" });
                currentLocalSnapshot = serializeLocalState();
            }
        } else if (action === "toggleLayoutComboPicking") {
            const before = currentLocalSnapshot || serializeLocalState();
            captureLayoutComboBuilderInputs();
            layoutComboPicking = !layoutComboPicking;
            lastLayoutKeyClick = { index: undefined, time: 0 };
            render();
            commitLocalHistory(before);
        } else if (action === "toggleLayoutComboKey") {
            const before = currentLocalSnapshot || serializeLocalState();
            captureLayoutComboBuilderInputs();
            toggleLayoutComboKey(Number(target.dataset.index));
            syncLayoutComboInputsFromSelection();
            syncLayoutComboMatchFromInputs();
            render();
            commitLocalHistory(before);
        } else if (action === "clearLayoutComboSelection") {
            const before = currentLocalSnapshot || serializeLocalState();
            captureLayoutComboBuilderInputs();
            layoutComboSelection = [];
            layoutComboOutput = "";
            layoutComboInputs = "";
            clearLayoutComboOriginal();
            render();
            commitLocalHistory(before);
        } else if (action === "toggleRgbLed") {
            const before = currentLocalSnapshot || serializeLocalState();
            toggleRgbLed(Number(target.dataset.led));
            render();
            commitLocalHistory(before);
        } else if (action === "toggleRgbTrackball") {
            const before = currentLocalSnapshot || serializeLocalState();
            toggleRgbLed(56);
            render();
            commitLocalHistory(before);
        } else if (action === "clearRgbSelection") {
            const before = currentLocalSnapshot || serializeLocalState();
            rgbSelectedLeds = [];
            rgbLedGroupSource = "inline";
            render();
            commitLocalHistory(before);
        } else if (action === "editReusableLedGroup") {
            const before = currentLocalSnapshot || serializeLocalState();
            const group = rgbReusableLedGroupByName(target.dataset.ledGroup || "");
            if (group) {
                rgbReusableGroupOriginalName = group.name;
                rgbReusableGroupDraftName = group.name;
                rgbSelectedLeds = (group.ledIndices || []).map(Number).filter(Number.isInteger);
                rgbLedGroupSource = "inline";
            }
            render();
            commitLocalHistory(before);
        } else if (action === "useReusableLedGroup") {
            const before = currentLocalSnapshot || serializeLocalState();
            const group = rgbReusableLedGroupByName(target.dataset.ledGroup || "");
            if (group) {
                rgbLedGroupSource = group.name;
                rgbSelectedLeds = [];
            }
            render();
            commitLocalHistory(before);
        } else if (action === "clearReusableLedGroupDraft") {
            const before = currentLocalSnapshot || serializeLocalState();
            rgbReusableGroupOriginalName = "";
            rgbReusableGroupDraftName = "";
            render();
            commitLocalHistory(before);
        } else if (action === "saveRgbReusableLedGroup") {
            const form = document.getElementById("rgbReusableGroups");
            post({
                type: "saveRgbReusableLedGroup",
                group: readRgbReusableLedGroupDraft(form)
            });
        } else if (action === "deleteRgbReusableLedGroup") {
            post({
                type: "deleteRgbReusableLedGroup",
                name: target.dataset.ledGroup || ""
            });
        } else if (action === "openKeyPicker") {
            const pickerTarget = target.dataset.target;
            openKeyPicker(pickerTarget, target.dataset.mode || "single", pickerTarget === "keycodeInput" ? { layoutStageIndex: selectedKey } : {});
        } else if (action === "dismissStatus") {
            dismissedStatusSignature = currentStatusSignature();
            render();
        } else if (action === "insertMacroStep") {
            const before = currentLocalSnapshot || serializeLocalState();
            if (insertMacroStep(target)) {
                commitLocalHistory(before);
            }
        } else if (action === "clearMacroPayload") {
            const before = currentLocalSnapshot || serializeLocalState();
            if (clearMacroPayload(target)) {
                commitLocalHistory(before);
            }
        } else if (action === "startMacroRecording") {
            startMacroRecording(target);
        } else if (action === "stopMacroRecording") {
            stopMacroRecording(true);
        } else if (action === "clearMacroRecording") {
            const before = currentLocalSnapshot || serializeLocalState();
            if (clearMacroRecording(target)) {
                commitLocalHistory(before);
            }
        } else if (action === "applyKey") {
            const input = document.getElementById("keycodeInput");
            const before = currentLocalSnapshot || serializeLocalState();
            if (stageLayoutKey(selectedKey, input.value)) {
                render();
                commitLocalHistory(before);
            } else {
                render();
            }
        } else if (action === "updateLayerColor") {
            const card = target.closest(".card");
            post({
                type: "updateLayerColor",
                layer: card.dataset.layer,
                hue: value(card, "h"),
                sat: value(card, "s"),
                val: value(card, "v"),
                mode: value(card, "mode")
            });
        } else if (action === "updatePdModeColor") {
            const card = target.closest(".card");
            post({
                type: "updatePdModeColor",
                pointingMode: card.dataset.mode,
                hue: value(card, "h"),
                sat: value(card, "s"),
                val: value(card, "v"),
                locality: value(card, "locality")
            });
        } else if (action === "updateAutomouseFade") {
            const card = target.closest(".card");
            post({
                type: "updateAutomouseFade",
                mode: value(card, "mode"),
                hue: value(card, "h"),
                sat: value(card, "s"),
                val: value(card, "v")
            });
        } else if (action === "updateComboFeedback") {
            const card = target.closest(".card");
            post({
                type: "updateComboFeedback",
                hue: value(card, "h"),
                sat: value(card, "s"),
                val: value(card, "v"),
                locality: value(card, "locality")
            });
        } else if (action === "updateKeyBehaviorFeedback") {
            const card = document.getElementById("keyBehaviorFeedbackCard");
            post({
                type: "updateKeyBehaviorFeedback",
                config: {
                    tapPendingColor: readColorControl(card, "tapPendingColor"),
                    tapBranchColors: Array.from(card.querySelectorAll("[data-tap-branch-color]")).map(readColorControlFromNode),
                    branchConfirmMode: value(card, "branchConfirmMode"),
                    tapCommittedColor: readColorControl(card, "tapCommittedColor"),
                    holdActiveColor: readColorControl(card, "holdActiveColor"),
                    longHoldActiveColor: readColorControl(card, "longHoldActiveColor"),
                    tapCommitMode: value(card, "tapCommitMode"),
                    locality: value(card, "locality")
                }
            });
        } else if (action === "updateConfigDefaults") {
            const section = target.closest("[data-config-section]");
            post({
                type: "updateConfigDefaults",
                fields: readConfigDefaultFields(section)
            });
        } else if (action === "addRgbLedGroup") {
            const form = document.getElementById("rgbGroupBuilder");
            post({
                type: "addRgbLedGroup",
                group: readRgbLedGroupBuilder(form)
            });
        } else if (action === "updateViaMacro") {
            stopMacroRecording(true);
            clearMacroRecorderSession(false);
            const row = target.closest("[data-macro-editor]") || target.closest("tr");
            const payloadControl = row.querySelector("[data-macro-payload]") || row.querySelector("input, textarea");
            captureMacroDraftFromControl(payloadControl);
            post({
                type: "updateViaMacro",
                keycode: row.dataset.keycode,
                payload: payloadControl.value
            });
        } else if (action === "addCombo") {
            post({ type: "addCombo", ...readComboBuilder(target) });
        } else if (action === "addLayoutCombo") {
            const payload = readComboBuilder(target);
            const original = activeLayoutComboOriginal;
            const originalSource = activeLayoutComboOriginalSource;
            layoutComboPicking = false;
            layoutComboSelection = [];
            layoutComboOutput = "";
            layoutComboInputs = "";
            clearLayoutComboOriginal();
            post(layoutComboShouldSaveOriginal(original, originalSource, payload.inputs) ? { type: "saveCombo", ...payload, originalOutput: original.output, originalInputs: original.inputs.join(", ") } : { type: "addCombo", ...payload });
        } else if (action === "applyLayoutChanges") {
            const groups = pendingLayoutChangeGroups();
            const stagedEditLayer = groups.find((group) => pendingLayerAdd(group.layer));
            if (stagedEditLayer) {
                notice = "Use Apply layer changes to write staged key edits on " + stagedEditLayer.layer + ".";
                render();
                return;
            }
            const stagedLayer = pendingAddedLayerReference(groups.flatMap((group) => group.changes));
            if (stagedLayer) {
                notice = "Apply layer changes before writing layout keys that reference " + stagedLayer.name + ".";
                render();
                return;
            }
            if (groups.length) {
                post({ type: "updateLayoutKeys", layers: groups });
            }
        } else if (action === "addBehavior") {
            post({ type: "addBehavior", behavior: readBehaviorForm() });
        } else if (action === "saveSelectedBehavior") {
            post({ type: "saveBehavior", behavior: readSelectedBehaviorForm() });
        }
    });

    app.addEventListener("change", (event) => {
        const before = currentLocalSnapshot || serializeLocalState();
        const colorControl = event.target?.closest?.("[data-color-control]");
        if (colorControl) {
            syncColorControl(event, colorControl);
            validateControl(event.target);
            updateDirtyFromEvent(event);
            commitLocalHistory(before);
            return;
        }
        if (event.target?.name === "target" && event.target.closest("#rgbGroupBuilder")) {
            rgbGroupTarget = event.target.value;
            rgbGroupOwner = "";
            rgbBuilderColor = undefined;
            normalizeRgbGroupState();
            render();
            commitLocalHistory(before);
            return;
        }
        if (event.target?.name === "ledGroupSource" && event.target.closest("#rgbGroupBuilder")) {
            rgbLedGroupSource = event.target.value || "inline";
            if (rgbLedGroupSource !== "inline") {
                rgbSelectedLeds = [];
            }
            normalizeRgbGroupState();
            render();
            commitLocalHistory(before);
            return;
        }
        if (event.target?.name === "owner" && event.target.closest("#rgbGroupBuilder")) {
            rgbGroupOwner = event.target.value;
            if (rgbGroupTarget === "layer" && rgbGroupOwner && rgbGroupOwner !== rgbLayerAllGroups) {
                activeLayer = rgbGroupOwner;
                selectedKey = 0;
                activeBehaviorKeycode = "";
                layoutNotice = "";
                layoutComboPicking = false;
                layoutComboSelection = [];
                layoutComboOutput = "";
                layoutComboInputs = "";
                clearLayoutComboOriginal();
                lastLayoutKeyClick = { index: undefined, time: 0 };
            }
            rgbBuilderColor = undefined;
            render();
            commitLocalHistory(before);
            return;
        }
        if (event.target?.matches("[data-helper-enabled]")) {
            updateHelperFields(event.target.dataset.helperPrefix || event.target.id.replace(/Enabled$/, ""));
        }
        if (event.target?.matches("[data-helper-select]")) {
            updateHelperFields(event.target.dataset.helperPrefix || event.target.id.replace(/Helper$/, ""));
        }
        if (event.target?.id === "macroStepType") {
            updateMacroComposerFields(event.target.closest("[data-macro-workbench]") || document);
        }
        if (event.target?.closest?.("[data-macro-recorder]")) {
            captureMacroRecorderSettings();
            syncMacroRecordedPayloadDraft();
            refreshMacroRecorderPanel(event.target.closest("[data-macro-workbench]"));
            validateControl(event.target);
            if (!macroRecording) {
                commitLocalHistory(before);
            }
            return;
        }
        validateControl(event.target);
        updateDirtyFromEvent(event);
        commitLocalHistory(before);
    });

    app.addEventListener("input", (event) => {
        const before = currentLocalSnapshot || serializeLocalState();
        const control = event.target?.closest?.("[data-color-control]");
        if (control) syncColorControl(event, control);
        if (event.target?.id === "layoutComboOutput") {
            layoutComboOutput = event.target.value;
        }
        if (event.target?.id === "layoutComboInputs") {
            layoutComboInputs = event.target.value;
            syncLayoutComboSelectionFromInputs();
            syncLayoutComboMatchFromInputs();
            refreshLayoutComboSelection(event.target.closest("[data-combo-builder]"));
        }
        if (event.target?.id === "layerDraftName") {
            layerDraftName = event.target.value;
        }
        if (event.target?.id === "rgbReusableGroupName") {
            rgbReusableGroupDraftName = event.target.value;
        }
        if (event.target?.closest?.("[data-combo-builder]")) {
            updateLayoutComboClearState(event.target.closest("[data-combo-builder]"));
        }
        if (event.target?.matches("[data-macro-payload]")) {
            captureMacroDraftFromControl(event.target);
            refreshMacroPreview(event.target.closest("[data-macro-workbench]"));
        }
        if (event.target?.closest?.("[data-macro-composer]")) {
            validateControl(event.target);
            updateMacroComposerInsertState(event.target.closest("[data-macro-workbench]") || event.target.closest("[data-macro-composer]"));
            commitLocalHistory(before);
            return;
        }
        if (event.target?.closest?.("[data-macro-recorder]")) {
            captureMacroRecorderSettings();
            syncMacroRecordedPayloadDraft();
            validateControl(event.target);
            if (!macroRecording) {
                commitLocalHistory(before);
            }
            return;
        }
        validateControl(event.target);
        updateDirtyFromEvent(event);
        commitLocalHistory(before);
    });

    function syncColorControl(event, control) {
        if (event.target.matches("input[type='color'][data-color-picker]")) {
            const hsv = hexToHsv(event.target.value);
            if (hsv) {
                control.querySelector("[name='h']").value = String(hsv.h);
                control.querySelector("[name='s']").value = String(hsv.s);
                control.querySelector("[name='v']").value = String(hsv.v);
            }
        }
        updateColorControl(control);
        if (control.closest("#rgbGroupBuilder")) {
            rgbBuilderColor = {
                h: value(control, "h"),
                s: value(control, "s"),
                v: value(control, "v")
            };
            updateRgbSelectionPreview();
        }
    }

    function value(root, name) {
        return root.querySelector("[name='" + name + "']").value;
    }

    function readColorControl(root, id) {
        return readColorControlFromNode(root.querySelector("[data-color-id='" + id + "']"));
    }

    function readColorControlFromNode(control) {
        return {
            hue: value(control, "h"),
            sat: value(control, "s"),
            val: value(control, "v")
        };
    }

    function toggleRgbLed(ledIndex) {
        if (!Number.isInteger(ledIndex)) return;
        if (rgbBuilderUsesReusableGroup()) {
            rgbSelectedLeds = effectiveRgbBuilderLedIndices();
            rgbLedGroupSource = "inline";
        }
        if (rgbSelectedLeds.includes(ledIndex)) {
            rgbSelectedLeds = rgbSelectedLeds.filter((candidate) => candidate !== ledIndex);
            return;
        }
        rgbSelectedLeds = rgbSelectedLeds.concat([ledIndex]);
    }

    function normalizeLayoutComboState() {
        const layer = currentLayer();
        if (!layer) {
            layoutComboSelection = [];
            clearLayoutComboOriginal();
            return;
        }
        const valid = new Set(layer.positions.map((position) => position.layoutIndex));
        const seen = new Set();
        layoutComboSelection = layoutComboSelection.filter((index) => {
            if (!Number.isInteger(index) || !valid.has(index) || seen.has(index)) return false;
            seen.add(index);
            return true;
        });
        if (activeLayoutComboOriginal && !model.combos.some((combo) => comboIdentityEquals(combo, activeLayoutComboOriginal))) {
            clearLayoutComboOriginal();
        }
    }

    function normalizeMacroBuilderState() {
        const slots = model?.viaMacros || [];
        if (!slots.length) {
            activeMacroKeycode = "";
            macroDrafts = {};
            return;
        }

        const keys = new Set(slots.map((slot) => slot.keycode));
        if (!keys.has(activeMacroKeycode)) {
            activeMacroKeycode = (slots.find((slot) => !slot.empty) || slots[0]).keycode;
        }

        const nextDrafts = {};
        for (const [keycode, payload] of Object.entries(macroDrafts || {})) {
            if (!keys.has(keycode)) continue;
            const slot = slots.find((candidate) => candidate.keycode === keycode);
            if (slot && String(payload || "") !== String(slot.payload || "")) {
                nextDrafts[keycode] = String(payload || "");
            }
        }
        macroDrafts = nextDrafts;
    }

    function toggleLayoutComboKey(layoutIndex) {
        if (!Number.isInteger(layoutIndex)) return;
        normalizeLayoutComboState();
        if (layoutComboSelection.includes(layoutIndex)) {
            layoutComboSelection = layoutComboSelection.filter((candidate) => candidate !== layoutIndex);
            return;
        }
        layoutComboSelection = layoutComboSelection.concat([layoutIndex]);
    }

    function captureLayoutComboBuilderInputs() {
        const output = document.getElementById("layoutComboOutput");
        const inputs = document.getElementById("layoutComboInputs");
        if (output) layoutComboOutput = output.value;
        if (inputs) layoutComboInputs = inputs.value;
    }

    function updateLayoutComboClearState(builder) {
        const button = builder?.querySelector?.("[data-layout-combo-clear]");
        if (!button) return;
        button.disabled = !layoutComboBuilderHasClearableValue(builder);
    }

    function layoutComboBuilderHasClearableValue(builder) {
        const output = String(fieldValueFromMarker(builder || document, "[data-combo-output]") || "").trim();
        const inputs = String(fieldValueFromMarker(builder || document, "[data-combo-inputs]") || "").trim();
        return Boolean(output || inputs || layoutComboSelection.length);
    }

    function syncLayoutComboInputsFromSelection() {
        const layer = currentLayer();
        if (!layer) {
            layoutComboInputs = "";
            return;
        }
        layoutComboInputs = layoutComboSelectedPositions(layer).map((position) => position.keycode).join(", ");
    }

    function syncLayoutComboMatchFromInputs() {
        const combo = exactLayoutComboForInputs(layoutComboInputs);
        if (activeLayoutComboOriginalSource === "matched" && !comboIdentityEquals(combo, activeLayoutComboOriginal)) {
            clearLayoutComboOriginal();
        }
        if (combo && !activeLayoutComboOriginal) {
            layoutComboOutput = combo.output;
            setLayoutComboOriginal(comboIdentity(combo), "matched");
        }
    }

    function exactLayoutComboForInputs(inputs) {
        const layer = currentLayer();
        if (!layer) return undefined;
        const inputList = splitLayoutArguments(inputs || "");
        if (inputList.length < 2) return undefined;
        const signature = comboInputSignature(inputList);
        return layerCombos(layer).find((combo) => comboInputSignature(combo.inputs) === signature);
    }

    function comboIdentity(combo) {
        return combo ? {output: combo.output, inputs: (combo.inputs || []).slice()} : undefined;
    }

    function normalizeLayoutComboOriginal(value) {
        if (!value || typeof value !== "object") return undefined;
        const inputs = Array.isArray(value.inputs) ? value.inputs.map(normalizeDisplayExpression).filter(Boolean) : [];
        const output = normalizeDisplayExpression(value.output || "");
        return output && inputs.length ? {output, inputs} : undefined;
    }

    function normalizeLayoutComboOriginalSource(value, original) {
        if (!original) return "";
        return value === "matched" ? "matched" : "explicit";
    }

    function setLayoutComboOriginal(original, source) {
        activeLayoutComboOriginal = original;
        activeLayoutComboOriginalSource = normalizeLayoutComboOriginalSource(source, original);
    }

    function clearLayoutComboOriginal() {
        activeLayoutComboOriginal = undefined;
        activeLayoutComboOriginalSource = "";
    }

    function layoutComboShouldSaveOriginal(original, source, inputs) {
        if (!original) return false;
        if (source !== "matched") return true;
        return comboInputSignature(splitLayoutArguments(inputs || "")) === comboInputSignature(original.inputs);
    }

    function comboIdentityEquals(combo, identity) {
        return Boolean(combo && identity && keyExpressionsEquivalent(combo.output, identity.output) && comboInputSignature(combo.inputs) === comboInputSignature(identity.inputs));
    }

    function comboInputSignature(inputs) {
        return (inputs || []).map(canonicalKeyExpression).filter(Boolean).sort().join("\\u0000");
    }

    function loadLayoutComboIntoBuilder(combo) {
        if (!combo) return;
        layoutComboOutput = combo.output || "";
        layoutComboInputs = (combo.inputs || []).join(", ");
        setLayoutComboOriginal(comboIdentity(combo), "explicit");
        syncLayoutComboSelectionFromInputs();
        layoutComboPicking = false;
    }

    function syncLayoutComboSelectionFromInputs() {
        const layer = currentLayer();
        if (!layer) return;
        const unusedPositions = layer.positions.slice();
        layoutComboSelection = splitLayoutArguments(layoutComboInputs)
            .map((part) => part.trim())
            .filter(Boolean)
            .map((keycode) => {
                const canonical = canonicalLayoutKeyExpression(keycode);
                if (!canonical || layoutKeyExpressionError(canonical)) return undefined;
                const index = unusedPositions.findIndex((position) => layoutKeyEquivalent(position.keycode, canonical));
                if (index === -1) return undefined;
                const [position] = unusedPositions.splice(index, 1);
                return position.layoutIndex;
            })
            .filter((index) => Number.isInteger(index));
    }

    function refreshLayoutComboSelection(builder) {
        refreshLayoutComboHighlights();
        refreshLayoutComboSelectedList(builder);
    }

    function refreshLayoutComboHighlights() {
        const selected = new Set(layoutComboSelection);
        for (const key of document.querySelectorAll(".layout-board-svg .svg-key[data-index]")) {
            key.classList.toggle("combo-input-selected", selected.has(Number(key.dataset.index)));
        }
    }

    function refreshLayoutComboSelectedList(builder) {
        const layer = currentLayer();
        const list = builder?.querySelector?.("[data-layout-combo-selected-list]");
        if (!layer || !list) return;
        list.innerHTML = renderLayoutComboSelectedList(layer);
    }

    function currentLayoutPosition(layoutIndex = selectedKey) {
        const layer = currentLayer();
        if (!layer) return undefined;
        return layerPositionByLayoutIndex(layer, layoutIndex);
    }

    function layerPositionByLayoutIndex(layer, layoutIndex) {
        if (!layer) return undefined;
        const positions = layer.positions || [];
        return positions.find((position) => position.layoutIndex === layoutIndex) || positions[layoutIndex];
    }

    function layersForUi() {
        const deleted = new Set(pendingLayerDeletes);
        return (model.layers || [])
            .filter((layer) => !deleted.has(layer.name))
            .concat(pendingLayerAdds);
    }

    function pendingLayerAdd(name) {
        return pendingLayerAdds.find((layer) => layer.name === name);
    }

    function originalLayer(name) {
        return (model.layers || []).find((layer) => layer.name === name);
    }

    function baseLayer(layerName = activeLayer) {
        return layersForUi().find((layer) => layer.name === layerName) || layersForUi()[0];
    }

    function reconcilePendingLayerStructure() {
        const modelNames = new Set((model.layers || []).map((layer) => layer.name));
        const deleteNames = new Set();
        pendingLayerDeletes = (Array.isArray(pendingLayerDeletes) ? pendingLayerDeletes : [])
            .map(normalizeLayerNameInput)
            .filter((name) => {
                if (!name || name === "LAYER_BASE" || !modelNames.has(name) || deleteNames.has(name)) return false;
                deleteNames.add(name);
                return true;
            });

        const addNames = new Set();
        pendingLayerAdds = (Array.isArray(pendingLayerAdds) ? pendingLayerAdds : []).filter((layer) => {
            const name = normalizeLayerNameInput(layer?.name);
            if (!name || modelNames.has(name) || addNames.has(name) || deleteNames.has(name)) return false;
            if (!Array.isArray(layer.positions) || layer.positions.length !== layoutSlotCount) return false;
            layer.name = name;
            layer.pendingAdd = true;
            if (!layer.rgbColor) layer.rgbColor = randomLayerColor();
            addNames.add(name);
            return true;
        });
    }

    function normalizeActiveLayer() {
        const layers = layersForUi();
        if (!layers.length) {
            activeLayer = "";
            selectedKey = 0;
            activeBehaviorKeycode = "";
            return;
        }
        if (!layers.some((layer) => layer.name === activeLayer)) {
            activeLayer = layers[0].name;
            activeBehaviorKeycode = "";
        }
        const layer = layers.find((candidate) => candidate.name === activeLayer) || layers[0];
        const positions = layer.positions || [];
        if (!Number.isInteger(selectedKey) || selectedKey < 0 || selectedKey >= positions.length) {
            selectedKey = 0;
            activeBehaviorKeycode = "";
        }
    }

    function layerPendingLayoutEdits(layerName = activeLayer) {
        return pendingLayoutEdits[layerName] || {};
    }

    function pendingLayoutChanges(layerName = activeLayer) {
        const base = baseLayer(layerName);
        if (!base) return [];
        const byIndex = layerPendingLayoutEdits(layerName);
        return Object.keys(byIndex)
            .map((key) => Number(key))
            .filter((layoutIndex) => Number.isInteger(layoutIndex))
            .map((layoutIndex) => ({ layoutIndex, keycode: byIndex[layoutIndex] }))
            .filter((change) => {
                const original = base.positions.find((position) => position.layoutIndex === change.layoutIndex);
                return original && change.keycode && !layoutKeyEquivalent(change.keycode, original.keycode);
            })
            .sort((left, right) => left.layoutIndex - right.layoutIndex);
    }

    function pendingLayoutChangeGroups() {
        return Object.keys(pendingLayoutEdits)
            .map((layer) => ({ layer, changes: pendingLayoutChanges(layer) }))
            .filter((group) => group.changes.length)
            .sort((left, right) => layerOrderIndex(left.layer) - layerOrderIndex(right.layer));
    }

    function pendingLayoutChangeCount() {
        return pendingLayoutChangeGroups().reduce((count, group) => count + group.changes.length, 0);
    }

    function layerOrderIndex(layerName) {
        const index = layersForUi().findIndex((layer) => layer.name === layerName);
        return index === -1 ? Number.MAX_SAFE_INTEGER : index;
    }

    function stageLayoutKey(layoutIndex, keycode, layerName = activeLayer) {
        const base = baseLayer(layerName);
        const original = base?.positions.find((position) => position.layoutIndex === layoutIndex);
        const value = canonicalLayoutKeyExpression(keycode);
        const error = layoutKeyExpressionError(value);
        if (!base || !original || error) {
            if (error) layoutNotice = error;
            return false;
        }
        layoutNotice = "";
        const nextLayerEdits = { ...layerPendingLayoutEdits(layerName) };
        if (layoutKeyEquivalent(value, original.keycode)) {
            delete nextLayerEdits[layoutIndex];
        } else {
            nextLayerEdits[layoutIndex] = value;
        }
        pendingLayoutEdits = { ...pendingLayoutEdits, [layerName]: nextLayerEdits };
        if (!Object.keys(nextLayerEdits).length) {
            delete pendingLayoutEdits[layerName];
        }
        return true;
    }

    function stageLayoutSwap(sourceIndex, targetIndex) {
        const source = currentLayoutPosition(sourceIndex);
        const target = currentLayoutPosition(targetIndex);
        if (!source || !target || sourceIndex === targetIndex) return false;
        return stageLayoutKey(sourceIndex, target.keycode) && stageLayoutKey(targetIndex, source.keycode);
    }

    function reconcilePendingLayoutEdits() {
        const next = {};
        for (const [layerName, edits] of Object.entries(pendingLayoutEdits)) {
            const base = baseLayer(layerName);
            if (!base) continue;
            const layerEdits = {};
            for (const [index, keycode] of Object.entries(edits || {})) {
                const layoutIndex = Number(index);
                const original = base.positions.find((position) => position.layoutIndex === layoutIndex);
                const value = canonicalLayoutKeyExpression(keycode);
                if (original && value && !layoutKeyExpressionError(value) && !layoutKeyEquivalent(value, original.keycode)) {
                    layerEdits[index] = value;
                }
            }
            if (Object.keys(layerEdits).length) {
                next[layerName] = layerEdits;
            }
        }
        pendingLayoutEdits = next;
    }

    function layoutKeyEquivalent(left, right) {
        return canonicalLayoutKeyExpression(left) === canonicalLayoutKeyExpression(right);
    }

    function canonicalLayoutKeyExpression(value) {
        const normalized = normalizeDisplayExpression(value);
        if (!normalized) return "";
        const alias = userKeyAliases[normalized] || userKeyAliases[normalized.toLowerCase()];
        if (alias) return alias;
        const qmkAlias = qmkKeyAliases[normalized];
        if (qmkAlias) return qmkAlias;
        if (qmkKeyLabels[normalized]) return normalized;
        const chord = canonicalLayoutChordExpression(normalized);
        if (chord) return chord;
        const match = Object.entries(qmkKeyLabels).find(([, label]) => String(label || "").toLowerCase() === normalized.toLowerCase());
        if (match) return match[0];
        if (/^[a-z]$/i.test(normalized)) return "KC_" + normalized.toUpperCase();
        if (/^\\d$/.test(normalized)) return "KC_" + normalized;
        if (/^[a-z][a-z0-9_]*$/i.test(normalized) && normalized.includes("_")) return normalized.toUpperCase();
        return normalized;
    }

    function authoredInternalKeyExpression(value) {
        const normalized = normalizeDisplayExpression(value);
        const alias = userKeyAliases[normalized] || userKeyAliases[normalized.toLowerCase()];
        return alias === "_______" || alias === "XXXXXXX" ? alias : "";
    }

    function layoutKeyExpressionError(value) {
        const normalized = normalizeDisplayExpression(value);
        if (!normalized) {
            return "Layout slots expect one keycode expression.";
        }
        if (hasTopLevelLayoutDelimiter(normalized, ",")) {
            return "Layout slots accept one keycode expression. Use combo inputs for comma-separated key lists.";
        }
        if (!isLayoutKeyExpression(normalized)) {
            return "Layout slots expect a keycode, alias, or QMK key expression.";
        }
        return "";
    }

    function isLayoutKeyExpression(value) {
        if (value === "_______" || value === "XXXXXXX") return true;
        if (/^[A-Z_][A-Z0-9_]*$/.test(value)) {
            return Boolean(qmkKeyLabels[value]) || value.includes("_") || knownLayoutKeyTokens().has(value);
        }
        return isLayoutKeyCallExpression(value);
    }

    function isLayoutKeyCallExpression(value) {
        const match = String(value || "").match(/^([A-Z][A-Z0-9_]*)\\s*\\(/);
        if (!match) return false;
        const open = value.indexOf("(", match[1].length);
        const close = matchingLayoutParenIndex(value, open);
        if (close !== value.length - 1) return false;
        const name = match[1];
        return layoutKeyCallFunctions.has(name) || Boolean(modWrapperLabels[name]) || name.includes("_") || knownLayoutKeyTokens().has(name);
    }

    function knownLayoutKeyTokens() {
        const tokens = new Set(Object.keys(qmkKeyLabels));
        tokens.add("_______");
        tokens.add("XXXXXXX");
        for (const layer of layersForUi()) {
            for (const position of layer.positions || []) {
                collectLayoutKeyTokens(position.keycode, tokens);
            }
        }
        return tokens;
    }

    function collectLayoutKeyTokens(expression, tokens) {
        for (const match of String(expression || "").matchAll(/\\b[A-Z][A-Z0-9_]*\\b/g)) {
            tokens.add(match[0]);
        }
    }

    function hasTopLevelLayoutDelimiter(text, delimiter) {
        let depthParen = 0;
        let depthBrace = 0;
        let depthBracket = 0;
        let quote = "";
        let escaped = false;

        for (let index = 0; index < text.length; index += 1) {
            const char = text[index];
            if (quote) {
                if (escaped) escaped = false;
                else if (char === "\\\\") escaped = true;
                else if (char === quote) quote = "";
                continue;
            }
            if (char === '"' || char === "'") {
                quote = char;
                continue;
            }
            if (char === "(") depthParen += 1;
            else if (char === ")") depthParen -= 1;
            else if (char === "{") depthBrace += 1;
            else if (char === "}") depthBrace -= 1;
            else if (char === "[") depthBracket += 1;
            else if (char === "]") depthBracket -= 1;
            else if (char === delimiter && depthParen === 0 && depthBrace === 0 && depthBracket === 0) {
                return true;
            }
        }
        return false;
    }

    function matchingLayoutParenIndex(text, openIndex) {
        if (openIndex < 0 || text[openIndex] !== "(") return -1;
        let depth = 0;
        let quote = "";
        let escaped = false;
        for (let index = openIndex; index < text.length; index += 1) {
            const char = text[index];
            if (quote) {
                if (escaped) escaped = false;
                else if (char === "\\\\") escaped = true;
                else if (char === quote) quote = "";
                continue;
            }
            if (char === '"' || char === "'") {
                quote = char;
                continue;
            }
            if (char === "(") depth += 1;
            else if (char === ")") {
                depth -= 1;
                if (depth === 0) return index;
            }
        }
        return -1;
    }

    function canonicalLayoutChordExpression(value) {
        const parts = String(value || "").split("+").map((part) => part.trim()).filter(Boolean);
        if (parts.length < 2) return "";
        const key = canonicalLayoutKeyExpression(parts[parts.length - 1]);
        if (!key) return "";
        const modifiers = [];
        for (const part of parts.slice(0, -1)) {
            const modifier = normalizeLayoutModifier(part);
            if (!modifier) return "";
            if (!modifiers.includes(modifier)) modifiers.push(modifier);
        }
        const wrapper = wrapperForLayoutModifiers(modifiers);
        if (wrapper) return wrapper + "(" + key + ")";
        const wrappers = wrappersForLayoutModifiers(modifiers);
        if (wrappers.length !== modifiers.length) return "";
        return wrappers.reduceRight((expression, candidate) => candidate + "(" + expression + ")", key);
    }

    function normalizeLayoutModifier(value) {
        const normalized = String(value || "").toLowerCase().replace(/[\\s_-]+/g, "");
        return {
            ctrl: "Ctrl",
            control: "Ctrl",
            lctrl: "Ctrl",
            leftctrl: "Ctrl",
            shift: "Shift",
            lshift: "Shift",
            leftshift: "Shift",
            alt: "Alt",
            option: "Alt",
            lalt: "Alt",
            leftalt: "Alt",
            cmd: "Cmd",
            command: "Cmd",
            gui: "Cmd",
            win: "Cmd",
            meta: "Cmd",
            lgui: "Cmd",
            leftgui: "Cmd",
            rctrl: "Right Ctrl",
            rightctrl: "Right Ctrl",
            rshift: "Right Shift",
            rightshift: "Right Shift",
            rgui: "Right Cmd",
            rightgui: "Right Cmd",
            rightcmd: "Right Cmd",
            ralt: "Right Alt",
            rightalt: "Right Alt"
        }[normalized] || "";
    }

    function wrapperForLayoutModifiers(modifiers) {
        const key = modifiers.map(normalizeLayoutModifier).filter(Boolean).sort().join("+");
        for (const [wrapper, labels] of Object.entries(modWrapperLabels)) {
            const candidate = labels.map(normalizeLayoutModifier).filter(Boolean).sort().join("+");
            if (candidate === key) return wrapper;
        }
        return "";
    }

    function wrappersForLayoutModifiers(modifiers) {
        return modifiers.map((modifier) => {
            switch (normalizeLayoutModifier(modifier)) {
                case "Ctrl": return "C";
                case "Shift": return "S";
                case "Alt": return "A";
                case "Cmd": return "G";
                case "Right Ctrl": return "RCTL";
                case "Right Shift": return "RSFT";
                case "Right Alt": return "RALT";
                case "Right Cmd": return "RGUI";
                default: return "";
            }
        }).filter(Boolean);
    }

    function canUseLayoutClipboard(target) {
        if (keyPicker || activeView !== "layout" || !currentLayer()) return false;
        if (isEditableTarget(target)) return false;
        return Boolean(currentLayoutPosition());
    }

    function isEditableTarget(target) {
        return Boolean(target?.closest?.("input, textarea, select, [contenteditable='true']"));
    }

    function copySelectedLayoutKey() {
        const position = currentLayoutPosition();
        if (!position?.keycode) return "";
        copiedLayoutKey = position.keycode;
        navigator.clipboard?.writeText(copiedLayoutKey)?.catch(() => {});
        return copiedLayoutKey;
    }

    function pasteLayoutKey(keycode) {
        const position = currentLayoutPosition();
        const value = String(keycode || "").trim();
        if (!position || !value) return false;
        if (!stageLayoutKey(position.layoutIndex, value)) {
            render();
            return false;
        }
        render();
        return true;
    }

    function stageNewLayerDraft(target) {
        const card = target.closest("[data-layer-flow]") || document;
        const input = card.querySelector("#layerDraftName");
        const name = normalizeLayerNameInput(input?.value || layerDraftName);
        if (!name) {
            notice = "Enter a layer name such as LAYER_MEDIA.";
            render();
            return false;
        }
        if (!/^LAYER_[A-Z0-9_]+$/.test(name)) {
            notice = "Layer names must use letters, numbers, and underscores.";
            render();
            return false;
        }
        if (layersForUi().some((layer) => layer.name === name)) {
            notice = name + " already exists.";
            render();
            return false;
        }
        if (pendingLayerDeletes.includes(name)) {
            notice = name + " is already staged for deletion.";
            render();
            return false;
        }

        pendingLayerAdds = pendingLayerAdds.concat([createTransparentLayerDraft(name)]);
        layerAddOpen = false;
        layerDraftName = "";
        activeLayer = name;
        selectedKey = 0;
        activeBehaviorKeycode = "";
        layoutComboPicking = false;
        layoutComboSelection = [];
        layoutComboOutput = "";
        layoutComboInputs = "";
        clearLayoutComboOriginal();
        notice = "Staged " + name + ". Use Apply layer changes to write config.h, keymap.c, and rgb_config.c.";
        render();
        return true;
    }

    function stageDeleteActiveLayer() {
        if (!activeLayer) return false;
        if (activeLayer === "LAYER_BASE") {
            notice = "LAYER_BASE cannot be deleted.";
            render();
            return false;
        }

        const deleteName = activeLayer;
        const added = pendingLayerAdd(deleteName);
        if (added) {
            pendingLayerAdds = pendingLayerAdds.filter((layer) => layer.name !== deleteName);
            delete pendingLayoutEdits[deleteName];
            notice = "Discarded staged layer " + deleteName + ".";
        } else if (!pendingLayerDeletes.includes(deleteName)) {
            pendingLayerDeletes = pendingLayerDeletes.concat([deleteName]);
            notice = "Staged deletion of " + deleteName + ". Use Apply layer changes to write source files.";
        }

        const nextLayers = layersForUi();
        activeLayer = nextLayers[0]?.name || "";
        selectedKey = 0;
        activeBehaviorKeycode = "";
        layoutComboPicking = false;
        layoutComboSelection = [];
        layoutComboOutput = "";
        layoutComboInputs = "";
        clearLayoutComboOriginal();
        render();
        return true;
    }

    function discardLayerStructureDrafts() {
        const addedNames = new Set(pendingLayerAdds.map((layer) => layer.name));
        pendingLayerAdds = [];
        pendingLayerDeletes = [];
        pendingLayoutEdits = Object.fromEntries(
            Object.entries(pendingLayoutEdits).filter(([layerName]) => !addedNames.has(layerName) && originalLayer(layerName))
        );
        layerAddOpen = false;
        layerDraftName = "";
        normalizeActiveLayer();
        notice = "Discarded staged layer changes.";
    }

    function layerStructurePayload() {
        return {
            adds: pendingLayerAdds.map((layer) => ({
                name: layer.name,
                keycodes: Array.from({ length: layoutSlotCount }, (_, layoutIndex) => {
                    const position = layer.positions.find((candidate) => candidate.layoutIndex === layoutIndex) || layer.positions[layoutIndex];
                    const edit = pendingLayoutEdits[layer.name]?.[layoutIndex];
                    return edit || position?.keycode || "_______";
                }),
                color: layer.rgbColor,
            })),
            deletes: pendingLayerDeletes.slice(),
        };
    }

    function applyAllStagedChanges() {
        if (!hasApplyAllChanges()) return;
        post({
            type: "applyAllChanges",
            ...layerStructurePayload(),
            layoutGroups: pendingLayoutChangeGroups(),
            activeLayer,
        });
    }

    function messageWithActiveProfile(message) {
        const activeProfileId = model?.activeProfile?.id || "";
        return activeProfileId && !message.profileId ? { ...message, profileId: activeProfileId } : message;
    }

    function profileDocsPostMessage(message) {
        const payload = messageWithActiveProfile(message);
        showFloatingStatus("Generating profile overview doc...", false, "Profile overview");
        vscode.postMessage(payload);
    }

    function compilePostMessage(message) {
        const payload = messageWithActiveProfile(message);
        showFloatingStatus("Compiling left and right firmware...", false);
        vscode.postMessage(payload);
    }

    async function requestProfileDocsGeneration() {
        if (!model?.activeProfile?.editable) return;
        captureActiveMacroDraft();
        captureLayoutComboBuilderInputs();
        const summary = compileUnsavedSummary();
        if (!summary.hasUnsaved) {
            profileDocsPostMessage({ type: "generateProfileDocs", activeLayer });
            return;
        }

        const choice = await showProfileDocsConfirmDialog(summary);
        if (choice === "apply") {
            profileDocsPostMessage({
                type: "applyAllChangesAndGenerateProfileDocs",
                ...layerStructurePayload(),
                layoutGroups: pendingLayoutChangeGroups(),
                activeLayer,
            });
        } else if (choice === "saved") {
            profileDocsPostMessage({ type: "generateProfileDocs", activeLayer });
        }
    }

    async function requestFirmwareCompile() {
        if (!model?.activeProfile?.buildable) return;
        captureActiveMacroDraft();
        captureLayoutComboBuilderInputs();
        const summary = compileUnsavedSummary();
        if (!summary.hasUnsaved) {
            compilePostMessage({ type: "compileFirmware", activeLayer });
            return;
        }

        const choice = await showCompileConfirmDialog(summary);
        if (choice === "apply") {
            compilePostMessage({
                type: "applyAllChangesAndCompile",
                ...layerStructurePayload(),
                layoutGroups: pendingLayoutChangeGroups(),
                activeLayer,
            });
        } else if (choice === "saved") {
            compilePostMessage({ type: "compileFirmware", activeLayer });
        }
    }

    function compileUnsavedSummary() {
        const staged = [];
        if (pendingLayerAdds.length) {
            staged.push("Add " + pendingLayerAdds.length + " layer " + plural(pendingLayerAdds.length, "draft", "drafts") + ": " + pendingLayerAdds.map((layer) => layer.name).join(", "));
        }
        if (pendingLayerDeletes.length) {
            staged.push("Delete " + pendingLayerDeletes.length + " layer " + plural(pendingLayerDeletes.length, "draft", "drafts") + ": " + pendingLayerDeletes.join(", "));
        }
        for (const group of pendingLayoutChangeGroups()) {
            staged.push(group.layer + ": " + group.changes.length + " staged layout " + plural(group.changes.length, "key", "keys"));
        }
        const local = dirtySectionSummaries();
        return {
            staged,
            local,
            canApplyAll: staged.length > 0,
            hasUnsaved: staged.length > 0 || local.length > 0,
        };
    }

    function dirtySectionSummaries() {
        const labels = [];
        const seen = new Set();
        const add = (label) => addDirtySummaryLabel(labels, seen, label);
        for (const label of currentDirtySectionLabels()) {
            add(label);
        }
        for (const [viewId, draft] of Object.entries(viewDrafts || {})) {
            if (viewId === activeView || !draft?.dirty) continue;
            const draftLabels = Array.isArray(draft.labels) ? draft.labels : [];
            if (draftLabels.length) {
                for (const label of draftLabels) {
                    add(label);
                }
            } else {
                add(viewLabel(viewId) + " form edits");
            }
        }
        for (const keycode of Object.keys(macroDrafts || {})) {
            if (macroSlotDirty(keycode)) {
                add("Macro " + keycode);
            }
        }
        return labels;
    }

    function currentDirtySectionLabels() {
        const labels = [];
        const seen = new Set();
        for (const section of document.querySelectorAll("[data-dirty-section].dirty")) {
            if (section.closest("[hidden]")) continue;
            addDirtySummaryLabel(labels, seen, dirtySectionLabel(section));
        }
        return labels;
    }

    function addDirtySummaryLabel(labels, seen, label) {
        const clean = String(label || "").replace(/\\s+/g, " ").trim();
        if (!clean || seen.has(clean)) return;
        seen.add(clean);
        labels.push(clean);
    }

    function viewLabel(viewId) {
        const view = views.find(([id]) => id === viewId);
        return view?.[1] || viewId || "View";
    }

    function plural(count, singular, pluralValue) {
        return count === 1 ? singular : pluralValue;
    }

    function dirtySectionLabel(section) {
        if (section.id === "layoutComboBuilder") return "Layout combo builder";
        if (section.id === "rgbGroupBuilder") return "RGB LED group builder";
        if (section.id === "rgbReusableGroups") return "Reusable RGB LED groups";
        if (section.id === "keyBehaviorFeedbackCard") return "Key behavior feedback";
        if (section.matches?.("[data-macro-editor]")) return "Macro " + (section.dataset.keycode || "slot");
        if (section.dataset.configSection) {
            const heading = section.querySelector("h2, h3, summary")?.textContent;
            return "Defaults: " + (heading || section.dataset.configSection);
        }
        if (section.dataset.layer) return "RGB layer " + section.dataset.layer;
        if (section.dataset.mode) return "RGB pointing mode " + section.dataset.mode;
        const action = section.querySelector("[data-dirty-button]")?.dataset.action || "";
        if (action === "applyKey") return "Selected layout key editor";
        if (action === "saveSelectedBehavior") return "Selected behavior editor";
        const heading = section.querySelector("h2, h3, summary")?.textContent;
        if (heading) return heading;
        const button = section.querySelector("[data-dirty-button]");
        return button?.dataset.cleanLabel || button?.textContent || "Unsaved form";
    }

    function showProfileDocsConfirmDialog(summary) {
        return showUnsavedActionDialog(summary, {
            titleId: "profileDocsConfirmTitle",
            diskNotice: "Profile overview generation reads the profile source files currently on disk.",
            applyDescription: "Apply all staged and generate writes staged layer and layout changes before creating the overview doc.",
            localNote: "Local form edits are not written by the header Apply all action. Use each card's Apply button first if those edits should be included in the overview doc.",
            applyLabel: "Apply all staged and generate",
            savedLabel: "Keep as is and generate",
        });
    }

    function showCompileConfirmDialog(summary) {
        return showUnsavedActionDialog(summary, {
            titleId: "compileConfirmTitle",
            diskNotice: "Firmware compile reads the profile source files currently on disk.",
            applyDescription: "Apply all staged and compile writes staged layer and layout changes before building both firmware files.",
            localNote: "Local form edits are not written by the header Apply all action. Use each card's Apply button first if those edits should be compiled.",
            applyLabel: "Apply all staged and compile",
            savedLabel: "Keep as is and compile",
        });
    }

    function showUnsavedActionDialog(summary, action) {
        return new Promise((resolve) => {
            const backdrop = document.createElement("div");
            backdrop.className = "modal-backdrop";
            const stagedList = summary.staged.length
                ? "<h3>Apply all can write</h3><ul class='confirm-list'>" + summary.staged.map((item) => "<li>" + escapeHtml(item) + "</li>").join("") + "</ul>"
                : "";
            const localList = summary.local.length
                ? "<h3>Local form edits</h3><ul class='confirm-list'>" + summary.local.map((item) => "<li>" + escapeHtml(item) + "</li>").join("") + "</ul>"
                : "";
            const localNote = summary.local.length
                ? "<p>" + escapeHtml(action.localNote) + "</p>"
                : "";
            backdrop.innerHTML =
                "<div class='confirm-dialog' role='dialog' aria-modal='true' aria-labelledby='" + escapeHtml(action.titleId) + "'>" +
                "<h2 id='" + escapeHtml(action.titleId) + "'>Unsaved Studio changes</h2>" +
                "<p>" + escapeHtml(action.diskNotice) + "</p>" +
                (summary.canApplyAll ? "<p>" + escapeHtml(action.applyDescription) + "</p>" : "") +
                localNote +
                stagedList +
                localList +
                "<div class='confirm-actions'>" +
                (summary.canApplyAll ? "<button type='button' class='primary' data-choice='apply'>" + escapeHtml(action.applyLabel) + "</button>" : "") +
                "<button type='button' data-choice='saved'>" + escapeHtml(action.savedLabel) + "</button>" +
                "<button type='button' data-choice='cancel'>Cancel</button>" +
                "</div>" +
                "</div>";
            const close = (choice) => {
                document.removeEventListener("keydown", onKeyDown);
                backdrop.remove();
                resolve(choice);
            };
            const onKeyDown = (event) => {
                if (event.key === "Escape") close("cancel");
            };
            backdrop.addEventListener("click", (event) => {
                if (event.target === backdrop) close("cancel");
                const choice = event.target?.closest?.("[data-choice]")?.dataset.choice;
                if (choice) close(choice);
            });
            document.addEventListener("keydown", onKeyDown);
            document.body.appendChild(backdrop);
            backdrop.querySelector("[data-choice]")?.focus();
        });
    }

    function createTransparentLayerDraft(name) {
        return {
            name,
            pendingAdd: true,
            rgbColor: randomLayerColor(),
            positions: Array.from({ length: layoutSlotCount }, (_, index) => ({
                layoutIndex: index,
                keycode: "_______",
                display: displayKeyExpression("_______"),
                editLabel: displayKeyExpression("_______"),
            })),
        };
    }

    function randomLayerColor() {
        return {
            h: String(Math.floor(Math.random() * 256)),
            s: "255",
            v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS",
            mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY",
        };
    }

    function normalizeLayerNameInput(value) {
        const normalized = String(value || "").trim().toUpperCase().replace(/[^A-Z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
        if (!normalized) return "";
        return normalized.startsWith("LAYER_") ? normalized : "LAYER_" + normalized;
    }

    function nextLayerDraftName() {
        let index = 1;
        let name = "LAYER_NEW";
        const used = new Set(layersForUi().map((layer) => layer.name).concat(pendingLayerDeletes));
        while (used.has(name)) {
            index += 1;
            name = "LAYER_NEW_" + index;
        }
        return name;
    }

    function hasPendingLayerChanges() {
        return Boolean(pendingLayerAdds.length || pendingLayerDeletes.length);
    }

    function pendingAddedLayerReference(changes) {
        return pendingLayerAdds.find((layer) =>
            changes.some((change) => expressionReferencesToken(change.keycode, layer.name))
        );
    }

    function expressionReferencesToken(expression, token) {
        const escaped = String(token || "").replace(/[-/\\^$*+?.()|[\]{}]/g, "\\$&");
        return new RegExp("\\b" + escaped + "\\b").test(String(expression || ""));
    }

    function canDragLayoutKey(event, target) {
        if (!target || activeView !== "layout" || layoutComboPicking || keyPicker) return false;
        if (event.button !== 0 || event.ctrlKey || event.metaKey || event.altKey) return false;
        return target.dataset.action === "selectKey" && Number.isInteger(Number(target.dataset.index));
    }

    function updateLayoutKeyDrag(event) {
        if (!layoutDragState || event.pointerId !== layoutDragState.pointerId) return;
        const dx = event.clientX - layoutDragState.startX;
        const dy = event.clientY - layoutDragState.startY;
        if (!layoutDragState.dragging && Math.hypot(dx, dy) < 6) return;
        if (!layoutDragState.dragging) {
            layoutDragState.dragging = true;
            lastLayoutKeyClick = { index: undefined, time: 0 };
            suppressNextLayoutClick = true;
            window.setTimeout(() => {
                suppressNextLayoutClick = false;
            }, 250);
            document.body.classList.add("layout-key-dragging");
            layoutKeyElement(layoutDragState.sourceIndex)?.classList.add("drag-source");
            beginLayoutKeyDragVisual(event);
        }
        event.preventDefault();
        updateLayoutKeyDragVisual(event);
        markLayoutDragTarget(layoutKeyDropIndex(event.clientX, event.clientY));
    }

    function finishLayoutKeyDrag(event) {
        if (!layoutDragState || event.pointerId !== layoutDragState.pointerId) return;
        const state = layoutDragState;
        const targetIndex = state.dragging ? layoutKeyDropIndex(event.clientX, event.clientY) : undefined;
        cleanupLayoutKeyDrag();
        if (!state.dragging) return;
        event.preventDefault();
        event.stopPropagation();
        if (!Number.isInteger(targetIndex) || targetIndex === state.sourceIndex) return;
        const before = currentLocalSnapshot || serializeLocalState();
        stageLayoutSwap(state.sourceIndex, targetIndex);
        selectedKey = targetIndex;
        activeBehaviorKeycode = "";
        render();
        commitLocalHistory(before);
    }

    function cancelLayoutKeyDrag() {
        cleanupLayoutKeyDrag();
    }

    function cleanupLayoutKeyDrag() {
        document.body.classList.remove("layout-key-dragging");
        layoutDragState?.ghost?.remove?.();
        for (const key of document.querySelectorAll(".layout-board-svg .svg-key.drag-source, .layout-board-svg .svg-key.drag-target")) {
            key.classList.remove("drag-source", "drag-target");
        }
        layoutDragState = undefined;
    }

    function beginLayoutKeyDragVisual(event) {
        if (!layoutDragState) return;
        const source = layoutKeyElement(layoutDragState.sourceIndex);
        const svg = source?.ownerSVGElement;
        if (!source || !svg) return;
        const ghost = source.cloneNode(true);
        ghost.classList.remove("drag-source", "drag-target", "selected", "combo-input-selected");
        ghost.classList.add("layout-drag-ghost");
        ghost.removeAttribute("data-action");
        ghost.removeAttribute("tabindex");
        ghost.setAttribute("aria-hidden", "true");
        layoutDragState.svg = svg;
        layoutDragState.sourceMatrix = source.transform.baseVal.consolidate()?.matrix || svg.createSVGMatrix();
        layoutDragState.ghost = ghost;
        svg.appendChild(ghost);
        updateLayoutKeyDragVisual(event);
    }

    function updateLayoutKeyDragVisual(event) {
        if (!layoutDragState?.ghost || !layoutDragState.svg || !layoutDragState.startSvg) return;
        const point = layoutSvgPoint(layoutDragState.svg, event.clientX, event.clientY);
        if (!point) return;
        const dx = point.x - layoutDragState.startSvg.x;
        const dy = point.y - layoutDragState.startSvg.y;
        const matrix = layoutDragMatrix(layoutDragState.svg, layoutDragState.sourceMatrix, dx, dy);
        layoutDragState.ghost.setAttribute("transform", svgMatrixAttribute(matrix));
    }

    function layoutDragMatrix(svg, sourceMatrix, dx, dy) {
        const translation = svg.createSVGMatrix().translate(dx, dy);
        return translation.multiply(sourceMatrix || svg.createSVGMatrix());
    }

    function svgMatrixAttribute(matrix) {
        return "matrix(" + [matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f].map(svgNumber).join(" ") + ")";
    }

    function markLayoutDragTarget(layoutIndex) {
        for (const key of document.querySelectorAll(".layout-board-svg .svg-key.drag-target")) {
            key.classList.remove("drag-target");
        }
        if (!layoutDragState || !Number.isInteger(layoutIndex) || layoutIndex === layoutDragState.sourceIndex) return;
        layoutKeyElement(layoutIndex)?.classList.add("drag-target");
    }

    function layoutKeyDropIndex(clientX, clientY) {
        const element = document.elementFromPoint(clientX, clientY);
        const key = element?.closest?.(".layout-board-svg .svg-key[data-index]");
        if (!key || key.dataset.action !== "selectKey") return undefined;
        const index = Number(key.dataset.index);
        return Number.isInteger(index) ? index : undefined;
    }

    function layoutSvgPoint(svg, clientX, clientY) {
        const ctm = svg?.getScreenCTM?.();
        if (!svg || !ctm) return undefined;
        const point = svg.createSVGPoint();
        point.x = clientX;
        point.y = clientY;
        return point.matrixTransform(ctm.inverse());
    }

    function layoutKeyElement(layoutIndex) {
        return document.querySelector(".layout-board-svg .svg-key[data-index='" + String(layoutIndex) + "']");
    }

    function normalizeRgbGroupState() {
        if (!["layer", "pdMode", "combo", "keyBehavior"].includes(rgbGroupTarget)) {
            rgbGroupTarget = "layer";
        }
        normalizeRgbLedGroupSource();
        const owners = rgbGroupOwners(rgbGroupTarget);
        if (!owners.length) {
            rgbGroupOwner = "";
        } else if (rgbGroupTarget === "layer") {
            const layerNames = layersForUi().map((layer) => layer.name);
            if (activeLayer === rgbLayerAllGroups || !layerNames.includes(activeLayer)) {
                activeLayer = layerNames[0] || "";
                activeBehaviorKeycode = "";
            }
            if (!owners.includes(rgbGroupOwner)) {
                rgbGroupOwner = activeLayer && owners.includes(activeLayer) ? activeLayer : owners[0];
            }
        } else if (!owners.includes(rgbGroupOwner)) {
            rgbGroupOwner = owners[0];
        }
    }

    function normalizeRgbLedGroupSource() {
        const names = rgbReusableLedGroups().map((group) => group.name);
        if (rgbLedGroupSource !== "inline" && !names.includes(rgbLedGroupSource)) {
            rgbLedGroupSource = "inline";
        }
    }

    function syncRgbLayerOwnerToActiveLayer() {
        if (rgbGroupTarget !== "layer") return;
        if (rgbGroupOwner === rgbLayerAllGroups) return;
        if (!activeLayer || !rgbGroupOwners("layer").includes(activeLayer)) return;
        if (rgbGroupOwner !== activeLayer) {
            rgbBuilderColor = undefined;
        }
        rgbGroupOwner = activeLayer;
    }

    function post(message) {
        storeActiveViewDraft();
        dismissedStatusSignature = "";
        notice = "Working...";
        layoutNotice = "";
        render();
        resetLocalHistory();
        const activeProfileId = model?.activeProfile?.id || "";
        const payload = activeProfileId && !message.profileId ? { ...message, profileId: activeProfileId } : message;
        vscode.postMessage(payload);
    }

    function discardLocalDraftState() {
        rgbGroupOwner = "";
        rgbSelectedLeds = [];
        rgbLedGroupSource = "inline";
        rgbReusableGroupDraftName = "";
        rgbReusableGroupOriginalName = "";
        rgbBuilderColor = undefined;
        layoutComboPicking = false;
        layoutComboSelection = [];
        layoutComboOutput = "";
        layoutComboInputs = "";
        clearLayoutComboOriginal();
        activeMacroKeycode = "";
        macroDrafts = {};
        viewDrafts = {};
        resetMacroRecorderState();
        pendingLayoutEdits = {};
        pendingLayerAdds = [];
        pendingLayerDeletes = [];
        layerAddOpen = false;
        layerDraftName = "";
        copiedLayoutKey = "";
        layoutDragState = undefined;
        suppressNextLayoutClick = false;
        lastLayoutKeyClick = { index: undefined, time: 0 };
        keyPicker = undefined;
        keyPickerHost.innerHTML = "";
        layoutNotice = "";
        localUndoStack = [];
        localRedoStack = [];
        currentLocalSnapshot = "";
    }

    function render() {
        if (!model) {
            app.innerHTML = "<section>Loading...</section>";
            hydrateTooltips();
            return;
        }

        renderProfileControls();
        subtitle.textContent = model.activeProfile ? model.root + " / " + model.activeProfile.keymap : model.root;
        updateLayoutKeyBehaviorColorStyle();
        app.innerHTML = renderDiagnostics() + renderViewTabs() + renderActiveView();
        initializeDirtyTracking();
        restoreActiveViewDraft();
        hydrateTooltips();
        scheduleMacroSlotBrowserHeightSync();
    }

    function renderProfileControls() {
        const select = document.getElementById("profileSelect");
        const create = document.getElementById("createProfile");
        if (!select) return;
        const profiles = model.profiles || [];
        if (!profiles.length) {
            select.innerHTML = "<option value=''>No profiles</option>";
            select.value = "";
            select.disabled = true;
        } else {
            select.innerHTML = profiles.map(renderProfileOption).join("");
            select.value = model.activeProfile?.id || "";
            select.disabled = false;
        }
        if (create) {
            create.disabled = false;
        }
        const editable = Boolean(model.activeProfile?.editable);
        const mutable = editable && model.activeProfile?.keymap !== "noah";
        setHeaderButtonDisabled("cloneProfile", !editable);
        setHeaderButtonDisabled("renameProfile", !mutable);
        setHeaderButtonDisabled("deleteProfile", !mutable);
        setHeaderButtonDisabled("openKeymap", !editable);
        setHeaderButtonDisabled("openConfig", !editable);
        setHeaderButtonDisabled("openRgb", !editable);
        setHeaderButtonDisabled("generateProfileDocs", !editable);
        setHeaderButtonDisabled("compileFirmware", !Boolean(model.activeProfile?.buildable));
    }

    function renderProfileOption(profile) {
        const flags = [
            profile.editable ? "" : "incomplete",
            profile.registered ? "" : "unregistered",
        ].filter(Boolean);
        const label = profile.keymap + (flags.length ? " (" + flags.join(", ") + ")" : "");
        const disabled = profile.editable ? "" : " disabled";
        return "<option value='" + escapeAttr(profile.id) + "'" + disabled + ">" + escapeHtml(label) + "</option>";
    }

    function setHeaderButtonDisabled(id, disabled) {
        const button = document.getElementById(id);
        if (button) button.disabled = disabled;
    }

    function scheduleMacroSlotBrowserHeightSync() {
        if (macroSlotHeightFrame) return;
        macroSlotHeightFrame = requestAnimationFrame(() => {
            macroSlotHeightFrame = 0;
            syncMacroSlotBrowserHeight();
        });
    }

    function syncMacroSlotBrowserHeight() {
        const browser = document.querySelector(".macro-slot-browser");
        const main = document.querySelector(".macro-builder-main");
        if (!browser || !main) return;
        browser.style.height = "";
        const browserRect = browser.getBoundingClientRect();
        const mainRect = main.getBoundingClientRect();
        const sideBySide = Math.abs(browserRect.top - mainRect.top) < 8 && browserRect.width > 0 && mainRect.width > 0;
        if (!sideBySide) return;
        const height = Math.ceil(mainRect.height);
        if (height > 0) {
            browser.style.height = height + "px";
        }
    }

    function resetLocalHistory() {
        localUndoStack = [];
        localRedoStack = [];
        currentLocalSnapshot = serializeLocalState();
    }

    function commitLocalHistory(before) {
        if (restoringLocalSnapshot) return;
        const after = serializeLocalState();
        if (!before || before === after) {
            currentLocalSnapshot = after;
            return;
        }
        localUndoStack.push(before);
        if (localUndoStack.length > localHistoryLimit) {
            localUndoStack.shift();
        }
        localRedoStack = [];
        currentLocalSnapshot = after;
    }

    function undoLocalEdit() {
        if (!localUndoStack.length) return;
        const current = currentLocalSnapshot || serializeLocalState();
        const previous = localUndoStack.pop();
        localRedoStack.push(current);
        if (localRedoStack.length > localHistoryLimit) {
            localRedoStack.shift();
        }
        restoreLocalSnapshot(previous);
    }

    function redoLocalEdit() {
        if (!localRedoStack.length) return;
        const current = currentLocalSnapshot || serializeLocalState();
        const next = localRedoStack.pop();
        localUndoStack.push(current);
        if (localUndoStack.length > localHistoryLimit) {
            localUndoStack.shift();
        }
        restoreLocalSnapshot(next);
    }

    function serializeLocalState() {
        return JSON.stringify({
            activeView,
            activeLayer,
            selectedKey,
            activeBehaviorKeycode,
            rgbGroupTarget,
            rgbGroupOwner,
            rgbSelectedLeds,
            rgbLedGroupSource,
            rgbReusableGroupDraftName,
            rgbReusableGroupOriginalName,
            rgbBuilderColor,
            layoutComboPicking,
            layoutComboSelection,
            layoutComboOutput,
            layoutComboInputs,
            activeLayoutComboOriginal,
            activeLayoutComboOriginalSource: activeLayoutComboOriginal ? activeLayoutComboOriginalSource : "",
            activeMacroKeycode,
            macroDrafts,
            macroRecordDelays,
            macroRecorderMode,
            macroRecorderDelayThreshold,
            macroRecorderDelayRound,
            pendingLayoutEdits,
            pendingLayerAdds,
            pendingLayerDeletes,
            layerAddOpen,
            layerDraftName,
            controls: localEditableControls().map(controlSnapshot)
        });
    }

    function restoreLocalSnapshot(snapshot) {
        let state;
        try {
            state = JSON.parse(snapshot);
        } catch {
            return;
        }
        restoringLocalSnapshot = true;
        try {
            activeView = state.activeView || activeView;
            activeLayer = state.activeLayer || activeLayer;
            selectedKey = Number.isInteger(state.selectedKey) ? state.selectedKey : selectedKey;
            activeBehaviorKeycode = state.activeBehaviorKeycode || "";
            rgbGroupTarget = state.rgbGroupTarget || rgbGroupTarget;
            rgbGroupOwner = state.rgbGroupOwner || "";
            rgbSelectedLeds = Array.isArray(state.rgbSelectedLeds) ? state.rgbSelectedLeds : [];
            rgbLedGroupSource = state.rgbLedGroupSource || "inline";
            rgbReusableGroupDraftName = state.rgbReusableGroupDraftName || "";
            rgbReusableGroupOriginalName = state.rgbReusableGroupOriginalName || "";
            rgbBuilderColor = state.rgbBuilderColor || undefined;
            layoutComboPicking = Boolean(state.layoutComboPicking);
            layoutComboSelection = Array.isArray(state.layoutComboSelection) ? state.layoutComboSelection : [];
            layoutComboOutput = state.layoutComboOutput || "";
            layoutComboInputs = state.layoutComboInputs || "";
            activeLayoutComboOriginal = normalizeLayoutComboOriginal(state.activeLayoutComboOriginal);
            activeLayoutComboOriginalSource = normalizeLayoutComboOriginalSource(state.activeLayoutComboOriginalSource, activeLayoutComboOriginal);
            activeMacroKeycode = state.activeMacroKeycode || "";
            macroDrafts = state.macroDrafts && typeof state.macroDrafts === "object" ? state.macroDrafts : {};
            macroRecordDelays = typeof state.macroRecordDelays === "boolean" ? state.macroRecordDelays : macroRecordDelays;
            macroRecorderMode = state.macroRecorderMode === "exact" ? "exact" : "compact";
            macroRecorderDelayThreshold = state.macroRecorderDelayThreshold || macroRecorderDelayThreshold;
            macroRecorderDelayRound = state.macroRecorderDelayRound || macroRecorderDelayRound;
            clearMacroRecorderSession(false);
            pendingLayoutEdits = state.pendingLayoutEdits && typeof state.pendingLayoutEdits === "object" ? state.pendingLayoutEdits : {};
            pendingLayerAdds = Array.isArray(state.pendingLayerAdds) ? state.pendingLayerAdds : [];
            pendingLayerDeletes = Array.isArray(state.pendingLayerDeletes) ? state.pendingLayerDeletes : [];
            layerAddOpen = Boolean(state.layerAddOpen);
            layerDraftName = state.layerDraftName || "";
            reconcilePendingLayerStructure();
            normalizeActiveLayer();
            normalizeLayoutComboState();
            normalizeMacroBuilderState();
            normalizeRgbGroupState();
            render();
            restoreLocalControls(state.controls || []);
            refreshRestoredLocalState();
            currentLocalSnapshot = serializeLocalState();
        } finally {
            restoringLocalSnapshot = false;
        }
    }

    function restoreLocalControls(snapshots) {
        const controls = localEditableControls();
        snapshots.forEach((snapshot, index) => {
            const control = controls[index];
            if (!control) return;
            if (control.type === "checkbox" || control.type === "radio") {
                control.checked = Boolean(snapshot.checked);
            } else {
                control.value = snapshot.value || "";
            }
        });
    }

    function refreshRestoredLocalState() {
        for (const select of document.querySelectorAll("[data-helper-select]")) {
            updateHelperFields(select.id.replace(/Helper$/, ""));
        }
        for (const control of document.querySelectorAll("[data-color-control]")) {
            updateColorControl(control);
        }
        for (const composer of document.querySelectorAll("[data-macro-composer]")) {
            updateMacroComposerFields(composer.closest("[data-macro-workbench]") || composer);
        }
        updateRgbSelectionPreview();
        for (const section of document.querySelectorAll("[data-dirty-section]")) {
            updateDirtySection(section);
        }
        hydrateTooltips();
    }

    function localEditableControls() {
        return Array.from(app.querySelectorAll("input, select, textarea"))
            .filter((control) => !control.disabled);
    }

    function controlSnapshot(control) {
        if (control.type === "checkbox" || control.type === "radio") {
            return { checked: Boolean(control.checked) };
        }
        return { value: control.value || "" };
    }

    function storeActiveViewDraft() {
        if (!model || !activeView) return;
        const dirty = Boolean(app.querySelector("[data-dirty-section].dirty"));
        if (!dirty) {
            delete viewDrafts[activeView];
            return;
        }
        const labels = currentDirtySectionLabels();
        viewDrafts = {
            ...viewDrafts,
            [activeView]: {
                dirty: true,
                labels: labels.length ? labels : [viewLabel(activeView) + " form edits"],
                controls: localEditableControls().map(controlSnapshot),
            },
        };
    }

    function restoreActiveViewDraft() {
        const draft = viewDrafts[activeView];
        if (!draft || !draft.controls) return;
        restoreLocalControls(draft.controls);
        refreshRestoredLocalState();
    }

    function initializeDirtyTracking() {
        for (const section of document.querySelectorAll("[data-dirty-section]")) {
            section.dataset.dirtyBaseline = dirtySnapshot(section);
            updateDirtySection(section);
        }
        refreshDirtyTabIndicators();
    }

    function updateDirtyFromEvent(event) {
        const section = event.target?.closest?.("[data-dirty-section]");
        if (section) {
            updateDirtySection(section);
        }
        storeActiveViewDraft();
        refreshDirtyTabIndicators();
    }

    function updateDirtySection(section) {
        const dirty = dirtySnapshot(section) !== (section.dataset.dirtyBaseline || "") || sectionHasCustomDirtyState(section);
        section.classList.toggle("dirty", dirty);
        for (const button of section.querySelectorAll("[data-dirty-button]")) {
            setDirtyButtonState(button, dirty);
        }
    }

    function sectionHasCustomDirtyState(section) {
        if (section.id === "layoutComboBuilder") return Boolean(layoutComboOutput || layoutComboInputs || layoutComboSelection.length);
        if (section.matches?.("[data-macro-editor]")) return macroSlotDirty(section.dataset.keycode || "");
        if (section.id === "rgbReusableGroups") return Boolean(rgbReusableGroupDraftName || rgbReusableGroupOriginalName);
        return section.id === "rgbGroupBuilder" && Boolean(effectiveRgbBuilderLedIndices().length || rgbBuilderColor || rgbBuilderUsesReusableGroup());
    }

    function setDirtyButtonState(button, dirty) {
        const cleanLabel = button.dataset.cleanLabel || button.textContent.trim();
        button.dataset.cleanLabel = cleanLabel;
        button.textContent = dirty ? "Unsaved - " + cleanLabel : cleanLabel;
        button.classList.toggle("dirty", dirty);
        button.setAttribute("aria-label", dirty ? "Unsaved changes: " + cleanLabel : cleanLabel);
        button.disabled = !dirty;
    }

    function refreshDirtyTabIndicators() {
        for (const tab of document.querySelectorAll("[data-view-tab]")) {
            const dirty = viewTabDirty(tab.dataset.view || "");
            tab.classList.toggle("dirty", dirty);
            tab.setAttribute("aria-label", dirty ? tab.textContent.trim() + " has unsaved changes" : tab.textContent.trim());
        }
        for (const tab of document.querySelectorAll("[data-layer-tab]")) {
            const dirty = tab.dataset.layerDirtyScope === "layout" && layerTabDirty(tab.dataset.layer || "");
            tab.classList.toggle("dirty", dirty);
            tab.setAttribute("aria-label", dirty ? tab.textContent.trim() + " has unsaved changes" : tab.textContent.trim());
        }
        updateHeaderApplyAllState();
    }

    function hasApplyAllChanges() {
        return Boolean(model && (hasPendingLayerChanges() || pendingLayoutChangeCount()));
    }

    function updateHeaderApplyAllState() {
        const button = document.getElementById("applyAll");
        if (!button) return;
        const active = hasApplyAllChanges();
        button.hidden = !active;
        button.disabled = !active;
        const count = model ? pendingLayoutChangeCount() : 0;
        const layerChanges = pendingLayerAdds.length + pendingLayerDeletes.length;
        const summary = [
            layerChanges ? layerChanges + " layer " + (layerChanges === 1 ? "change" : "changes") : "",
            count ? count + " layout " + (count === 1 ? "change" : "changes") : "",
        ].filter(Boolean).join(", ");
        button.textContent = summary ? "Apply all (" + summary + ")" : "Apply all";
        button.setAttribute("aria-label", summary ? "Apply all staged changes: " + summary : "Apply all staged changes");
    }

    function dirtySnapshot(section) {
        const values = [];
        const controls = Array.from(section.querySelectorAll("input, select, textarea"))
            .filter((control) => !control.disabled && !control.closest("[hidden]"));
        controls.forEach((control, index) => {
            const key = control.name || control.id || String(index);
            if (control.type === "checkbox" || control.type === "radio") {
                values.push([key, control.checked ? "1" : "0"]);
            } else {
                values.push([key, control.value || ""]);
            }
        });
        return JSON.stringify(values);
    }

    function validateWriteTarget(target) {
        const section = target.closest("[data-dirty-section]") || target.closest(".card") || app;
        const valid = validateSection(section, true);
        if (!valid) {
            notice = "Fix invalid fields before writing.";
        }
        return valid;
    }

    function validateSection(section, focusFirst) {
        let firstInvalid = undefined;
        for (const control of section.querySelectorAll("input, select, textarea")) {
            const valid = validateControl(control);
            if (!valid && !firstInvalid) {
                firstInvalid = control;
            }
        }
        if (firstInvalid && focusFirst) {
            reportInvalidControl(firstInvalid);
        }
        return !firstInvalid;
    }

    function validateControl(control) {
        if (!control || control.disabled || control.closest("[hidden]")) return true;
        const rule = control.dataset.validate || "";
        if (!rule) {
            clearFieldError(control);
            return true;
        }
        const value = String(control.value || "").trim();
        if (macroStepEmptyFieldShouldStayNeutral(control, value)) {
            clearFieldError(control);
            return true;
        }
        if (comboBuilderShouldStayNeutral(control)) {
            clearComboBuilderFieldErrors(control.closest("[data-combo-builder]"));
            return true;
        }
        let error = "";
        if (rule === "uint8") {
            error = validateUint8(value, "Enter an integer from 0 to 255.");
        } else if (rule === "timing-ms") {
            error = validateMsTermNumber(value, "Enter 1-" + keyBehaviorTimingMaxMs + " ms.", false);
        } else if (rule === "hsv-value") {
            error = validateHsvValue(value);
        } else if (rule === "optional-term") {
            error = validateOptionalTerm(value, "Enter 1-" + keyBehaviorTimingMaxMs + " ms.", false);
        } else if (rule === "layout-key") {
            error = validateLayoutKeyInput(value);
        } else if (rule === "rgb-led-group-name") {
            error = validateRgbLedGroupName(value);
        } else if (rule === "combo-inputs") {
            error = validateComboInputs(value);
        } else if (rule === "macro-key-list") {
            error = validateMacroKeyListInput(value, false);
        } else if (rule === "macro-key-single") {
            error = validateMacroKeyListInput(value, true);
        } else if (rule === "positive-int") {
            error = validatePositiveInteger(value, "Enter a positive integer.");
        } else if (rule === "nonnegative-int") {
            error = validateNonnegativeInteger(value, "Enter zero or a positive integer.");
        } else if (rule === "identifier") {
            error = validateIdentifier(value, "Enter a C identifier.");
        } else if (rule === "layer") {
            error = validateLayerIdentifier(value);
        } else if (rule === "safe-expression") {
            error = validateSafeConfigExpression(value);
        } else if (rule === "macro-payload") {
            error = validateMacroPayload(value);
        }
        setFieldError(control, error);
        return !error;
    }

    function reportFieldError(control, message) {
        setFieldError(control, message);
        reportInvalidControl(control);
    }

    function reportInvalidControl(control) {
        if (!control) return;
        let parent = control.parentElement;
        while (parent) {
            if (parent.matches?.("details:not([open])")) {
                parent.open = true;
            }
            parent = parent.parentElement;
        }
        control.scrollIntoView?.({ block: "center", inline: "nearest" });
        control.focus?.({ preventScroll: true });
        control.reportValidity?.();
    }

    function validateUint8(value, message) {
        if (!/^\\d+$/.test(value)) return message;
        const number = Number(value);
        return Number.isInteger(number) && number >= 0 && number <= 255 ? "" : message;
    }

    function validateHsvValue(value) {
        if (/^\\d+$/.test(value)) {
            return validateUint8(value, "Enter an integer from 0 to 255, or a safe constant like RGB_MATRIX_MAXIMUM_BRIGHTNESS.");
        }
        if (/^[A-Z_][A-Z0-9_]*(?:\\s*[-+*/]\\s*(?:\\d+|[A-Z_][A-Z0-9_]*))*$/.test(value)) {
            return "";
        }
        return "Enter an integer from 0 to 255, or a safe constant like RGB_MATRIX_MAXIMUM_BRIGHTNESS.";
    }

    function validateOptionalTerm(value, message, allowZero) {
        if (!value) return "";
        if (/^\\d+$/.test(value)) return validateMsTermNumber(value, message, allowZero);
        return message;
    }

    function validateMsTermNumber(value, message, allowZero) {
        if (!/^\\d+$/.test(value || "")) return message;
        const normalized = String(value).replace(/^0+(?=\\d)/, "");
        if (!allowZero && normalized === "0") return message;
        const max = String(keyBehaviorTimingMaxMs);
        return normalized.length < max.length || (normalized.length === max.length && normalized <= max) ? "" : message;
    }

    function validateLayoutKeyInput(value) {
        return layoutKeyExpressionError(canonicalLayoutKeyExpression(value));
    }

    function validateRgbLedGroupName(value) {
        if (!value) return "Enter a reusable RGB_LED_GROUP_* name.";
        if (!/^RGB_LED_GROUP_[A-Z0-9_]+$/.test(value)) {
            return "Use a name like RGB_LED_GROUP_THUMBS.";
        }
        return "";
    }

    function validateComboInputs(value) {
        const parts = splitLayoutArguments(String(value || "")).filter(Boolean);
        if (parts.length < 2) return "Combo inputs need at least two keycodes.";
        const seen = new Set();
        for (const part of parts) {
            const canonical = canonicalLayoutKeyExpression(part);
            const error = layoutKeyExpressionError(canonical);
            if (error) return "Invalid combo input '" + part + "': " + error;
            if (seen.has(canonical)) return "Combo inputs must be unique; " + displayKeyExpression(canonical) + " appears more than once.";
            seen.add(canonical);
        }
        return "";
    }

    function comboBuilderShouldStayNeutral(control) {
        const builder = control.closest?.("[data-combo-builder]");
        if (!builder || !control.closest?.("[data-combo-output], [data-combo-inputs]")) return false;
        const output = String(fieldValueFromMarker(builder, "[data-combo-output]") || "").trim();
        const inputs = String(fieldValueFromMarker(builder, "[data-combo-inputs]") || "").trim();
        return !output && !inputs && !layoutComboSelection.length;
    }

    function clearComboBuilderFieldErrors(builder) {
        if (!builder) return;
        for (const marker of builder.querySelectorAll("[data-combo-output], [data-combo-inputs]")) {
            for (const control of marker.querySelectorAll("input, select, textarea")) {
                clearFieldError(control);
            }
        }
    }

    function validateMacroKeyListInput(value, single) {
        const keys = canonicalMacroKeySequence(value || "");
        if (single && keys.length !== 1) return "Choose exactly one key.";
        if (!single && !keys.length) return "Choose at least one key.";
        return macroPayloadKeyListError(keys);
    }

    function validatePositiveInteger(value, message) {
        if (!/^\\d+$/.test(value)) return message;
        return Number(value) > 0 ? "" : message;
    }

    function validateNonnegativeInteger(value, message) {
        return /^\\d+$/.test(value) ? "" : message;
    }

    function validateIdentifier(value, message) {
        return /^[A-Z_][A-Z0-9_]*$/.test(value || "") ? "" : message;
    }

    function validateLayerIdentifier(value) {
        const identifierError = validateIdentifier(value, "Enter a LAYER_* identifier.");
        if (identifierError) return identifierError;
        return String(value || "").startsWith("LAYER_") ? "" : "Enter a LAYER_* identifier.";
    }

    function validateSafeConfigExpression(value) {
        if (!value || /[;"{}#\\n\\r]/.test(value)) {
            return "Enter a safe C expression.";
        }
        return "";
    }

    function validateMacroPayload(value) {
        const parsed = parseMacroPayloadPreview(value || "");
        return parsed.error || "";
    }

    function macroStepEmptyFieldShouldStayNeutral(control, value) {
        return !value && Boolean(control?.closest?.("[data-macro-composer]") && control.closest("[data-macro-step-field]"));
    }

    function setFieldError(control, message) {
        if (!control) return;
        const label = control.closest("label");
        control.classList.toggle("invalid", Boolean(message));
        control.toggleAttribute("aria-invalid", Boolean(message));
        control.setCustomValidity?.(message || "");
        if (!label) return;
        label.classList.toggle("invalid", Boolean(message));
        let error = label.querySelector(".field-error");
        if (!error) {
            error = document.createElement("span");
            error.className = "field-error";
            label.appendChild(error);
        }
        error.textContent = message || "";
    }

    function clearFieldError(control) {
        setFieldError(control, "");
    }

    function hydrateTooltips() {
        for (const [id, text] of Object.entries(headerTooltips)) {
            setTooltip(document.getElementById(id), text, true);
        }
        for (const button of document.querySelectorAll("button")) {
            setTooltip(button, tooltipForButton(button), true);
        }
        for (const summary of document.querySelectorAll("summary")) {
            setTooltip(summary, tooltipForSummary(summary), true);
        }
        for (const label of document.querySelectorAll("label")) {
            const text = tooltipForLabel(label);
            setTooltip(label, text, false);
            for (const control of label.querySelectorAll("input, select, textarea")) {
                setTooltip(control, text, true);
            }
        }
        for (const control of document.querySelectorAll("input, select, textarea")) {
            setTooltip(control, tooltipForControl(control), true);
        }
        for (const header of document.querySelectorAll("th")) {
            setTooltip(header, tooltipForField(header.textContent), false);
        }
        for (const source of document.querySelectorAll(".source-pill")) {
            setTooltip(source, fieldTooltips.source, false);
        }
        for (const swatch of document.querySelectorAll(".inline-swatch")) {
            setTooltip(swatch, swatch.getAttribute("data-tooltip") || "Color preview for this HSV expression.", false);
        }
        for (const swatch of document.querySelectorAll(".rgb-summary-swatch")) {
            const expression = swatch.closest(".rgb-subsection")?.querySelector("[data-summary-expression]")?.textContent || "";
            setTooltip(swatch, expression ? "Collapsed color preview: " + expression : "Collapsed color preview.", true);
        }
    }

    function tooltipForButton(button) {
        if (!button) return "";
        if (button.id && headerTooltips[button.id]) return headerTooltips[button.id];
        const action = button.dataset.action;
        if (action === "selectView") {
            return viewTooltips[button.dataset.view] || "Switch to this studio view.";
        }
        if (action === "selectLayer") {
            return "Show layer " + (button.dataset.layer || button.textContent.trim()) + " in the layout and RGB previews.";
        }
        if (action === "selectKey") {
            return "Select this key on the layout for editing.";
        }
        if (action === "toggleRgbLed") {
            return "Add or remove LED " + (button.dataset.led || "") + " from the pending RGB group.";
        }
        if (button.dataset.pickerAction) {
            return tooltipForPickerButton(button);
        }
        return actionTooltips[action] || button.textContent.trim();
    }

    function tooltipForPickerButton(button) {
        const action = button.dataset.pickerAction;
        if (action === "section") {
            return "Show the " + button.textContent.trim() + " keycode section in the picker.";
        }
        if (action === "modifier") {
            return "Toggle the " + (button.dataset.modifier || button.textContent.trim()) + " modifier for the selected key.";
        }
        if (action === "key") {
            return "Select " + displayKeyExpression(button.dataset.value || button.textContent.trim()) + " as the pending keycode.";
        }
        if (action === "removeKey") {
            return "Remove this key from the pending picker value.";
        }
        if (action === "clear") {
            return "Clear the pending modifiers and selected keys.";
        }
        if (action === "ok") {
            return "Write the pending picker value into the field.";
        }
        if (action === "cancel") {
            return "Close the picker without changing the field.";
        }
        return button.textContent.trim();
    }

    function tooltipForSummary(summary) {
        const title =
            summary.querySelector("h2, h3, .rgb-summary-title")?.textContent?.trim() ||
            summary.textContent.trim();
        if (panelTooltips[title]) return panelTooltips[title] + " Click to expand or collapse.";
        if (summary.closest(".rgb-subsection")) {
            return "RGB color row for " + title + ". Click to expand the HSV picker and channel fields.";
        }
        if (summary.closest(".behavior-step")) {
            return "Behavior actions for the " + title + ". Click to expand or collapse this tap-count branch.";
        }
        return "Click to expand or collapse this section.";
    }

    function tooltipForLabel(label) {
        const span = label.querySelector("span");
        const text = (span?.textContent || label.textContent || "").trim();
        return tooltipForField(text);
    }

    function tooltipForControl(control) {
        const label = control.closest("label");
        if (label) {
            return tooltipForLabel(label);
        }
        return tooltipForField(control.getAttribute("aria-label") || control.name || control.id || control.placeholder || "");
    }

    function tooltipForField(text) {
        const key = normalizeTooltipKey(text);
        if (!key) return "";
        if (fieldTooltips[key]) return fieldTooltips[key];
        if (key.endsWith(" helper")) {
            return "Choose how this behavior action runs. The selected helper controls when and how the action keycode is sent.";
        }
        if (key.endsWith(" action")) {
            return "Keycode or action passed to this behavior helper, for example Esc, Shift+\`, Cmd+Q, or a pointing-mode key.";
        }
        if (key.endsWith(" repeat hz")) {
            return "Repeat frequency used only when the helper is REPEAT_WHILE_HELD. Higher values send the action more often while held.";
        }
        return "";
    }

    function normalizeTooltipKey(text) {
        return String(text || "").replace(/\\s+/g, " ").trim().toLowerCase();
    }

    function setTooltip(element, text, aria = false) {
        if (!element || !text) return;
        const tooltipText = element.getAttribute("data-tooltip") || text;
        element.setAttribute("data-tooltip", tooltipText);
        element.removeAttribute("title");
        if (aria && !element.getAttribute("aria-label")) {
            element.setAttribute("aria-label", tooltipText);
        }
    }

    function tooltipTarget(target) {
        const element = target?.nodeType === 1 ? target : target?.parentElement;
        return element?.closest?.("[data-tooltip]");
    }

    function showTooltip(target, event) {
        const text = target?.getAttribute("data-tooltip") || "";
        if (!text || !tooltip) return;
        activeTooltipTarget = target;
        tooltip.className = "tooltip";
        const richHtml = tooltipRichHtml(target);
        if (richHtml) {
            tooltip.classList.add("layout-key-tooltip");
            tooltip.innerHTML = richHtml;
        } else {
            tooltip.textContent = text;
        }
        tooltip.hidden = false;
        if (event) {
            positionTooltip(event.clientX, event.clientY);
            return;
        }
        const box = target.getBoundingClientRect();
        positionTooltip(box.left + Math.min(24, box.width / 2), box.bottom);
    }

    function positionTooltip(clientX, clientY) {
        if (!tooltip || tooltip.hidden) return;
        const gap = 14;
        const margin = 8;
        const box = tooltip.getBoundingClientRect();
        let left = clientX + gap;
        let top = clientY + gap;
        if (left + box.width > window.innerWidth - margin) {
            left = Math.max(margin, clientX - box.width - gap);
        }
        if (top + box.height > window.innerHeight - margin) {
            top = Math.max(margin, clientY - box.height - gap);
        }
        tooltip.style.left = left + "px";
        tooltip.style.top = top + "px";
    }

    function hideTooltip() {
        activeTooltipTarget = undefined;
        if (tooltip) tooltip.hidden = true;
    }

    function tooltipRichHtml(target) {
        if (target?.dataset?.tooltipKind === "layoutKey") {
            return layoutKeyTooltipCardHtml(target);
        }
        return "";
    }

    function showFloatingStatus(message, isError, title) {
        clearFloatingStatus();
        const popup = document.createElement("div");
        popup.className = "status-popup";
        popup.dataset.ephemeralStatus = "1";
        popup.setAttribute("role", isError ? "alert" : "status");
        popup.setAttribute("aria-live", isError ? "assertive" : "polite");
        popup.innerHTML =
            "<div class='status-popup-header'>" +
            "<div class='status-popup-title'>" + escapeHtml(title || (isError ? "Compile failed" : "Firmware")) + "</div>" +
            "<button type='button' class='status-popup-dismiss' aria-label='Dismiss status popup'></button>" +
            "</div>" +
            "<div class='status-popup-body'><div class='" + (isError ? "error" : "notice") + "'>" + escapeHtml(message) + "</div></div>";
        popup.querySelector("button")?.addEventListener("click", () => popup.remove());
        document.body.appendChild(popup);
    }

    function clearFloatingStatus() {
        for (const existing of document.querySelectorAll("[data-ephemeral-status]")) {
            existing.remove();
        }
    }

    function renderDiagnostics() {
        const items = [];
        if (notice) items.push("<div class='notice'>" + escapeHtml(notice) + "</div>");
        for (const diagnostic of model.diagnostics || []) {
            items.push("<div class='warning'>" + escapeHtml(diagnostic) + "</div>");
        }
        const signature = currentStatusSignature();
        if (!items.length || signature === dismissedStatusSignature) return "";
        return "<div class='status-popup' role='status' aria-live='polite'>" +
            "<div class='status-popup-header'>" +
            "<div class='status-popup-title'>Status</div>" +
            "<button type='button' class='status-popup-dismiss' data-action='dismissStatus' aria-label='Dismiss status popup'></button>" +
            "</div>" +
            "<div class='status-popup-body'>" + items.join("") + "</div>" +
            "</div>";
    }

    function currentStatusSignature() {
        return JSON.stringify({
            notice: notice || "",
            diagnostics: model?.diagnostics || []
        });
    }

    function panel(title, body, open = true) {
        return "<details class='panel' " + (open ? "open" : "") + "><summary><h2>" + escapeHtml(title) + "</h2></summary><div class='panel-body'>" + body + "</div></details>";
    }

    function staticPanel(title, body) {
        return "<section class='panel'><h2>" + escapeHtml(title) + "</h2><div class='panel-body'>" + body + "</div></section>";
    }

    function renderViewTabs() {
        return "<div class='view-tabs' role='tablist' aria-label='Profile Studio views'>" + views.map(([id, label]) =>
            "<button type='button' role='tab' aria-selected='" + (activeView === id ? "true" : "false") + "' class='" + viewTabClasses(id).join(" ") + "' data-action='selectView' data-view-tab data-view='" + escapeAttr(id) + "'>" + escapeHtml(label) + "</button>"
        ).join("") + "</div>";
    }

    function viewTabClasses(viewId) {
        return ["view-tab", activeView === viewId ? "active" : "", viewTabDirty(viewId) ? "dirty" : ""].filter(Boolean);
    }

    function viewTabDirty(viewId) {
        if (viewHasDirtyDomSection(viewId)) return true;
        if (viewDrafts[viewId]?.dirty) return true;
        if (viewId === "layout") return layoutViewHasUnsavedChanges();
        if (viewId === "macros") return macroViewHasUnsavedChanges();
        if (viewId === "rgb") return rgbViewHasUnsavedChanges();
        return false;
    }

    function viewHasDirtyDomSection(viewId) {
        return activeView === viewId && Boolean(app.querySelector("[data-dirty-section].dirty"));
    }

    function layoutViewHasUnsavedChanges() {
        return Boolean(hasPendingLayerChanges() || pendingLayoutChangeCount());
    }

    function macroViewHasUnsavedChanges() {
        return Object.keys(macroDrafts || {}).some((keycode) => macroSlotDirty(keycode));
    }

    function rgbViewHasUnsavedChanges() {
        return Boolean(
            effectiveRgbBuilderLedIndices().length ||
            rgbBuilderColor ||
            rgbBuilderUsesReusableGroup() ||
            rgbReusableGroupDraftName ||
            rgbReusableGroupOriginalName
        );
    }

    function renderActiveView() {
        if (activeView === "macros") return renderMacroStudio();
        if (activeView === "rgb") return renderRgbStudio();
        if (activeView === "defaults") return renderDefaultsStudio();
        return renderLayerStudio();
    }

    function renderLayerStudio() {
        const layer = currentLayer();
        if (!layer) return panel("Layers", "<p class='muted'>No LAYOUT blocks found.</p>", true);
        const selected = layerPositionByLayoutIndex(layer, selectedKey) || layer.positions[0];
        const behaviorTarget = behaviorEditorTarget(layer, selected);
        const selectedBehavior = behaviorForKey(behaviorTarget.keycode);
        return "<div class='stack'>" +
            panel("Layout", renderLayerTabs() + renderLayoutWithSelectedKeyEditor(layer, selected) + renderSelectedBehaviorEditor(behaviorTarget, selectedBehavior), true) +
            panel("Layer Overview", renderLayerOverview(layer), true) +
            "</div>";
    }

    function behaviorEditorTarget(layer, selected) {
        const combo = layerCombos(layer).find((row) => keyExpressionsEquivalent(row.output, activeBehaviorKeycode));
        if (combo) {
            return {
                keycode: combo.output,
                display: combo.outputDisplay || combo.output,
                behaviorTitle: "Behavior for " + combo.badge + " output",
                behaviorContext: (combo.inputDisplays || combo.inputs).join(" + ") + " -> " + (combo.outputDisplay || combo.output)
            };
        }
        activeBehaviorKeycode = "";
        return selected;
    }

    function renderLayerTabs(showLayerFlow = true, showDirty = true) {
        const layers = layersForUi();
        const deleteDisabled = !activeLayer || activeLayer === "LAYER_BASE";
        return "<div class='tabs layer-tabs'>" + layers.map((layer) => {
            const pending = pendingLayerAdd(layer.name);
            const dirty = showDirty && layerTabDirty(layer.name);
            const classes = ["tab", "layer-tab", layer.name === activeLayer ? "active" : "", pending ? "pending-add" : "", dirty ? "dirty" : ""].filter(Boolean).join(" ");
            return "<button class='" + classes + "' data-action='selectLayer' data-layer-tab" + (showDirty ? " data-layer-dirty-scope='layout'" : "") + " data-layer='" + escapeAttr(layer.name) + "'>" + escapeHtml(layer.name + (pending ? " *" : "")) + "</button>";
        }).join("") + (showLayerFlow ?
            "<button type='button' class='layer-tab-action' data-action='showAddLayerDraft' aria-label='Add layer'>+</button>" +
            "<button type='button' class='layer-tab-action' data-action='deleteLayerDraft'" + (deleteDisabled ? " disabled" : "") + " aria-label='Delete active layer'>-</button>" : "") +
            "</div>" +
            (showLayerFlow ? renderLayerFlow() : "");
    }

    function layerTabDirty(layerName) {
        return Boolean(pendingLayerAdd(layerName) || pendingLayerDeletes.includes(layerName) || pendingLayoutChanges(layerName).length);
    }

    function renderLayerFlow() {
        if (!layerAddOpen && !hasPendingLayerChanges()) return "";
        return "<div class='layer-flow-card' data-layer-flow>" +
            (layerAddOpen ? renderLayerAddForm() : "") +
            (hasPendingLayerChanges() ? renderLayerStructureActions() : "") +
            "</div>";
    }

    function renderLayerAddForm() {
        const name = layerDraftName || nextLayerDraftName();
        return "<div class='layer-flow-row'>" +
            "<label><span>New layer</span><input id='layerDraftName' value='" + escapeAttr(name) + "' placeholder='LAYER_MEDIA' spellcheck='false'></label>" +
            "<button type='button' data-action='cancelLayerDraft'>Cancel</button>" +
            "<button type='button' class='primary' data-action='stageLayerDraft'>Stage layer</button>" +
            "</div>";
    }

    function renderLayerStructureActions() {
        const additions = pendingLayerAdds.map((layer) => "<span class='macro-chip'><strong>add</strong> " + escapeHtml(layer.name) + "</span>");
        const deletions = pendingLayerDeletes.map((layer) => "<span class='macro-chip delete'><strong>delete</strong> " + escapeHtml(layer) + "</span>");
        return "<div class='layer-flow-actions'>" +
            "<div class='layer-flow-summary'>" + additions.concat(deletions).join("") + "</div>" +
            "<button type='button' data-action='discardLayerChanges'>Discard</button>" +
            "<button type='button' class='primary dirty' data-action='applyLayerChanges'>Apply layer changes</button>" +
            "</div>";
    }

    function renderLayoutWithSelectedKeyEditor(layer, selected) {
        return "<div class='layout-with-key-editor'>" +
            renderBoard(layer) +
            "<div class='layout-selected-key-column'>" +
            "<div class='layout-sidecar-stack'>" +
            renderSelectedKeyEditor(layer, selected) +
            renderLayoutComboBuilder(layer) +
            "</div>" +
            "</div>" +
            "</div>";
    }

    function renderSelectedKeyEditor(layer, selected) {
        return "<div class='card selected-key-edit-card' data-dirty-section>" +
            "<h3>" + escapeHtml(selected.display || selected.keycode) + "</h3>" +
            "<div class='selected-key-edit-fields'>" +
            "<label><span>Layer</span><input disabled value='" + escapeAttr(layer.name) + "'></label>" +
            "<label><span>Layout index</span><input disabled value='" + selected.layoutIndex + "'></label>" +
            renderKeyPickerInput("keycodeInput", "Key", selected.editLabel || selected.display || selected.keycode, "A, Enter, Space, _______", "single", "", "", "data-validate='layout-key'") +
            "<div><span class='muted'>Source</span><br><code class='source-pill' data-tooltip='Raw keymap.c expression currently stored in this selected LAYOUT() slot.'>" + escapeHtml(selected.keycode) + "</code></div>" +
            "<button data-action='applyKey' data-dirty-button class='primary'>Stage key</button>" +
            "</div>" +
            "</div>";
    }

    function renderLayoutComboBuilder(layer) {
        const selectedPositions = layoutComboSelectedPositions(layer);
        const inputValue = layoutComboInputs || selectedPositions.map((position) => position.keycode).join(", ");
        const inputPickingLabel = layoutComboPicking ? "Done picking inputs" : "Pick input keys on layout";
        const canClear = Boolean(layoutComboOutput || layoutComboInputs || selectedPositions.length);
        const editing = Boolean(activeLayoutComboOriginal);
        return "<div id='layoutComboBuilder' class='card selected-key-edit-card layout-combo-builder-card' data-dirty-section data-combo-builder>" +
            "<h3>" + (editing ? "Edit combo" : "Create combo") + "</h3>" +
            "<div class='selected-key-edit-fields'>" +
            renderKeyPickerInput("layoutComboOutput", "Output", layoutComboOutput, "Tab", "single", "", "data-combo-output", "data-validate='layout-key'") +
            renderKeyPickerInput("layoutComboInputs", "Inputs", inputValue, "D, F", "list", "", "data-combo-inputs", "data-validate='combo-inputs'") +
            "<div class='layout-combo-selected-list' data-layout-combo-selected-list>" + renderLayoutComboSelectedList(layer) + "</div>" +
            "<div class='layout-combo-actions'>" +
            "<button type='button' class='combo-pick-toggle " + (layoutComboPicking ? "active" : "") + "' aria-pressed='" + (layoutComboPicking ? "true" : "false") + "' data-action='toggleLayoutComboPicking'>" + inputPickingLabel + "</button>" +
            "<button type='button' data-action='clearLayoutComboSelection' data-layout-combo-clear" + (canClear ? "" : " disabled") + ">Clear</button>" +
            "</div>" +
            "<button data-action='addLayoutCombo' data-dirty-button class='primary'>" + (editing ? "Save combo row" : "Append combo row") + "</button>" +
            "</div>" +
            "</div>";
    }

    function renderLayoutComboSelectedList(layer) {
        const selectedPositions = layoutComboSelectedPositions(layer);
        if (!selectedPositions.length) {
            return "<span class='muted' data-tooltip='No physical layout slots are selected as combo inputs yet. Use Pick input keys on layout, type Inputs manually, or open the key picker.'>No layout input keys selected</span>";
        }
        return selectedPositions.map((position) =>
            "<button type='button' data-action='toggleLayoutComboKey' data-index='" + position.layoutIndex + "' data-tooltip='" + escapeAttr("Remove layout index " + position.layoutIndex + " from this combo input draft: " + (position.display || position.keycode) + " (" + position.keycode + ")") + "'><span>" + escapeHtml(position.display || position.keycode) + "</span><code>" + escapeHtml(position.keycode) + "</code></button>"
        ).join("");
    }

    function renderSelectedBehaviorEditor(selected, behavior) {
        const row = behavior || {
            keycode: selected.keycode,
            tapHoldTerm: "",
            longerHoldTerm: "",
            multiTapTerm: "",
            rgbBranchConfirmTerm: "",
            skipRgbBranchConfirm: false,
            steps: []
        };
        const steps = [];
        for (let index = 0; index < 5; index += 1) {
            steps.push(row.steps.find((step) => step.tapCount === index) || { tapCount: index, tapCountName: tapBranchName(index) });
        }
        const title = selected.behaviorTitle || "Behavior on this key";
        const context = selected.behaviorContext
            ? "<div><span class='muted'>Reachable via</span><br><code data-tooltip='Visible selected key or combo output that reaches this behavior row.'>" + escapeHtml(selected.behaviorContext) + "</code></div>"
            : "";
        return "<div class='card selected-behavior-editor' data-dirty-section><h3>" + escapeHtml(title) + "</h3>" +
            "<input type='hidden' id='selectedBehaviorKeycode' value='" + escapeAttr(row.keycode) + "'>" +
            "<div><span class='muted'>Source</span><br><code class='source-pill' data-tooltip='Raw keycode that owns this key_behaviors[] row in keymap.c.'>" + escapeHtml(row.keycode) + "</code></div>" +
            context +
            "<div class='form-grid four'>" +
            renderTimingInput("selectedTapHoldTerm", "tap_hold_term", row.tapHoldTerm || "", row.keycode) +
            renderTimingInput("selectedLongerHoldTerm", "longer_hold_term", row.longerHoldTerm || "", row.keycode) +
            renderTimingInput("selectedMultiTapTerm", "multi_tap_term", row.multiTapTerm || "", row.keycode) +
            renderTimingInput("selectedRgbBranchConfirmTerm", "rgb_branch_confirm_term", row.rgbBranchConfirmTerm || "", row.keycode) +
            renderSkipRgbBranchConfirmToggle(row.skipRgbBranchConfirm) +
            "</div>" +
            "<div class='behavior-branch-grid'>" +
            steps.map(renderBehaviorStepEditor).join("") +
            "</div>" +
            "<button data-action='saveSelectedBehavior' data-dirty-button class='primary'>Save behavior row</button></div>";
    }

    function renderTimingInput(id, field, value, keycode) {
        const fallback = behaviorTimingDefault(field, keycode);
        const placeholder = "default: " + fallback.label;
        const tooltip = timingTooltip(field, fallback);
        return "<label data-tooltip='" + escapeAttr(tooltip) + "'><span>" + field + "</span><input id='" + id + "' data-validate='optional-term' inputmode='numeric' value='" + escapeAttr(value || "") + "' placeholder='" + escapeAttr(placeholder) + "' data-tooltip='" + escapeAttr(tooltip) + "'></label>";
    }

    function renderSkipRgbBranchConfirmToggle(checked) {
        const tooltip = fieldTooltips.skip_rgb_branch_confirm || "";
        return "<label class='toggle-inline' data-tooltip='" + escapeAttr(tooltip) + "'><input type='checkbox' id='selectedSkipRgbBranchConfirm'" + (checked ? " checked" : "") + " data-tooltip='" + escapeAttr(tooltip) + "'><span class='toggle-switch' aria-hidden='true'></span><span class='toggle-label'>skip RGB branch confirm</span></label>";
    }

    function behaviorTimingDefault(field, keycode) {
        const defaults = model.behaviorTimingDefaults || {};
        const useLtTapTerm = field === "tap_hold_term" && /^LT\\(/.test(normalizeDisplayExpression(keycode));
        const expression = {
            tap_hold_term: useLtTapTerm ? defaults.tappingTerm : defaults.tapHoldTerm,
            longer_hold_term: defaults.longerHoldTerm,
            multi_tap_term: defaults.multiTapTerm,
            rgb_branch_confirm_term: defaults.rgbBranchConfirmTerm
        }[field] || "";
        return {
            expression,
            label: formatMsExpression(expression) + (useLtTapTerm ? " via TAPPING_TERM for LT()" : "")
        };
    }

    function timingTooltip(field, fallback) {
        const base = fieldTooltips[field] || "";
        const suffix = fallback.expression ? " Empty uses " + fallback.label + "." : " Empty uses the runtime default.";
        return base + suffix;
    }

    function formatMsExpression(expression) {
        const text = normalizeDisplayExpression(expression);
        if (!text) return "runtime default";
        if (/^\\d+$/.test(text)) return text + " ms";
        return text;
    }

    function tapBranchName(index) {
        return tapCountNames[index] || ("Tap " + (index + 1) + " Branch");
    }

    function renderBehaviorStepEditor(step) {
        const open = stepHasAction(step) ? " open" : "";
        return "<details class='card behavior-step'" + open + ">" +
            "<summary><h3>" + escapeHtml(step.tapCountName || ("tap " + (step.tapCount + 1))) + "</h3></summary>" +
            "<input type='hidden' data-behavior-step='" + step.tapCount + "' value='" + step.tapCount + "'>" +
            "<div class='behavior-step-actions'>" +
            renderActionEditor("step" + step.tapCount + "Tap", "Tap", step.tap, ["", "TAP_SENDS"]) +
            renderActionEditor("step" + step.tapCount + "Hold", "Hold", step.hold, ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            renderActionEditor("step" + step.tapCount + "LongHold", "Long hold", step.longHold, ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            "</div></details>";
    }

    function renderActionEditor(id, label, action, helpers) {
        const hasRepeat = helpers.includes("REPEAT_WHILE_HELD");
        const actionHidden = action?.helper ? "" : " hidden";
        const repeatHidden = action?.helper === "REPEAT_WHILE_HELD" ? "" : " hidden";
        return "<div class='stack behavior-action-editor'>" +
            renderHelperControl(id, label, helpers, action?.helper || "") +
            renderKeyPickerInput(id + "Action", label + " action", editableActionValue(action), "Esc, Shift+\`, Cmd+Q", "single", "", "data-helper-action-prefix='" + escapeAttr(id) + "'" + actionHidden, "data-validate='layout-key'") +
            (hasRepeat ? "<label data-helper-field data-helper-prefix='" + id + "' data-helper-value='REPEAT_WHILE_HELD'" + repeatHidden + "><span>" + label + " repeat Hz</span><input id='" + id + "Repeat' data-validate='positive-int' inputmode='numeric' value='" + escapeAttr(action?.repeatHz || "") + "' placeholder='only for REPEAT_WHILE_HELD'></label>" : "") +
            "</div>";
    }

    function renderHelperControl(prefix, label, helpers, selected) {
        const enabled = Boolean(selected);
        const escapedPrefix = escapeAttr(prefix);
        return "<div class='behavior-helper-control'>" +
            "<label class='toggle-inline'><input type='checkbox' id='" + escapedPrefix + "Enabled' data-helper-enabled data-helper-prefix='" + escapedPrefix + "'" + (enabled ? " checked" : "") + "><span class='toggle-switch' aria-hidden='true'></span><span class='toggle-label'>" + escapeHtml(label) + "</span></label>" +
            "<label class='behavior-helper-select' data-helper-select-prefix='" + escapedPrefix + "'" + (enabled ? "" : " hidden") + "><span>" + escapeHtml(label) + " helper</span><select id='" + escapedPrefix + "Helper' data-helper-select data-helper-prefix='" + escapedPrefix + "'>" + helperOptions(helpers, selected) + "</select></label>" +
            "</div>";
    }

    function renderKeyPickerInput(id, label, value, placeholder, mode = "single", style = "", attrs = "", inputAttrs = "") {
        const styleAttr = style ? " style='" + escapeAttr(style) + "'" : "";
        const extraAttrs = attrs ? " " + attrs : "";
        const extraInputAttrs = inputAttrs ? " " + inputAttrs : "";
        const buttonLabel = mode === "list" ? "Pick keycodes" : "Pick keycode";
        return "<label" + styleAttr + extraAttrs + "><span>" + escapeHtml(label) + "</span><span class='input-with-button'>" +
            "<input id='" + escapeAttr(id) + "' value='" + escapeAttr(value || "") + "' placeholder='" + escapeAttr(placeholder || "") + "'" + extraInputAttrs + ">" +
            "<button type='button' data-action='openKeyPicker' data-target='" + escapeAttr(id) + "' data-mode='" + escapeAttr(mode) + "'>" + buttonLabel + "</button>" +
            "</span></label>";
    }

    function openLayoutKeyPicker(layoutIndex) {
        openKeyPicker("keycodeInput", "single", { layoutStageIndex: layoutIndex });
    }

    function openKeyPicker(targetId, mode, options = {}) {
        const input = document.getElementById(targetId);
        if (!input) return;
        keyPicker = {
            targetId,
            mode: mode === "list" ? "list" : "single",
            layoutStageIndex: Number.isInteger(options.layoutStageIndex) ? options.layoutStageIndex : undefined,
            section: "qwerty",
            search: "",
            layerTapLayer: "",
            mods: [],
            keys: []
        };
        seedKeyPickerFromValue(input.value || "");
        renderKeyPicker();
    }

    function seedKeyPickerFromValue(value) {
        const text = String(value || "").trim();
        if (!text || !keyPicker) return;
        if (keyPicker.mode === "list") {
            keyPicker.keys = text.split(",").map((part) => part.trim()).filter(Boolean);
            return;
        }
        const layerTap = text.match(/^LT\((LAYER_[A-Z0-9_]+),\s*(.+)\)$/);
        if (layerTap) {
            keyPicker.section = "layers";
            keyPicker.layerTapLayer = layerTap[1];
            keyPicker.keys = [layerTap[2].trim()];
            return;
        }
        const parts = text.split("+").map((part) => part.trim()).filter(Boolean);
        if (parts.length > 1) {
            const mods = parts.slice(0, -1).filter((part) => keyPickerModifiers.includes(part));
            if (mods.length === parts.length - 1) {
                keyPicker.mods = mods;
                keyPicker.keys = [parts[parts.length - 1]];
                return;
            }
        }
        keyPicker.keys = [text];
    }

    function closeKeyPicker() {
        keyPicker = undefined;
        keyPickerHost.innerHTML = "";
    }

    function renderKeyPicker() {
        if (!keyPicker) {
            keyPickerHost.innerHTML = "";
            return;
        }
        keyPickerHost.innerHTML = "<div class='modal-backdrop'>" +
            "<div class='key-picker' role='dialog' aria-label='Keycode picker'>" +
            "<div class='key-picker-head'>" +
            "<div><h2>Pick Keycode</h2><div class='muted'>" + escapeHtml(keyPicker.mode === "list" ? "Select one or more keys for a comma-separated combo input list." : "Select one key, optionally with modifiers.") + "</div></div>" +
            "<button data-picker-action='cancel'>Close</button>" +
            "</div>" +
            (keyPicker.mode === "list" || keyPicker.layerTapLayer ? "" : (
                "<div class='key-picker-mods'>" +
                "<span class='muted'>Mods</span>" +
                keyPickerModifiers.map((modifier) => "<button class='key-picker-mod " + (keyPicker.mods.includes(modifier) ? "selected" : "") + "' data-picker-action='modifier' data-modifier='" + escapeAttr(modifier) + "'>" + escapeHtml(modifier) + "</button>").join("") +
                "</div>"
            )) +
            "<div class='key-picker-body'>" +
            "<div class='key-picker-tabs'>" + keyPickerResolvedSections().map((section) =>
                "<button class='key-picker-tab " + (section.id === keyPicker.section ? "active" : "") + "' data-picker-action='section' data-section='" + escapeAttr(section.id) + "'>" + escapeHtml(section.label) + "</button>"
            ).join("") + "</div>" +
            "<div class='key-picker-section'>" + renderKeyPickerSection() + "</div>" +
            "</div>" +
            "<div class='stack'>" +
            "<div class='key-picker-selection'>" +
            "<span class='muted'>Selected</span>" +
            (keyPicker.layerTapLayer ? "<button data-picker-action='clearLayerTap' data-tooltip='Layer-tap target layer. Pick a tap key from any section to build LT(layer, key).'>LT " + escapeHtml(layerShortName(keyPicker.layerTapLayer)) + "</button>" : "") +
            keyPicker.keys.map((key, index) => "<button data-picker-action='removeKey' data-index='" + index + "'>" + escapeHtml(displayKeyExpression(key)) + "</button>").join("") +
            (keyPicker.keys.length ? "" : "<span class='muted'>No key selected</span>") +
            "</div>" +
            "<code class='key-picker-expression'>" + escapeHtml(keyPickerExpression() || "Select a key") + "</code>" +
            "</div>" +
            "<div class='key-picker-actions'>" +
            "<button data-picker-action='clear'>Clear</button>" +
            "<span class='toolbar'><button data-picker-action='cancel'>Cancel</button><button data-picker-action='ok' class='primary'>OK</button></span>" +
            "</div>" +
            "</div></div>";
        hydrateTooltips();
    }

    function keyPickerResolvedSections() {
        const qmkSections = qmkKeyPickerSections();
        const allQmkSection = {
            id: "qmk-all",
            label: "All QMK",
            kind: "allQmk",
            qmkEntries: model.qmkKeycodes || [],
            rows: qmkKeyRows(model.qmkKeycodes || [], 8)
        };
        return keyPickerSections.concat(qmkSections, [allQmkSection]).map((section) => {
            if (section.id === "layers") {
                return {
                    ...section,
                    rows: layersForUi().map((layer) => {
                        const actions = [{
                            value: "MO(" + layer.name + ")",
                            label: "Hold " + layerShortName(layer.name),
                            tooltip: "Momentary access to " + layer.name + " while held."
                        },
                        {
                            value: "LOCK_LAYER(" + layer.name + ")",
                            label: "Lock " + layerShortName(layer.name),
                            tooltip: "Toggle persistent lock for " + layer.name + "."
                        }];
                        if (keyPicker.mode !== "list") {
                            actions.push({
                                value: keyPickerLayerTapPrefix + layer.name,
                                label: "LT " + layerShortName(layer.name),
                                tooltip: "Choose this layer, then pick the tap key from any section."
                            });
                        }
                        return actions;
                    })
                };
            }
            if (section.kind === "allQmk") {
                return {
                    ...section,
                    qmkEntries: model.qmkKeycodes || [],
                    rows: qmkKeyRows(model.qmkKeycodes || [], 8)
                };
            }
            if (section.id === "modes") {
                const modes = (model.rgb?.pdModeColors || []).map((row) => row.pointingMode.replace(/^PD_MODE_/, ""));
                return {
                    ...section,
                    rows: modes.map((mode) => [mode + "_MODE", mode + "_MODE_LOCK"])
                };
            }
            if (section.id === "macros") {
                return {
                    ...section,
                    rows: chunkKeyPickerItems((model.viaMacros || []).map((slot) => slot.keycode), 8)
                        .concat(chunkKeyPickerItems((model.hardcodedMacros || []).map((slot) => slot.keycode), 8))
                };
            }
            if (section.id === "custom") {
                return {
                    ...section,
                    rows: (section.rows || []).concat(chunkKeyPickerItems(model.customKeycodes || [], 8))
                };
            }
            return section;
        });
    }

    function renderKeyPickerSection() {
        const section = keyPickerResolvedSections().find((candidate) => candidate.id === keyPicker.section) || keyPickerResolvedSections()[0];
        const query = keyPickerSearchQuery();
        if (query) {
            return renderKeyPickerSearch() + renderKeyPickerSectionSearchResults(section, query);
        }
        if (section.kind === "keyboard") {
            return renderKeyPickerKeyboard(section);
        }
        const empty = !(section.rows || []).some((row) => row.length)
            ? "<div class='key-picker-empty muted'>No keys in this section.</div>"
            : "";
        return renderKeyPickerSearch() + empty + renderKeyPickerGrid(section.rows || []);
    }

    function renderKeyPickerKeyboard(section) {
        const layout = section.layout || keyPickerKeyboardSvgLayout;
        return "<div class='key-picker-keyboard'>" +
            renderKeyPickerSearch() +
            "<div class='key-picker-keyboard-svg-wrap'>" +
            "<svg class='key-picker-keyboard-svg' viewBox='0 0 " + escapeAttr(layout.width) + " " + escapeAttr(layout.height) + "' role='group' aria-label='Full keyboard key picker'>" +
            "<rect x='0.75' y='0.75' width='" + escapeAttr(layout.width - 1.5) + "' height='" + escapeAttr(layout.height - 1.5) + "' rx='6' fill='none' stroke='rgba(96,112,122,0.34)' stroke-width='1.5'></rect>" +
            (layout.keys || []).map((key) => renderKeyPickerSvgKey(key)).join("") +
            "</svg>" +
            "</div>" +
            "</div>";
    }

    function renderKeyPickerSearch() {
        return "<input class='key-picker-search' data-picker-search value='" + escapeAttr(keyPicker.search || "") + "' placeholder='Search keys; categories narrow results'>";
    }

    function renderKeyPickerSectionSearchResults(section, query) {
        const rows = keyPickerSectionSearchRows(section, query);
        const empty = rows.length ? "" : "<div class='key-picker-empty muted'>No matching keys in " + escapeHtml(section.label || "this section") + ".</div>";
        return empty + renderKeyPickerGrid(rows);
    }

    function renderKeyPickerGrid(rows) {
        return "<div class='key-picker-grid'>" + (rows || []).map((row) =>
            "<div class='key-picker-row'>" + row.map((value) => renderKeyPickerKey(value)).join("") + "</div>"
        ).join("") + "</div>";
    }

    function keyPickerSectionSearchRows(section, query) {
        if (section.kind === "allQmk" || section.kind === "qmkCategory") {
            return qmkKeyRows(filterQmkKeycodes(section.qmkEntries || [], query), 8);
        }
        if (section.kind === "keyboard") {
            return qmkKeyRows(filterQmkKeycodes(model.qmkKeycodes || [], query), 8);
        }
        const text = keyPickerSearchNeedle(query);
        const items = flattenKeyPickerRows(section.rows || [])
            .filter((item) => keyPickerItemSearchText(item).includes(text))
            .slice(0, 160);
        return chunkKeyPickerItems(items, 8);
    }

    function flattenKeyPickerRows(rows) {
        const items = [];
        (rows || []).forEach((row) => {
            (row || []).forEach((item) => {
                if (item && typeof item === "object" && item.spacer) return;
                items.push(item);
            });
        });
        return items;
    }

    function chunkKeyPickerItems(items, columns) {
        const rows = [];
        for (let index = 0; index < items.length; index += columns) {
            rows.push(items.slice(index, index + columns));
        }
        return rows;
    }

    function keyPickerItemSearchText(item) {
        const value = keyPickerItemValue(item);
        const label = keyPickerItemLabel(item);
        const tooltip = item && typeof item === "object" && item.tooltip ? item.tooltip : "";
        return keyPickerSearchNeedle([value, label, displayKeyExpression(value), tooltip].join(" "));
    }

    function keyPickerSearchNeedle(value) {
        return String(value || "").trim().toLowerCase();
    }

    function renderKeyPickerSvgKey(key) {
        const selected = keyPicker.keys.includes(key.value);
        const lines = keyPickerSvgLabelLines(key.label || displayKeyExpression(key.value));
        const centerX = key.x + key.w / 2;
        const firstY = key.y + key.h / 2 - (lines.length - 1) * 8;
        const tooltip = displayKeyExpression(key.value) + " (" + key.value + ")";
        return "<g class='key-picker-svg-key " + (selected ? "selected" : "") + "' tabindex='0' role='button' data-picker-action='key' data-value='" + escapeAttr(key.value) + "' data-tooltip='" + escapeAttr(tooltip) + "'>" +
            "<rect x='" + escapeAttr(key.x) + "' y='" + escapeAttr(key.y) + "' width='" + escapeAttr(key.w) + "' height='" + escapeAttr(key.h) + "' rx='3' ry='3'></rect>" +
            "<text x='" + escapeAttr(centerX) + "' y='" + escapeAttr(firstY) + "'>" +
            lines.map((line, index) => "<tspan x='" + escapeAttr(centerX) + "' dy='" + (index === 0 ? "0" : "16") + "' class='" + (index === 0 ? "primary" : "secondary") + "'>" + escapeHtml(line) + "</tspan>").join("") +
            "</text>" +
            "</g>";
    }

    function keyPickerSvgLabelLines(label) {
        return (Array.isArray(label) ? label : String(label || "").split("\\n"))
            .map((line) => String(line || "").trim())
            .filter(Boolean);
    }

    function renderKeyPickerKey(item) {
        if (item && typeof item === "object" && item.spacer) {
            return "<span class='key-picker-spacer' style='--key-units: " + escapeAttr(item.spacer) + "'></span>";
        }
        const value = keyPickerItemValue(item);
        const label = keyPickerItemLabel(item);
        const selected = value.startsWith(keyPickerLayerTapPrefix)
            ? keyPicker.layerTapLayer === value.slice(keyPickerLayerTapPrefix.length)
            : keyPicker.keys.includes(value);
        const width = item && typeof item === "object" && item.w ? item.w : 1;
        const tooltip = item && typeof item === "object" && item.tooltip ? item.tooltip : value;
        return "<button class='key-picker-key " + (selected ? "selected" : "") + "' style='--key-units: " + escapeAttr(width) + "' data-picker-action='key' data-value='" + escapeAttr(value) + "' data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(label) + "</button>";
    }

    function keyPickerItemValue(item) {
        return item && typeof item === "object" ? item.value || "" : item;
    }

    function keyPickerItemLabel(item) {
        if (item && typeof item === "object" && item.label) return item.label;
        return displayKeyExpression(keyPickerItemValue(item));
    }

    function layerShortName(layer) {
        return titleCase(String(layer || "").replace(/^LAYER_/, ""));
    }

    function qmkKeyPickerSections() {
        const entries = model.qmkKeycodes || [];
        return qmkKeycodeSectionGroups.map((config) => {
            const groupEntries = entries.filter((entry) => config.groups.includes(entry.group));
            return {
                id: config.id,
                label: config.label,
                kind: "qmkCategory",
                qmkEntries: groupEntries,
                rows: qmkKeyRows(groupEntries, 6)
            };
        }).filter((section) => section.rows.length);
    }

    function qmkKeyRows(entries, columns) {
        const rows = [];
        const items = entries.map((entry) => ({
            value: keyPickerAuthoredValue(entry.value),
            label: entry.label || entry.value,
            tooltip: (entry.value || "") + (entry.key && entry.key !== entry.value ? " / " + entry.key : "") + ((entry.aliases || []).length ? " / " + entry.aliases.join(", ") : "")
        }));
        for (let index = 0; index < items.length; index += columns) {
            rows.push(items.slice(index, index + columns));
        }
        return rows;
    }

    function keyPickerAuthoredValue(value) {
        const canonical = canonicalLayoutKeyExpression(value);
        return canonical === "_______" || canonical === "XXXXXXX" ? canonical : value;
    }

    function filterQmkKeycodes(entries, query) {
        const variants = keyPickerQmkSearchVariants(query);
        const source = entries || [];
        if (!variants.length) return [];
        return source
            .map((entry, index) => ({entry, index, score: qmkKeycodeSearchScore(entry, variants)}))
            .filter((candidate) => candidate.score < Number.POSITIVE_INFINITY)
            .sort((left, right) => left.score - right.score || left.index - right.index)
            .map((candidate) => candidate.entry);
    }

    function keyPickerQmkSearchVariants(query) {
        const term = String(query || "").trim().toLowerCase();
        if (!term) return [];
        const variants = [term];
        const compact = compactKeyPickerSearchToken(term);
        if (compact && compact !== term) {
            variants.push(compact);
        }
        return Array.from(new Set(variants));
    }

    function qmkKeycodeSearchScore(entry, variants) {
        const terms = entry.searchTerms || [];
        const search = entry.search || "";
        const compact = entry.searchCompact || "";
        for (const variant of variants) {
            if (terms.includes(variant)) return 0;
        }
        for (const variant of variants) {
            if (terms.some((term) => term.startsWith(variant))) return 1;
        }
        for (const variant of variants) {
            if (search.includes(variant)) return 2;
        }
        for (const variant of variants.map(compactKeyPickerSearchToken).filter(Boolean)) {
            if (compact.includes(variant)) return 3;
        }
        return Number.POSITIVE_INFINITY;
    }

    function compactKeyPickerSearchToken(value) {
        return String(value || "").toLowerCase().replace(/[^a-z0-9]+/g, "");
    }

    function keyPickerSearchQuery() {
        return String(keyPicker?.search || "").trim();
    }

    function togglePickerModifier(modifier) {
        if (!keyPicker || !modifier) return;
        if (keyPicker.layerTapLayer) return;
        keyPicker.mods = keyPicker.mods.includes(modifier)
            ? keyPicker.mods.filter((candidate) => candidate !== modifier)
            : keyPicker.mods.concat([modifier]);
        renderKeyPicker();
    }

    function choosePickerKey(value) {
        if (!keyPicker || !value) return;
        if (value.startsWith(keyPickerLayerTapPrefix)) {
            if (keyPicker.mode === "list") return;
            keyPicker.layerTapLayer = value.slice(keyPickerLayerTapPrefix.length);
            keyPicker.mods = [];
            renderKeyPicker();
            return;
        }
        if (/^(MO|LOCK_LAYER|LT)\\(/.test(value)) {
            keyPicker.layerTapLayer = "";
        }
        if (keyPicker.mode === "list") {
            keyPicker.keys = keyPicker.keys.includes(value)
                ? keyPicker.keys.filter((candidate) => candidate !== value)
                : keyPicker.keys.concat([value]);
        } else {
            keyPicker.keys = [value];
        }
        renderKeyPicker();
    }

    function removePickerKey(index) {
        if (!keyPicker || !Number.isInteger(index)) return;
        keyPicker.keys = keyPicker.keys.filter((_, candidateIndex) => candidateIndex !== index);
        renderKeyPicker();
    }

    function keyPickerExpression() {
        if (!keyPicker) return "";
        if (keyPicker.mode === "list") {
            return keyPicker.keys.join(", ");
        }
        const key = keyPicker.keys[0] || "";
        if (!key) return "";
        if (keyPicker.layerTapLayer) {
            return "LT(" + keyPicker.layerTapLayer + ", " + key + ")";
        }
        return keyPicker.mods.concat([key]).join("+");
    }

    function confirmKeyPicker() {
        if (!keyPicker) return;
        const targetId = keyPicker.targetId;
        const layoutStageIndex = keyPicker.layoutStageIndex;
        const expression = keyPickerExpression();
        if (Number.isInteger(layoutStageIndex)) {
            const before = currentLocalSnapshot || serializeLocalState();
            closeKeyPicker();
            selectedKey = layoutStageIndex;
            if (expression && stageLayoutKey(layoutStageIndex, expression)) {
                render();
                commitLocalHistory(before);
            } else if (expression) {
                render();
            }
            return;
        }
        const input = document.getElementById(keyPicker.targetId);
        if (input) {
            input.value = expression;
            input.dispatchEvent(new Event("input", { bubbles: true }));
        }
        closeKeyPicker();
        if (targetId === "layoutComboInputs") {
            render();
        }
    }

    function updateHelperFields(prefix) {
        const enabled = Boolean(document.getElementById(prefix + "Enabled")?.checked);
        const helper = enabled ? (document.getElementById(prefix + "Helper")?.value || "") : "";
        for (const field of document.querySelectorAll("[data-helper-select-prefix='" + prefix + "']")) {
            field.hidden = !enabled;
            field.style.display = enabled ? "" : "none";
        }
        for (const field of document.querySelectorAll("[data-helper-action-prefix='" + prefix + "']")) {
            const visible = Boolean(helper);
            field.hidden = !visible;
            field.style.display = visible ? "" : "none";
            for (const control of field.querySelectorAll("input, select, textarea")) {
                if (visible) validateControl(control);
                else clearFieldError(control);
            }
        }
        for (const field of document.querySelectorAll("[data-helper-field][data-helper-prefix='" + prefix + "']")) {
            const values = String(field.dataset.helperValue || "").split(" ");
            const visible = values.includes(helper);
            field.hidden = !visible;
            field.style.display = visible ? "" : "none";
            for (const control of field.querySelectorAll("input, select, textarea")) {
                if (visible) validateControl(control);
                else clearFieldError(control);
            }
        }
    }

    function stepHasAction(step) {
        return Boolean(step?.tap || step?.hold || step?.longHold);
    }

    function editableActionValue(action) {
        if (!action) return "";
        return editableActionExpression(action.action);
    }

    function renderBoard(layer) {
        const layerColor = colorForLayer(layer.name);
        const viewBox = keyboardGeometry.layoutViewBox;
        const subtitle = layerColorSubtitle(layerColor);
        return "<div class='board layout-board-card'>" +
            renderLayoutBoardInfoButton() +
            "<div class='layout-board-header'>" +
            "<h3 class='layout-board-title'>" + escapeHtml(layer.name) + "</h3>" +
            "<p class='layout-board-subtitle' data-tooltip='Layer RGB summary: render mode, authored HSV color, and whether pass-through exposes the default RGB Matrix color.'>" + escapeHtml(subtitle) + "</p>" +
            "</div>" +
            "<div class='layout-board-stage'>" +
            "<svg class='keyboard-svg layout-board-svg' viewBox='" + viewBox.x + " " + viewBox.y + " " + viewBox.width + " " + viewBox.height + "' preserveAspectRatio='xMidYMid meet' role='img' aria-label='" + escapeAttr(layer.name + " keyboard layout") + "'>" +
            layer.positions.map(renderSvgKey).join("") +
            "</svg>" +
            "</div>" +
            renderLayoutBoardFooter(layer) +
            "</div>";
    }

    function renderLayoutBoardInfoButton() {
        return "<button type='button' class='layout-board-info' aria-label='Layout help' data-tooltip='" + escapeAttr(layoutBoardHelpTooltip()) + "'>i</button>";
    }

    function layoutBoardHelpTooltip() {
        return [
            "Click a physical key to edit its LAYOUT() slot.",
            "Double-click a key to open the keycode picker.",
            "Drag one key onto another to swap staged keycodes.",
            "Copy and paste work between selected keys.",
            "Dashed outlines are staged edits until Apply layout changes.",
            "Dots mark key_behaviors[] tap, hold, and long hold branches; combo badges mark active-layer combo inputs."
        ].join("\\n");
    }

    function renderLayoutBoardFooter(layer) {
        const count = pendingLayoutChangeCount();
        if (!count && !layoutNotice) return "";
        return "<div class='layout-board-footer'>" +
            (layoutNotice ? "<div class='layout-board-notice'>" + escapeHtml(layoutNotice) + "</div>" : "") +
            renderLayoutBoardApplyButton(count) +
            "</div>";
    }

    function renderLayoutBoardApplyButton(count) {
        if (!count) return "";
        const label = count === 1 ? "Apply layout change" : "Apply all " + count + " layout changes";
        return "<div class='layout-board-apply'>" +
            "<button type='button' data-action='applyLayoutChanges' class='primary dirty' aria-label='" + escapeAttr("Unsaved changes: " + label) + "'>" + escapeHtml(label) + "</button>" +
            "</div>";
    }

    function renderSvgKey(position) {
        const visual = keyVisual(position.layoutIndex);
        const label = position.display || position.keycode;
        const selected = selectedKey === position.layoutIndex;
        const comboSelected = layoutComboSelection.includes(position.layoutIndex);
        const style = keyStyle(position);
        const dots = behaviorDotsForKey(position.keycode);
        const badges = comboBadgesForKey(position.keycode);
        const keyFaceState = { hasBehavior: dots.length > 0, hasCombo: badges.length > 0 };
        const cx = visual.x + keyboardGeometry.keyWidth / 2;
        const cy = visual.y + keyboardGeometry.keyHeight / 2;
        const transform = visual.angle ? " transform='rotate(" + visual.angle + " " + cx + " " + cy + ")'" : "";
        const tooltipText = layoutComboPicking
            ? "Toggle combo input " + label + " (" + position.keycode + ")"
            : layoutKeyBehaviorTooltip(position, label);
        const action = layoutComboPicking ? "toggleLayoutComboKey" : "selectKey";
        const tooltipKind = layoutComboPicking ? "" : " data-tooltip-kind='layoutKey'";
        return "<g class='svg-key " + (selected ? "selected" : "") + (comboSelected ? " combo-input-selected" : "") + (position.pending ? " pending" : "") + "' tabindex='0' role='button' data-action='" + action + "' data-index='" + position.layoutIndex + "' data-keycode='" + escapeAttr(position.keycode) + "' data-tooltip='" + escapeAttr(tooltipText) + "'" + tooltipKind + transform + ">" +
            "<rect x='" + visual.x + "' y='" + visual.y + "' width='" + keyboardGeometry.keyWidth + "' height='" + keyboardGeometry.keyHeight + "' rx='" + keyboardGeometry.radius + "' fill='" + style.fill + "' stroke='" + style.stroke + "'></rect>" +
            renderLayoutSvgLabel(position, label, cx, cy, style.text, keyFaceState) +
            renderBehaviorDots(dots, visual, style.text, keyFaceState) +
            renderComboBadges(badges, visual, keyFaceState) +
            "</g>";
    }

    function layoutKeyBehaviorTooltip(position, label) {
        const lines = [
            "Index " + position.layoutIndex + " - " + label,
            "Keycode: " + position.keycode
        ];
        const behavior = behaviorForKey(position.keycode);
        if (!behavior) {
            lines.push("Custom key behavior: none on this key");
        } else {
            const branches = behaviorTooltipLines(behavior);
            const branchCount = behaviorTooltipBranchCount(behavior);
            const branchSuffix = branchCount ? " (" + branchCount + " " + (branchCount === 1 ? "branch" : "branches") + ")" : "";
            lines.push("Custom key behavior: " + displayAction(behavior.keycode) + branchSuffix);
            lines.push(...branches);
        }
        const comboLines = layoutKeyComboTooltipLines(position.keycode);
        if (comboLines.length) {
            lines.push("Combos:");
            lines.push(...comboLines);
        }
        const macroLines = layoutKeyMacroTooltipLines(position.keycode);
        if (macroLines.length) {
            lines.push("Macro payload:");
            lines.push(...macroLines);
        }
        return lines.join("\\n");
    }

    function layoutKeyMacroTooltipLines(keycode) {
        const lines = [];
        for (const macroKeycode of macroKeycodesInExpression(keycode)) {
            const slot = macroSlotForKeycode(macroKeycode);
            const payload = macroPayloadForSlot(slot);
            const parsed = payload ? parseMacroPayloadPreview(payload) : { steps: [], error: "" };
            const title = (slot?.kind === "via" || /^VIA_MACRO_/.test(macroKeycode || "") ? "VIA macro " : "Hardcoded macro ") + macroSlotNumber(macroKeycode);
            lines.push("  " + title + " (" + macroKeycode + ")");
            if (!slot) {
                lines.push("      No parsed payload for this keycode.");
            } else if (parsed.error) {
                lines.push("      Invalid payload: " + parsed.error);
            } else if (!parsed.steps.length) {
                lines.push("      Empty payload.");
            } else {
                for (const step of parsed.steps.slice(0, 4)) {
                    lines.push("      " + step.kindLabel + ": " + step.detail);
                }
                const moreCount = parsed.steps.length - 4;
                if (moreCount > 0) lines.push("      +" + moreCount + " more " + (moreCount === 1 ? "step" : "steps"));
            }
        }
        return lines;
    }

    function layoutKeyComboTooltipLines(keycode) {
        const lines = [];
        for (const combo of combosForKey(keycode)) {
            const inputText = (combo.inputDisplays || combo.inputs || []).join(" + ");
            const outputText = combo.outputDisplay || displayAction(combo.output);
            lines.push("  " + combo.badge + "  " + inputText + " -> " + outputText);

            const outputBehavior = behaviorForKey(combo.output);
            if (!outputBehavior) {
                lines.push("      custom output behavior: none");
                continue;
            }

            const branchCount = behaviorTooltipBranchCount(outputBehavior);
            const branchSuffix = branchCount ? " (" + branchCount + " " + (branchCount === 1 ? "branch" : "branches") + ")" : "";
            lines.push("      custom output behavior: " + displayAction(outputBehavior.keycode) + branchSuffix);
            lines.push(...behaviorTooltipLines(outputBehavior, { branchIndent: "        ", actionIndent: "            " }));
        }
        return lines;
    }

    function layoutKeyTooltipCardHtml(target) {
        const position = layoutKeyTooltipPosition(target);
        if (!position) return "";
        const label = position.display || position.keycode;
        const rawCode = label === position.keycode ? "" : position.keycode;
        const behavior = behaviorForKey(position.keycode);
        const combos = combosForKey(position.keycode);
        const macroKeycodes = macroKeycodesInExpression(position.keycode);
        const sections = [];

        if (behavior) {
            sections.push(renderLayoutKeyBehaviorSection("Custom key behavior", behavior, { title: behaviorBranchCountText(behavior) }));
        }
        if (macroKeycodes.length) {
            sections.push(renderLayoutKeyMacroSection(macroKeycodes));
        }
        if (combos.length) {
            sections.push(renderLayoutKeyComboSection(combos));
        }
        if (!sections.length) {
            sections.push("<div class='layout-key-empty'>No custom key behavior or active combos on this key.</div>");
        }

        return "<div class='layout-key-hover-card'>" +
            "<div class='layout-key-hover-head'>" +
            "<div class='layout-key-hover-title-row'>" +
            "<span class='layout-key-hover-title'>" + escapeHtml(label) + "</span>" +
            "<span class='layout-key-index-pill'>index " + escapeHtml(String(position.layoutIndex)) + "</span>" +
            "</div>" +
            (rawCode ? "<code class='layout-key-hover-code'>" + escapeHtml(rawCode) + "</code>" : "") +
            "</div>" +
            sections.join("") +
            "</div>";
    }

    function layoutKeyTooltipPosition(target) {
        const index = Number(target?.dataset?.index);
        if (!Number.isInteger(index)) return undefined;
        return currentLayer()?.positions?.find((position) => position.layoutIndex === index);
    }

    function renderLayoutKeyBehaviorSection(title, behavior, options = {}) {
        return "<div class='layout-key-section'>" +
            "<div class='layout-key-section-title'>" + escapeHtml(title) + "</div>" +
            "<div class='layout-key-section-body'>" +
            renderLayoutKeyBehaviorBlock(behavior, options) +
            "</div>" +
            "</div>";
    }

    function renderLayoutKeyBehaviorBlock(behavior, options = {}) {
        const title = options.title || behaviorTooltipTitle(behavior);
        return "<div class='layout-key-behavior-title'>" + escapeHtml(title) + "</div>" +
            renderLayoutKeyBehaviorRows(behavior);
    }

    function behaviorTooltipTitle(behavior) {
        return displayAction(behavior.keycode) + " " + behaviorBranchCountLabel(behavior);
    }

    function behaviorBranchCountLabel(behavior) {
        return "(" + behaviorBranchCountText(behavior) + ")";
    }

    function behaviorBranchCountText(behavior) {
        const branchCount = behaviorTooltipBranchCount(behavior);
        return branchCount ? branchCount + " " + (branchCount === 1 ? "branch" : "branches") : "no active branches";
    }

    function renderLayoutKeyBehaviorRows(behavior) {
        const rows = [];
        for (const step of behavior?.steps || []) {
            const actions = behaviorTooltipActionItems(step);
            if (!actions.length) continue;
            rows.push("<div class='layout-key-branch'>" +
                "<span class='layout-key-branch-count'>" + escapeHtml(String(Number(step.tapCount || 0) + 1)) + "x</span>" +
                "<div class='layout-key-actions'>" +
                actions.map(renderLayoutKeyAction).join("") +
                "</div>" +
                "</div>");
        }
        return rows.length ? "<div class='layout-key-branches'>" + rows.join("") + "</div>" : "<div class='layout-key-empty'>No active tap-count branch actions.</div>";
    }

    function renderLayoutKeyAction(action) {
        const className = "layout-key-action " + behaviorActionStageClass(action.label);
        return "<div class='" + escapeAttr(className) + "'>" +
            "<div class='layout-key-action-meta'>" +
            "<span class='layout-key-stage-chip'>" + escapeHtml(action.label) + "</span>" +
            "</div>" +
            "<div class='layout-key-action-main'>" +
            "<div class='layout-key-action-output'><span class='layout-key-action-target'>" + escapeHtml(action.target) + "</span></div>" +
            "<div class='layout-key-action-lifecycle'>" + escapeHtml(action.lifecycle) + "</div>" +
            renderLayoutKeyMacroPreview(action.sourceAction) +
            "</div>" +
            "</div>";
    }

    function renderLayoutKeyMacroSection(keycodes) {
        return "<div class='layout-key-section'>" +
            "<div class='layout-key-section-title'>Macro payload</div>" +
            "<div class='layout-key-section-body'>" +
            keycodes.map(renderLayoutKeyMacroCard).join("") +
            "</div>" +
            "</div>";
    }

    function renderLayoutKeyComboSection(combos) {
        return "<div class='layout-key-section'>" +
            "<div class='layout-key-section-title'>Combos</div>" +
            "<div class='layout-key-section-body'>" +
            "<div class='layout-key-combo-list'>" + combos.map(renderLayoutKeyComboCard).join("") + "</div>" +
            "</div>" +
            "</div>";
    }

    function renderLayoutKeyComboCard(combo) {
        const inputs = combo.inputDisplays || combo.inputs || [];
        const outputText = combo.outputDisplay || displayAction(combo.output);
        const outputBehavior = behaviorForKey(combo.output);
        return "<div class='layout-key-combo'>" +
            "<div class='layout-key-combo-flow'>" +
            "<span class='layout-key-combo-badge'>" + escapeHtml(combo.badge || "combo") + "</span>" +
            inputs.map((input, index) => (index ? "<span class='layout-key-combo-plus'>+</span>" : "") + renderLayoutKeyChip(input)).join("") +
            "<span class='layout-key-combo-arrow'>-></span>" +
            renderLayoutKeyChip(outputText, "output") +
            "</div>" +
            (outputBehavior ? "<div class='layout-key-nested-block'><div class='layout-key-section-title'>Custom output behavior</div>" + renderLayoutKeyBehaviorBlock(outputBehavior) + "</div>" : "") +
            "</div>";
    }

    function renderLayoutKeyChip(label, extraClass = "") {
        const className = "layout-key-chip" + (extraClass ? " " + extraClass : "");
        return "<span class='" + escapeAttr(className) + "'>" + escapeHtml(label || "") + "</span>";
    }

    function behaviorTooltipBranchCount(behavior) {
        return (behavior?.steps || []).filter((step) => step.tap || step.hold || step.longHold).length;
    }

    function behaviorTooltipLines(behavior, options = {}) {
        const branchIndent = options.branchIndent || "  ";
        const actionIndent = options.actionIndent || "      ";
        const lines = [];
        for (const step of behavior?.steps || []) {
            const actions = behaviorTooltipActionItems(step);
            if (actions.length) {
                const branchPrefix = branchIndent + (Number(step.tapCount || 0) + 1) + "x  ";
                actions.forEach((action, index) => {
                    lines.push((index === 0 ? branchPrefix : actionIndent) + action.label + ": " + action.text);
                });
            }
        }
        return lines.length ? lines : ["No active tap-count branch actions."];
    }

    function behaviorTooltipActionItems(step) {
        return [
            step.tap ? behaviorTooltipActionItem({ label: "tap", sourceAction: step.tap, step }) : undefined,
            step.hold ? behaviorTooltipActionItem({ label: "hold", sourceAction: step.hold, step }) : undefined,
            step.longHold ? behaviorTooltipActionItem({ label: "long hold", sourceAction: step.longHold, step }) : undefined,
        ].filter(Boolean);
    }

    function behaviorTooltipActionItem(item) {
        const lifecycle = behaviorTooltipActionLifecycle(item.sourceAction, item);
        const target = behaviorTooltipActionTarget(item.sourceAction) || lifecycle.fallbackTarget;
        return {
            label: item.label,
            text: behaviorTooltipActionText(item.sourceAction),
            target,
            lifecycle: lifecycle.text,
            sourceAction: item.sourceAction,
        };
    }

    function behaviorTooltipActionText(action) {
        const target = behaviorTooltipActionTarget(action);
        const lifecycle = behaviorTooltipActionLifecycle(action);
        return (target || lifecycle.fallbackTarget) + " - " + lifecycle.text;
    }

    function behaviorTooltipActionTarget(action) {
        return action?.actionDisplay || displayAction(action?.action || "");
    }

    function behaviorTooltipActionLifecycle(action, context = {}) {
        const stage = context.label || "";
        const stageStart = stage === "long hold" ? "long hold" : "hold";
        const hasLongHoldAlternative = stage === "hold" && Boolean(context.step?.longHold);
        const macroTarget = macroKeycodesInExpression(action?.action).length > 0;
        if (action?.helper === "TAP_SENDS") {
            return { text: "fires once on tap release", fallbackTarget: "tap" };
        }
        if (action?.helper === "PRESS_AND_HOLD_UNTIL_RELEASE") {
            if (macroTarget) {
                return { text: "fires once when " + stageStart + " starts", fallbackTarget: "hold" };
            }
            return { text: "starts at " + stageStart + ", stops on release", fallbackTarget: "hold" };
        }
        if (action?.helper === "TAP_AT_HOLD_THRESHOLD") {
            return { text: "fires once when " + stageStart + " starts", fallbackTarget: "threshold" };
        }
        if (action?.helper === "TAP_ON_RELEASE_AFTER_HOLD") {
            return {
                text: hasLongHoldAlternative ? "fires on release unless long hold starts" : "fires on release after hold",
                fallbackTarget: "release"
            };
        }
        if (action?.helper === "REPEAT_WHILE_HELD") {
            return {
                text: "starts at " + stageStart + ", repeats" + (action.repeatHz ? " at " + action.repeatHz + " Hz" : "") + " until release",
                fallbackTarget: "repeat"
            };
        }
        if (action?.helper) {
            return { text: action.helper, fallbackTarget: action.helper };
        }
        return { text: "runs this action", fallbackTarget: "action" };
    }

    function behaviorActionColor(label) {
        const feedback = model.rgb?.keyBehaviorFeedback || {};
        if (label === "tap") return hsvToHex(feedback.tapCommittedColor) || "#00d084";
        if (label === "hold") return hsvToHex(feedback.holdActiveColor) || "#ff8a00";
        if (label === "long hold") return hsvToHex(feedback.longHoldActiveColor) || "#3094ff";
        return "#8fb0bb";
    }

    function behaviorActionStageClass(label) {
        if (label === "tap") return "action-tap";
        if (label === "hold") return "action-hold";
        if (label === "long hold") return "action-long-hold";
        return "action-custom";
    }

    function updateLayoutKeyBehaviorColorStyle() {
        if (!behaviorColorStyle) return;
        const tapColor = behaviorActionColor("tap");
        const holdColor = behaviorActionColor("hold");
        const longHoldColor = behaviorActionColor("long hold");
        behaviorColorStyle.textContent = [
            layoutKeyBehaviorColorRule("tap", tapColor),
            layoutKeyBehaviorColorRule("hold", holdColor),
            layoutKeyBehaviorColorRule("long-hold", longHoldColor),
        ].join("\\n");
    }

    function layoutKeyBehaviorColorRule(name, color) {
        return ":root {" +
            "--layout-key-" + name + "-color: " + color + ";" +
            "--layout-key-" + name + "-text: " + idealText(color) + ";" +
            "--layout-key-" + name + "-wash: " + hexToRgba(color, 0.13) + ";" +
            "}";
    }

    function renderLayoutKeyMacroPreview(action) {
        const keycodes = macroKeycodesInExpression(action?.action);
        if (!keycodes.length) return "";
        return keycodes.map(renderLayoutKeyMacroCard).join("");
    }

    function renderLayoutKeyMacroCard(keycode) {
        const slot = macroSlotForKeycode(keycode);
        const payload = macroPayloadForSlot(slot);
        const parsed = payload ? parseMacroPayloadPreview(payload) : { steps: [], error: "" };
        const slotKind = slot?.kind === "via" || /^VIA_MACRO_/.test(keycode || "") ? "VIA macro" : "Hardcoded macro";
        const title = slotKind + " " + macroSlotNumber(keycode);
        const previewLimit = 4;
        const previewSteps = parsed.steps.slice(0, previewLimit);
        const moreCount = parsed.steps.length - previewSteps.length;
        return "<div class='layout-key-macro-preview'>" +
            "<div class='layout-key-macro-head'><span>" + escapeHtml(title) + "</span><code class='layout-key-macro-code'>" + escapeHtml(keycode) + "</code></div>" +
            (!slot ? "<div class='layout-key-macro-empty'>No parsed payload for this keycode.</div>" : "") +
            (parsed.error ? "<div class='layout-key-macro-empty'>Invalid payload: " + escapeHtml(parsed.error) + "</div>" : "") +
            (slot && !parsed.error && !previewSteps.length ? "<div class='layout-key-macro-empty'>Empty payload.</div>" : "") +
            (previewSteps.length ? "<div class='layout-key-macro-steps'>" + previewSteps.map(renderLayoutKeyMacroStep).join("") + "</div>" : "") +
            (moreCount > 0 ? "<div class='layout-key-macro-more'>+" + escapeHtml(String(moreCount)) + " more " + (moreCount === 1 ? "step" : "steps") + "</div>" : "") +
            "</div>";
    }

    function renderLayoutKeyMacroStep(step) {
        return "<div class='layout-key-macro-step'>" +
            "<span class='layout-key-macro-kind'>" + escapeHtml(step.kindLabel) + "</span>" +
            "<span class='layout-key-macro-detail'>" + escapeHtml(step.detail) + "</span>" +
            "</div>";
    }

    function macroKeycodesInExpression(expression) {
        const found = [];
        const seen = new Set();
        for (const match of String(expression || "").matchAll(/\\b(?:VIA_MACRO|MACRO)_\\d+\\b/g)) {
            if (seen.has(match[0])) continue;
            seen.add(match[0]);
            found.push(match[0]);
        }
        return found;
    }

    function hexToRgba(hex, alpha) {
        const match = String(hex || "").trim().match(/^#?([0-9a-f]{6})$/i);
        if (!match) return "rgba(143, 176, 187, " + alpha + ")";
        const value = match[1];
        const red = parseInt(value.slice(0, 2), 16);
        const green = parseInt(value.slice(2, 4), 16);
        const blue = parseInt(value.slice(4, 6), 16);
        return "rgba(" + red + ", " + green + ", " + blue + ", " + alpha + ")";
    }

    function keyVisual(layoutIndex) {
        if (keyboardGeometry.thumbs[layoutIndex]) {
            const thumb = keyboardGeometry.thumbs[layoutIndex];
            return {
                x: thumb.x,
                y: thumb.y + keyboardGeometry.yOffset,
                angle: thumb.angle
            };
        }
        const row = Math.floor(layoutIndex / 12);
        const col = layoutIndex % 12;
        if (col < 6) {
            return {
                x: keyboardGeometry.leftX[col],
                y: keyboardGeometry.leftTopY[col] + row * keyboardGeometry.rowStep + keyboardGeometry.yOffset,
                angle: 0
            };
        }
        const rightCol = col - 6;
        return {
            x: keyboardGeometry.rightX[rightCol],
            y: keyboardGeometry.rightTopY[rightCol] + row * keyboardGeometry.rowStep + keyboardGeometry.yOffset,
            angle: 0
        };
    }

    function renderLayoutSvgLabel(position, label, cx, cy, textColor, keyFaceState = {}) {
        const dualRole = dualRoleLayoutVisual(position?.keycode, position?.layoutIndex);
        if (dualRole) return renderDualRoleSvgLabel(dualRole, cx, cy, textColor, keyFaceState);
        const visual = keyVisual(position?.layoutIndex);
        const pair = shiftedOutputPair(position?.keycode);
        const topRowCount = keyFaceTopRowCount(keyFaceState);
        const rows = keyFaceRows(visual, keyFaceState);
        const baseY = topRowCount ? rows.baseY : cy;
        if (!pair) return renderSvgLabel(label, cx, baseY, textColor);
        if (pair.active === "shifted") {
            return renderSvgLabel(label, cx, baseY, textColor);
        }
        const pairRows = keyFaceRows(visual, { ...keyFaceState, hasShifted: true });
        return renderSvgLabel(pair.shifted, cx, pairRows.shiftedY, textColor, topRowCount ? shiftedRowOptions() : relaxedShiftedRowOptions()) +
            renderSvgLabel(pair.base, cx, pairRows.baseY, textColor, topRowCount ? baseRowOptions() : relaxedBaseRowOptions());
    }

    function renderDualRoleSvgLabel(dualRole, cx, cy, textColor, keyFaceState = {}) {
        const visual = keyVisual(dualRole.layoutIndex);
        const pair = shiftedOutputPair(dualRole.tapKeycode);
        const hasShiftedRow = pair && pair.active === "base";
        const topRowCount = keyFaceTopRowCount(keyFaceState);
        const rows = keyFaceRows(visual, { ...keyFaceState, hasShifted: hasShiftedRow, hasHold: true });
        const separator = "<line class='dual-role-separator-line' x1='" + (visual.x + 8) + "' y1='" + rows.separatorY + "' x2='" + (visual.x + keyboardGeometry.keyWidth - 8) + "' y2='" + rows.separatorY + "' stroke='" + escapeAttr(textColor || "#fff") + "' stroke-width='1'></line>";
        const dense = topRowCount > 0 || hasShiftedRow;
        const holdOptions = dense ? denseHoldRowOptions() : relaxedHoldRowOptions();
        if (hasShiftedRow) {
            return separator +
                renderSvgLabel(pair.shifted, cx, rows.shiftedY, textColor, topRowCount ? shiftedRowOptions() : relaxedShiftedRowOptions()) +
                renderSvgLabel(pair.base, cx, rows.baseY, textColor, topRowCount ? baseRowOptions() : relaxedBaseRowOptions()) +
                renderSvgLabel(dualRole.holdLabel, cx, rows.holdY, textColor, holdOptions);
        }
        const tapLabel = pair && pair.active === "shifted" ? pair.shifted : dualRole.tapLabel;
        return separator +
            renderSvgLabel(tapLabel, cx, rows.baseY, textColor, topRowCount ? baseRowOptions() : relaxedTapRowOptions()) +
            renderSvgLabel(dualRole.holdLabel, cx, rows.holdY, textColor, holdOptions);
    }

    function keyFaceRows(visual, state = {}) {
        const rows = { comboHeight: 8.2 };
        const topRowCount = keyFaceTopRowCount(state);
        const y = visual.y;
        if (state.hasBehavior && state.hasCombo) {
            rows.behaviorY = y + 6.3;
            rows.comboTopY = y + 14.2;
        } else if (state.hasBehavior) {
            rows.behaviorY = y + 7.5;
        } else if (state.hasCombo) {
            rows.comboTopY = y + 7.2;
        }

        const lowerRows = keyFaceLowerRows(y, topRowCount, Boolean(state.hasShifted), Boolean(state.hasHold));
        return { ...rows, ...lowerRows };
    }

    function keyFaceTopRowCount(state = {}) {
        return (state.hasBehavior ? 1 : 0) + (state.hasCombo ? 1 : 0);
    }

    function keyFaceLowerRows(y, topRowCount, hasShifted, hasHold) {
        if (hasHold && hasShifted) {
            return [
                { shiftedY: y + 19.8, baseY: y + 31.2, separatorY: y + 41.5, holdY: y + 51.5 },
                { shiftedY: y + 24.6, baseY: y + 35, separatorY: y + 44.6, holdY: y + 53 },
                { shiftedY: y + 30.3, baseY: y + 40.4, separatorY: y + 47.8, holdY: y + 54 },
            ][topRowCount];
        }
        if (hasHold) {
            return [
                { baseY: y + 24.5, separatorY: y + 37.4, holdY: y + 48.8 },
                { baseY: y + 29.3, separatorY: y + 40.8, holdY: y + 51.8 },
                { baseY: y + 32.2, separatorY: y + 43.4, holdY: y + 53.5 },
            ][topRowCount];
        }
        if (hasShifted) {
            return [
                { shiftedY: y + 23, baseY: y + 36 },
                { shiftedY: y + 26.6, baseY: y + 39 },
                { shiftedY: y + 30.3, baseY: y + 40.4 },
            ][topRowCount];
        }
        return [
            { baseY: y + keyboardGeometry.keyHeight / 2 },
            { baseY: y + 36 },
            { baseY: y + 40.4 },
        ][topRowCount];
    }

    function shiftedRowOptions() {
        return { className: "alternate-output-label", maxFontSize: 7.4, minFontSize: 5.6, maxWidth: keyboardGeometry.keyWidth - 18 };
    }

    function baseRowOptions() {
        return { maxFontSize: 9.1, minFontSize: 6.4, maxWidth: keyboardGeometry.keyWidth - 12 };
    }

    function relaxedShiftedRowOptions() {
        return { className: "alternate-output-label", maxFontSize: 8.4, minFontSize: 6, maxWidth: keyboardGeometry.keyWidth - 16 };
    }

    function relaxedBaseRowOptions() {
        return { maxFontSize: 10.5, minFontSize: 7, maxWidth: keyboardGeometry.keyWidth - 12 };
    }

    function relaxedTapRowOptions() {
        return { maxFontSize: 12, minFontSize: 7, maxWidth: keyboardGeometry.keyWidth - 12 };
    }

    function denseHoldRowOptions() {
        return { className: "dual-role-hold-label", maxFontSize: 6.4, minFontSize: 5.2, maxWidth: keyboardGeometry.keyWidth - 12 };
    }

    function relaxedHoldRowOptions() {
        return { className: "dual-role-hold-label", maxFontSize: 8, minFontSize: 6.2, maxWidth: keyboardGeometry.keyWidth - 12 };
    }

    function dualRoleLayoutVisual(expression, layoutIndex) {
        const call = layoutTopLevelCall(expression);
        if (!call) return undefined;
        if (call.helper === "LT" && call.args.length >= 2) {
            return {
                layoutIndex,
                tapKeycode: call.args[1],
                tapLabel: displayKeyExpression(call.args[1]),
                holdLabel: layerHoldLabel(call.args[0])
            };
        }
        if (call.helper === "MT" && call.args.length >= 2) {
            return {
                layoutIndex,
                tapKeycode: call.args[1],
                tapLabel: displayKeyExpression(call.args[1]),
                holdLabel: modifierArgumentLabel(call.args[0])
            };
        }
        if (call.helper.endsWith("_T") && call.args.length >= 1) {
            const helper = call.helper.slice(0, -2);
            const modifiers = modWrapperLabels[helper];
            if (modifiers) {
                return {
                    layoutIndex,
                    tapKeycode: call.args[0],
                    tapLabel: displayKeyExpression(call.args[0]),
                    holdLabel: modifiers.join("+")
                };
            }
        }
        return undefined;
    }

    function layerHoldLabel(layer) {
        return String(layer || "").replace(/^LAYER_/, "").replace(/_/g, " ").toUpperCase();
    }

    function layoutTopLevelCall(expression) {
        const normalized = normalizeDisplayExpression(expression);
        const open = normalized.indexOf("(");
        if (open <= 0 || !normalized.endsWith(")")) return undefined;
        const close = matchingLayoutParenIndex(normalized, open);
        if (close !== normalized.length - 1) return undefined;
        return {
            helper: normalized.slice(0, open),
            args: splitLayoutArguments(normalized.slice(open + 1, -1))
        };
    }

    function modifierArgumentLabel(expression) {
        const parts = normalizeDisplayExpression(expression).split(/\\s*\\|\\s*/).map(modifierAtomLabel).filter(Boolean);
        return parts.length ? parts.join("+") : displayKeyExpression(expression);
    }

    function modifierAtomLabel(expression) {
        const normalized = normalizeDisplayExpression(expression).replace(/^MOD_/, "");
        const direct = {
            LCTL: "Ctrl",
            LSFT: "Shift",
            LALT: "Alt",
            LGUI: "Cmd",
            RCTL: "Right Ctrl",
            RSFT: "Right Shift",
            RALT: "Right Alt",
            RGUI: "Right Cmd",
            MASK_CTRL: "Ctrl",
            MASK_SHIFT: "Shift",
            MASK_ALT: "Alt",
            MASK_GUI: "Cmd",
        };
        if (direct[normalized]) return direct[normalized];
        if (modWrapperLabels[normalized]) return modWrapperLabels[normalized].join("+");
        return "";
    }

    function shiftedOutputPair(keycode) {
        const key = shiftedOutputPairKeycode(keycode);
        if (!key) return undefined;
        if (shiftedKeyOutputLabels[key]) {
            return { base: displayKeyExpression(key), shifted: shiftedKeyOutputLabels[key], active: "base" };
        }
        if (shiftedKeyBaseLabels[key]) {
            return { base: shiftedKeyBaseLabels[key], shifted: displayKeyExpression(key), active: "shifted" };
        }
        return undefined;
    }

    function shiftedOutputPairKeycode(expression) {
        const normalized = normalizeDisplayExpression(expression);
        if (!normalized) return "";
        const canonical = canonicalLayoutKeyExpression(normalized);
        if (shiftedKeyOutputLabels[normalized] || shiftedKeyBaseLabels[normalized]) return normalized;
        if (shiftedKeyOutputLabels[canonical] || shiftedKeyBaseLabels[canonical]) return canonical;
        const open = normalized.indexOf("(");
        if (open <= 0 || !normalized.endsWith(")")) return "";
        const close = matchingLayoutParenIndex(normalized, open);
        if (close !== normalized.length - 1) return "";
        const helper = normalized.slice(0, open);
        const args = splitLayoutArguments(normalized.slice(open + 1, -1));
        if ((helper === "LT" || helper === "MT") && args.length >= 2) {
            return shiftedOutputPairKeycode(args[1]);
        }
        return "";
    }

    function alternateOutputTooltip(keycode) {
        const pair = shiftedOutputPair(keycode);
        if (!pair) return "";
        if (pair.active === "base") return "Shifted output: " + pair.shifted + " (" + shiftedChordLabel(pair.base) + " on a US layout).";
        return "This keycode sends " + shiftedChordLabel(pair.base) + ", producing " + pair.shifted + " on a US layout.";
    }

    function shiftedChordLabel(baseLabel) {
        return "Shift+" + baseLabel;
    }

    function renderSvgLabel(label, cx, cy, textColor, options = {}) {
        const clean = String(label || "").replace(/\\s+/g, " ").trim();
        if (!clean) return "";
        const maxWidth = options.maxWidth || keyboardGeometry.keyWidth - 10;
        const maxFontSize = options.maxFontSize || 12;
        const minFontSize = options.minFontSize || 7;
        const classAttr = options.className ? " class='" + escapeAttr(options.className) + "'" : "";
        const widthAtOnePx = svgTextWidthEstimate(clean, 1);
        const preferredFontSize = widthAtOnePx > 0 ? Math.min(maxFontSize, maxWidth / widthAtOnePx) : maxFontSize;
        const fontSize = Math.max(minFontSize, preferredFontSize);
        const constrained = widthAtOnePx * maxFontSize > maxWidth;
        const fitAttrs = constrained ? " textLength='" + svgNumber(maxWidth) + "' lengthAdjust='spacingAndGlyphs'" : "";
        return "<text" + classAttr + " x='" + svgNumber(cx) + "' y='" + svgNumber(cy) + "' font-size='" + svgNumber(fontSize) + "'" + fitAttrs + " fill='" + escapeAttr(textColor || "#e7ecef") + "'>" + escapeHtml(clean) + "</text>";
    }

    function svgTextWidthEstimate(text, fontSize) {
        let units = 0;
        for (const char of String(text || "")) {
            units += svgCharWidthUnit(char);
        }
        return units * fontSize;
    }

    function svgCharWidthUnit(char) {
        if (char === " ") return 0.32;
        if (/^[.,:;!'|]$/.test(char)) return 0.26;
        if (/^[/\\\\()[\\]{}]$/.test(char)) return 0.34;
        if (/^[MW@#%&]$/.test(char)) return 0.82;
        if (/^[A-Z0-9_]$/.test(char)) return 0.62;
        if (/^[a-z]$/.test(char)) return 0.54;
        return 0.58;
    }

    function svgNumber(value) {
        return Number(Number(value || 0).toFixed(2));
    }

    function colorForLayer(layerName) {
        const pending = pendingLayerAdd(layerName);
        if (pending?.rgbColor) {
            return { layer: pending.name, color: pending.rgbColor, mode: pending.rgbColor.mode || "KEYS_MAPPED_ON_THIS_LAYER_ONLY" };
        }
        return (model.rgb?.layerColors || []).find((row) => row.layer === layerName);
    }

    function layerColorIsPassthrough(color) {
        return numericChannel(color?.s) === 0 && numericChannel(color?.v) === 0;
    }

    function layerPreviewColor(layerColor) {
        if (!layerColor?.color) return undefined;
        if (!layerColorIsPassthrough(layerColor.color)) return layerColor.color;
        if (layerColor.layer === "LAYER_BASE") return model.rgb?.defaultColor || layerColor.color;
        return undefined;
    }

    function layerColorSubtitle(layerColor) {
        if (!layerColor) return "No layer RGB config parsed";
        const color = layerColor.color || {};
        const passthrough = layerColorIsPassthrough(color);
        const fallback = layerColor.layer === "LAYER_BASE" && passthrough && model.rgb?.defaultColor;
        const suffix = passthrough
            ? (fallback ? " • pass-through, showing default RGB " + fallback.expression : " • pass-through to lower/default RGB")
            : "";
        return "RGB matrix " + layerColor.mode + " • authored HSV(" + [color.h, color.s, color.v].join(", ") + ")" + suffix;
    }

    function keyStyle(position) {
        if (position.keycode === "_______") {
            return { fill: "#5f686d", stroke: "#87929a", text: "#f0f4f5" };
        }
        if (position.keycode === "XXXXXXX") {
            return { fill: "#aeb4b7", stroke: "#7f898e", text: "#293036" };
        }

        const layerColor = colorForLayer(activeLayer);
        const fill = layerKeyFill(position, layerColor);
        if (fill) {
            return { fill, stroke: shadeColor(fill, -26), text: idealText(fill) };
        }
        return { fill: "#f2f4f2", stroke: "#c7ceca", text: "#18201d" };
    }

    function layerKeyFill(position, layerColor) {
        if (!layerColor || !layerColor.color) return "";
        const mode = layerColor.mode;
        const isReal = position.keycode !== "_______" && position.keycode !== "XXXXXXX";
        if (mode === "KEYS_MAPPED_ON_THIS_LAYER_ONLY" && !isReal) return "";
        const previewColor = layerPreviewColor(layerColor);
        if (!previewColor) return "";
        const css = hsvToHex(previewColor);
        if (!css) return "";
        return css;
    }

    function behaviorDotsForKey(keycode) {
        const behavior = behaviorForKey(keycode);
        if (!behavior) return [];
        const counts = { tap: 0, hold: 0, longHold: 0 };
        for (const step of behavior.steps || []) {
            if (step.tap) counts.tap += 1;
            if (step.hold) counts.hold += 1;
            if (step.longHold) counts.longHold += 1;
        }
        const feedback = model.rgb?.keyBehaviorFeedback || {};
        return [
            counts.tap ? { kind: "tap", count: counts.tap, color: hsvToHex(feedback.tapCommittedColor) || "#00d084" } : undefined,
            counts.hold ? { kind: "hold", count: counts.hold, color: hsvToHex(feedback.holdActiveColor) || "#ff8a00" } : undefined,
            counts.longHold ? { kind: "long", count: counts.longHold, color: hsvToHex(feedback.longHoldActiveColor) || "#3094ff" } : undefined
        ].filter(Boolean);
    }

    function renderBehaviorDots(dots, visual, textColor, keyFaceState = {}) {
        if (!dots.length) return "";
        const rows = keyFaceRows(visual, { ...keyFaceState, hasBehavior: true });
        const radius = 3.8;
        const gap = 2;
        const step = radius * 2 + gap;
        const startX = visual.x + keyboardGeometry.keyWidth / 2 - ((dots.length - 1) * step) / 2;
        return dots.map((dot, index) => {
            const x = startX + index * step;
            const y = rows.behaviorY;
            const label = dot.count > 1 ? String(dot.count) : "";
            return "<circle cx='" + svgNumber(x) + "' cy='" + svgNumber(y) + "' r='" + radius + "' fill='" + dot.color + "' stroke='" + escapeAttr(textColor || "#fff") + "' stroke-width='1.1'></circle>" +
                (label ? "<text x='" + svgNumber(x) + "' y='" + svgNumber(y + 0.5) + "' fill='" + idealText(dot.color) + "' font-size='5.8' text-anchor='middle' dominant-baseline='central' font-weight='700'>" + label + "</text>" : "");
        }).join("");
    }

    function comboBadgesForKey(keycode) {
        return combosForKey(keycode).map((combo) => combo.badge);
    }

    function combosForKey(keycode) {
        return layerCombos(currentLayer()).filter((combo) => combo.inputs.some((input) => keyExpressionsEquivalent(input, keycode)));
    }

    function renderComboBadges(badges, visual, keyFaceState = {}) {
        if (!badges.length) return "";
        const rows = keyFaceRows(visual, { ...keyFaceState, hasCombo: true });
        const gap = 1.5;
        const naturalWidths = badges.map((badge) => Math.max(11.5, 5 + String(badge).length * 3.2));
        const available = keyboardGeometry.keyWidth - 8;
        const naturalTotal = naturalWidths.reduce((total, width) => total + width, 0) + gap * (badges.length - 1);
        const scale = naturalTotal > available ? Math.max(0.68, (available - gap * (badges.length - 1)) / naturalWidths.reduce((total, width) => total + width, 0)) : 1;
        const widths = naturalWidths.map((width) => width * scale);
        const totalWidth = widths.reduce((total, width) => total + width, 0) + gap * (badges.length - 1);
        let x = visual.x + keyboardGeometry.keyWidth / 2 - totalWidth / 2;
        const y = rows.comboTopY;
        const fontSize = Math.max(4.9, 5.8 * scale);
        return badges.map((badge, index) => {
            const width = widths[index];
            const textX = x + width / 2;
            const textY = y + rows.comboHeight / 2 + 0.1;
            const output = "<rect x='" + svgNumber(x) + "' y='" + svgNumber(y) + "' width='" + svgNumber(width) + "' height='" + rows.comboHeight + "' rx='3.5' fill='#141714' fill-opacity='0.9' stroke='#f5f5f3' stroke-opacity='0.86'></rect>" +
                "<text x='" + svgNumber(textX) + "' y='" + svgNumber(textY) + "' fill='#f5f5f3' font-size='" + svgNumber(fontSize) + "' text-anchor='middle' dominant-baseline='central' font-weight='700'>" + escapeHtml(badge) + "</text>";
            x += width + gap;
            return output;
        }).join("");
    }

    function behaviorForKey(keycode) {
        const canonical = canonicalKeyExpression(keycode);
        return model.keyBehaviors.find((behavior) => canonicalKeyExpression(behavior.keycode) === canonical);
    }

    function keyExpressionsEquivalent(left, right) {
        return canonicalKeyExpression(left) === canonicalKeyExpression(right);
    }

    function canonicalKeyExpression(value) {
        const normalized = normalizeDisplayExpression(value);
        if (!normalized) return "";
        if (qmkKeyAliases[normalized]) return qmkKeyAliases[normalized];

        const open = normalized.indexOf("(");
        if (open <= 0 || !normalized.endsWith(")")) return normalized;
        const close = matchingLayoutParenIndex(normalized, open);
        if (close !== normalized.length - 1) return normalized;

        const helper = normalized.slice(0, open);
        const args = splitLayoutArguments(normalized.slice(open + 1, -1)).map(canonicalKeyExpression);
        return helper + "(" + args.join(", ") + ")";
    }

    function splitLayoutArguments(text) {
        const items = [];
        let start = 0;
        let depthParen = 0;
        let depthBrace = 0;
        let depthBracket = 0;
        let quote = "";
        let escaped = false;

        for (let index = 0; index < text.length; index += 1) {
            const char = text[index];
            if (quote) {
                if (escaped) escaped = false;
                else if (char === "\\\\") escaped = true;
                else if (char === quote) quote = "";
                continue;
            }
            if (char === '"' || char === "'") {
                quote = char;
                continue;
            }
            if (char === "(") depthParen += 1;
            else if (char === ")") depthParen -= 1;
            else if (char === "{") depthBrace += 1;
            else if (char === "}") depthBrace -= 1;
            else if (char === "[") depthBracket += 1;
            else if (char === "]") depthBracket -= 1;
            else if (char === "," && depthParen === 0 && depthBrace === 0 && depthBracket === 0) {
                items.push(text.slice(start, index).trim());
                start = index + 1;
            }
        }

        items.push(text.slice(start).trim());
        return items.filter(Boolean);
    }

    function layoutComboSelectedPositions(layer) {
        normalizeLayoutComboState();
        const byIndex = new Map(layer.positions.map((position) => [position.layoutIndex, position]));
        return layoutComboSelection.map((index) => byIndex.get(index)).filter(Boolean);
    }

    function layerBehaviorRows(layer) {
        const rowsByKey = new Map();

        function addSource(keycode, source) {
            const behavior = behaviorForKey(keycode);
            if (!behavior) return;

            const canonical = canonicalKeyExpression(behavior.keycode);
            let row = rowsByKey.get(canonical);
            if (!row) {
                row = { behavior, sources: [] };
                rowsByKey.set(canonical, row);
            }
            row.sources.push(source);
        }

        for (const position of layer.positions) {
            addSource(position.keycode, { kind: "key", position });
        }
        for (const combo of layerCombos(layer)) {
            addSource(combo.output, { kind: "combo", combo });
            for (const input of combo.inputs || []) {
                addSource(input, { kind: "comboInput", combo, input });
            }
        }

        return Array.from(rowsByKey.values());
    }

    function renderLayerOverview(layer) {
        return "<div class='stack'>" +
            "<div><h3>Behaviors</h3>" + renderLayerBehaviorTable(layer) + "</div>" +
            "<div>" + renderLayerMacroTable(layer) + "</div>" +
            "<div>" + renderLayerComboTable(layer) + "</div>" +
            "<div>" + renderLayerPdModeTable(layer) + "</div>" +
            "</div>";
    }

    function renderTooltipHeader(label, tooltip) {
        return "<th data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(label) + "</th>";
    }

    function renderOverviewKeyButton(position) {
        const label = position.display || position.keycode;
        const tooltip = "Select layout index " + position.layoutIndex + " for editing: " + label + " (" + position.keycode + ").";
        return "<button data-action='selectKey' data-index='" + position.layoutIndex + "' data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(label) + "</button>";
    }

    function renderOverviewBehaviorSource(source) {
        if (source.kind === "key") {
            return renderOverviewKeyButton(source.position);
        }
        if (source.kind === "comboInput") {
            const combo = source.combo;
            const inputIndex = (combo.inputs || []).findIndex((input) => keyExpressionsEquivalent(input, source.input));
            const inputDisplay = (combo.inputDisplays || [])[inputIndex] || displayAction(source.input);
            const label = combo.badge + " input " + inputDisplay;
            const tooltip = "Combo " + combo.badge + " includes this behavior keycode as an input: " + (combo.inputDisplays || combo.inputs).join(" + ") + " -> " + (combo.outputDisplay || combo.output) + ".";
            return "<span class='source-pill' data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(label) + "</span>";
        }
        const combo = source.combo;
        const label = combo.badge + " output " + (combo.outputDisplay || combo.output);
        const tooltip = "Combo " + combo.badge + " output behavior from " + (combo.inputDisplays || combo.inputs).join(" + ") + " -> " + (combo.outputDisplay || combo.output) + ".";
        return "<span class='source-pill' data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(label) + "</span>";
    }

    function renderLayerBehaviorTable(layer) {
        const rows = layerBehaviorRows(layer);
        if (!rows.length) {
            return "<p class='muted'>No authored behavior rows are active on this layer.</p>";
        }
        return "<table><thead><tr>" +
            renderTooltipHeader("Reachable via", "Physical keys, combo inputs, or combo outputs on the active layer that use this key_behaviors[] row.") +
            renderTooltipHeader("Behavior", "Authored keycode that owns the key_behaviors[] row.") +
            renderTooltipHeader("Steps", "Tap-count branch actions attached to this behavior row.") +
            "</tr></thead><tbody>" +
            rows.map((row) =>
                "<tr><td>" + row.sources.map(renderOverviewBehaviorSource).join(" ") +
                "</td><td>" + escapeHtml(displayAction(row.behavior.keycode)) + "<br><code class='muted'>" + escapeHtml(row.behavior.keycode) + "</code></td><td>" +
                row.behavior.steps.map(renderStep).join("<br>") + "</td></tr>"
            ).join("") +
            "</tbody></table>";
    }

    function layerCombos(layer) {
        const keycodes = new Set(layer.positions.map((position) => canonicalKeyExpression(position.keycode)));
        const rows = [];
        for (const combo of model.combos) {
            if (combo.inputs.every((input) => keycodes.has(canonicalKeyExpression(input)))) {
                rows.push({ ...combo, badge: "C" + (rows.length + 1) });
            }
        }
        return rows;
    }

    function renderLayerComboTable(layer) {
        const combos = layerCombos(layer);
        if (!combos.length) {
            return "<h3>Combos</h3><p class='muted'>No combos resolve entirely from keys on this layer.</p>";
        }
        return "<h3>Combos</h3><table><thead><tr>" +
            renderTooltipHeader("Badge", "Small combo badge shown on the physical layout preview for this active-layer combo.") +
            renderTooltipHeader("Inputs", "Physical input keys that must be pressed together for this combo.") +
            renderTooltipHeader("Output", "Keycode emitted by this combo, plus an edit action for the combo row.") +
            renderTooltipHeader("Output behavior", "Behavior row that can run after the combo emits its output keycode.") +
            "</tr></thead><tbody>" +
            combos.map(renderLayerComboRow).join("") +
            "</tbody></table>";
    }

    function renderLayerComboRow(combo) {
        const behavior = behaviorForKey(combo.output);
        return "<tr><td><code>" + combo.badge + "</code></td>" +
            "<td>" + escapeHtml((combo.inputDisplays || combo.inputs).join(" + ")) + "</td>" +
            "<td><div class='table-cell-stack'><span>" + escapeHtml(combo.outputDisplay || combo.output) + "</span><code class='muted'>" + escapeHtml(combo.output) + "</code><div class='table-cell-actions'><button type='button' data-action='editLayoutCombo' data-badge='" + escapeAttr(combo.badge) + "'>Edit combo</button></div></div></td>" +
            "<td>" + renderComboOutputBehavior(combo, behavior) + "</td></tr>";
    }

    function renderComboOutputBehavior(combo, behavior) {
        const button = "<div class='table-cell-actions'><button type='button' data-action='editComboOutputBehavior' data-keycode='" + escapeAttr(combo.output) + "'>" + (behavior ? "Edit behavior" : "Create behavior") + "</button></div>";
        if (!behavior) {
            return "<div class='table-cell-stack'><span class='muted'>No key behavior row for this output.</span>" + button + "</div>";
        }
        return "<div class='table-cell-stack'>" + behavior.steps.map(renderStep).join("") + button + "</div>";
    }

    function renderLayerMacroTable(layer) {
        const rows = collectLayerMacros(layer);
        if (!rows.length) {
            return "<h3>Macros</h3><p class='muted'>No macro keycodes are directly placed or reached by visible behavior actions on this layer.</p>";
        }
        return "<h3>Macros</h3><table><thead><tr>" +
            renderTooltipHeader("Reachable via", "Visible key, combo output, or behavior action on this layer that can trigger the macro.") +
            renderTooltipHeader("Macro", "Macro keycode reached from the active layer.") +
            renderTooltipHeader("Payload", "Parsed VIA or hardcoded macro payload sent by that macro keycode.") +
            "</tr></thead><tbody>" +
            rows.map((row) =>
                "<tr><td>" + escapeHtml(row.source) + "</td><td>" + escapeHtml(row.display || row.keycode) + "<br><code class='muted'>" + escapeHtml(row.keycode) + "</code></td><td>" + renderMacroPayload(row.slot) + "</td></tr>"
            ).join("") +
            "</tbody></table>";
    }

    function renderMacroPayload(slot) {
        if (!slot) return "<span class='muted'>No parsed payload.</span>";
        const kind = slot.kind === "via" ? "VIA" : "Hardcoded";
        const payload = slot.payload ? escapeHtml(slot.payload) : "<span class='muted'>empty</span>";
        return "<code class='muted'>" + kind + "</code><br>" + payload;
    }

    function collectLayerMacros(layer) {
        const rows = [];
        const seen = new Set();
        for (const position of layer.positions) {
            addMacroCandidate(rows, seen, position.keycode, position.display);
            addBehaviorMacroCandidates(rows, seen, behaviorForKey(position.keycode), position.display);
        }
        for (const combo of layerCombos(layer)) {
            addMacroCandidate(rows, seen, combo.output, combo.badge + " output " + (combo.outputDisplay || combo.output));
            addBehaviorMacroCandidates(rows, seen, behaviorForKey(combo.output), combo.badge + " output " + (combo.outputDisplay || combo.output));
        }
        return rows;
    }

    function addBehaviorMacroCandidates(rows, seen, behavior, sourcePrefix) {
        for (const step of behavior?.steps || []) {
            for (const action of [step.tap, step.hold, step.longHold]) {
                if (action?.action) {
                    addMacroCandidate(rows, seen, action.action, sourcePrefix + " via " + step.tapCountName + " " + action.helper);
                }
            }
        }
    }

    function addMacroCandidate(rows, seen, keycode, source) {
        const slot = macroSlotForKeycode(keycode);
        if (!slot && !looksLikeMacroKeycode(keycode)) return;
        const key = keycode + source;
        if (seen.has(key)) return;
        seen.add(key);
        rows.push({
            source,
            keycode,
            display: displayAction(keycode),
            slot
        });
    }

    function macroSlotForKeycode(keycode) {
        return (model.viaMacros || []).concat(model.hardcodedMacros || []).find((slot) => slot.keycode === keycode);
    }

    function looksLikeMacroKeycode(keycode) {
        return /^VIA_MACRO_\d+$/.test(keycode || "") || /^MACRO_\d+$/.test(keycode || "");
    }

    function renderLayerPdModeTable(layer) {
        const rows = collectLayerPdModes(layer);
        if (!rows.length) {
            return "<h3>PD Modes</h3><p class='muted'>No pointing modes are directly placed or reached by visible behavior actions on this layer.</p>";
        }
        return "<h3>PD Modes</h3><table><thead><tr>" +
            renderTooltipHeader("Reachable via", "Visible key, combo output, or behavior action on this layer that can activate the pointing mode.") +
            renderTooltipHeader("Mode", "Pointing mode reached from this layer, with the action or lock keycode that activates it.") +
            renderTooltipHeader("RGB", "Pointing-mode feedback color and locality used when this mode is active.") +
            "</tr></thead><tbody>" +
            rows.map((row) => "<tr><td>" + escapeHtml(row.source) + "</td><td>" + renderPdModeCell(row) + "</td><td>" + renderInlineSwatch(row.color) + " <code class='muted'>" + escapeHtml(row.locality || "no override") + "</code></td></tr>").join("") +
            "</tbody></table>";
    }

    function renderPdModeCell(row) {
        const action = row.locked ? "lock action: " : "action: ";
        return escapeHtml(row.mode) + "<br><code class='muted'>" + action + escapeHtml(row.keycode) + "</code>";
    }

    function collectLayerPdModes(layer) {
        const rows = [];
        const seen = new Set();
        for (const position of layer.positions) {
            addPdModeCandidate(rows, seen, position.keycode, position.display);
            const behavior = behaviorForKey(position.keycode);
            addBehaviorPdModeCandidates(rows, seen, behavior, position.display);
        }
        for (const combo of layerCombos(layer)) {
            addPdModeCandidate(rows, seen, combo.output, combo.badge + " output " + (combo.outputDisplay || combo.output));
            addBehaviorPdModeCandidates(rows, seen, behaviorForKey(combo.output), combo.badge + " output " + (combo.outputDisplay || combo.output));
        }
        return rows;
    }

    function addBehaviorPdModeCandidates(rows, seen, behavior, sourcePrefix) {
        for (const step of behavior?.steps || []) {
            for (const action of [step.tap, step.hold, step.longHold]) {
                if (action?.action) {
                    addPdModeCandidate(rows, seen, action.action, sourcePrefix + " via " + step.tapCountName + " " + action.helper);
                }
            }
        }
    }

    function addPdModeCandidate(rows, seen, keycode, source) {
        if (!looksLikePdMode(keycode)) return;
        const mode = pdModeNameForKeycode(keycode);
        const key = mode + source;
        if (seen.has(key)) return;
        seen.add(key);
        const color = colorForPdMode(mode);
        rows.push({
            source,
            keycode,
            mode,
            locked: isPdModeLockKeycode(keycode),
            color: color?.color,
            locality: color?.locality
        });
    }

    function looksLikePdMode(keycode) {
        return /(^|_)MODE(_LOCK)?$/.test(keycode) || keycode === "DRAGSCROLL" || keycode === "DRAGSCROLL_LOCK";
    }

    function pdModeNameForKeycode(keycode) {
        if (keycode === "DRAGSCROLL" || keycode === "DRAGSCROLL_LOCK") return "PD_MODE_DRAGSCROLL";
        const base = keycode.replace(/_MODE_LOCK$/, "").replace(/_MODE$/, "").replace(/_LOCK$/, "");
        return "PD_MODE_" + base;
    }

    function isPdModeLockKeycode(keycode) {
        return /_MODE_LOCK$/.test(keycode || "") || keycode === "DRAGSCROLL_LOCK";
    }

    function colorForPdMode(mode) {
        return (model.rgb?.pdModeColors || []).find((row) => row.pointingMode === mode);
    }

    function renderBehaviorStudio() {
        return panel("Behavior Builder",
            renderBehaviorForm() +
            "<h3 style='margin-top: 14px'>Existing rows</h3>" +
            "<table><thead><tr><th>Keycode</th><th>Steps</th></tr></thead><tbody>" +
            model.keyBehaviors.map((row) =>
                "<tr><td>" + escapeHtml(displayAction(row.keycode)) + "<br><code class='muted'>" + escapeHtml(row.keycode) + "</code></td><td>" + row.steps.map(renderStep).join("<br>") + "</td></tr>"
            ).join("") +
            "</tbody></table>",
            true
        );
    }

    function renderBehaviorForm() {
        return "<div class='card' data-dirty-section>" +
            "<h3>Append simple single-tap branch row</h3>" +
            "<div class='form-grid'>" +
            "<label><span>Key</span><input id='behaviorKeycode' data-validate='layout-key' placeholder='A'></label>" +
            renderTimingInput("behaviorTapHoldTerm", "tap_hold_term", "", "") +
            renderActionInputs("tap", "Tap", ["", "TAP_SENDS"]) +
            renderActionInputs("hold", "Hold", ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            renderActionInputs("longHold", "Long hold", ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            "<button data-action='addBehavior' data-dirty-button class='primary'>Append behavior row</button>" +
            "</div></div>";
    }

    function renderActionInputs(prefix, label, helpers) {
        const hasRepeat = helpers.includes("REPEAT_WHILE_HELD");
        return renderHelperControl(prefix, label, helpers, "") +
            renderKeyPickerInput(prefix + "Action", label + " action", "", "Esc, Shift+\`, Cmd+Q", "single", "", "data-helper-action-prefix='" + escapeAttr(prefix) + "' hidden", "data-validate='layout-key'") +
            (hasRepeat ? "<label data-helper-field data-helper-prefix='" + prefix + "' data-helper-value='REPEAT_WHILE_HELD' hidden><span>" + label + " repeat Hz</span><input id='" + prefix + "Repeat' data-validate='positive-int' inputmode='numeric' placeholder='only for REPEAT_WHILE_HELD'></label>" : "");
    }

    function renderStep(step) {
        const parts = [];
        if (step.tap) parts.push("tap " + actionText(step.tap));
        if (step.hold) parts.push("hold " + actionText(step.hold));
        if (step.longHold) parts.push("long " + actionText(step.longHold));
        return "<code>" + escapeHtml(step.tapCountName + ": " + parts.join(", ")) + "</code>";
    }

    function actionText(action) {
        return action.helper + "(" + displayAction(action.actionDisplay || action.action) + (action.repeatHz ? ", " + action.repeatHz : "") + ")";
    }

    function readComboBuilder(target) {
        const builder = target.closest("[data-combo-builder]") || document;
        return {
            output: fieldValueFromMarker(builder, "[data-combo-output]"),
            inputs: fieldValueFromMarker(builder, "[data-combo-inputs]")
        };
    }

    function fieldValueFromMarker(root, selector) {
        const marked = root.querySelector(selector);
        if (!marked) return "";
        if ("value" in marked) return marked.value || "";
        return marked.querySelector("input, select, textarea")?.value || "";
    }

    function readBehaviorForm() {
        return {
            keycode: document.getElementById("behaviorKeycode").value,
            tapHoldTerm: document.getElementById("behaviorTapHoldTerm").value,
            tap: readAction("tap"),
            hold: readAction("hold"),
            longHold: readAction("longHold")
        };
    }

    function readSelectedBehaviorForm() {
        const steps = [];
        for (let index = 0; index < 5; index += 1) {
            steps.push({
                tapCount: index,
                tap: readAction("step" + index + "Tap"),
                hold: readAction("step" + index + "Hold"),
                longHold: readAction("step" + index + "LongHold")
            });
        }
        return {
            keycode: document.getElementById("selectedBehaviorKeycode").value,
            tapHoldTerm: document.getElementById("selectedTapHoldTerm").value,
            longerHoldTerm: document.getElementById("selectedLongerHoldTerm").value,
            multiTapTerm: document.getElementById("selectedMultiTapTerm").value,
            rgbBranchConfirmTerm: document.getElementById("selectedRgbBranchConfirmTerm").value,
            skipRgbBranchConfirm: Boolean(document.getElementById("selectedSkipRgbBranchConfirm")?.checked),
            steps
        };
    }

    function readAction(prefix) {
        const enabled = Boolean(document.getElementById(prefix + "Enabled")?.checked);
        if (!enabled) {
            return {
                helper: "",
                action: "",
                repeatHz: ""
            };
        }
        const helper = document.getElementById(prefix + "Helper")?.value || "";
        if (!helper) {
            return {
                helper: "",
                action: "",
                repeatHz: ""
            };
        }
        const repeatInput = document.getElementById(prefix + "Repeat");
        return {
            helper,
            action: document.getElementById(prefix + "Action").value,
            repeatHz: repeatInput ? repeatInput.value : ""
        };
    }

    function readRgbLedGroupBuilder(form) {
        const target = value(form, "target");
        const owner = value(form, "owner");
        const ledGroupName = rgbBuilderUsesReusableGroup() ? rgbLedGroupSource : "";
        return {
            target,
            owner,
            hue: value(form, "h"),
            sat: value(form, "s"),
            val: value(form, "v"),
            ledGroupName,
            ledIndices: ledGroupName ? [] : rgbSelectedLeds
        };
    }

    function readRgbReusableLedGroupDraft(form) {
        return {
            originalName: rgbReusableGroupOriginalName,
            name: form?.querySelector("#rgbReusableGroupName")?.value || rgbReusableGroupDraftName,
            ledIndices: rgbSelectedLeds
        };
    }

    function readConfigDefaultFields(section) {
        return Array.from(section?.querySelectorAll("[data-config-field]") || []).map((field) => {
            const macro = field.dataset.macro || "";
            const kind = field.dataset.kind || "";
            if (kind === "toggle") {
                return {
                    macro,
                    enabled: Boolean(field.querySelector("input[type='checkbox']")?.checked)
                };
            }
            return {
                macro,
                value: field.querySelector("input, select")?.value || ""
            };
        });
    }

    function renderDefaultsStudio() {
        const sections = model.configDefaults || [];
        if (!sections.length) {
            return panel("Defaults", "<p class='muted'>No config.h default fields were parsed.</p>", true);
        }
        return "<div class='stack'>" + sections.map(renderConfigDefaultSection).join("") + "</div>";
    }

    function renderConfigDefaultSection(section) {
        return "<details class='panel' open>" +
            "<summary><h2>" + escapeHtml(section.label || "Defaults") + "</h2></summary>" +
            "<div id='configDefaults-" + escapeAttr(section.id || "") + "' class='panel-body config-default-section' data-dirty-section data-config-section='" + escapeAttr(section.id || "") + "'>" +
            "<div class='config-default-grid'>" + (section.fields || []).map(renderConfigDefaultField).join("") + "</div>" +
            "<div class='toolbar'><button type='button' class='primary' data-action='updateConfigDefaults' data-dirty-button>Apply " + escapeHtml(section.label || "defaults") + "</button></div>" +
            "</div></details>";
    }

    function renderConfigDefaultField(field) {
        const tooltip = configDefaultTooltip(field);
        return "<div class='card config-default-field' data-config-field data-macro='" + escapeAttr(field.macro || "") + "' data-kind='" + escapeAttr(field.kind || "") + "' data-tooltip='" + escapeAttr(tooltip) + "'>" +
            "<div class='config-default-field-head'>" +
            "<span class='config-default-field-title'>" + escapeHtml(field.label || field.macro || "Default") + "</span>" +
            "<code class='muted'>" + escapeHtml(field.macro || "") + "</code>" +
            (field.hint ? "<span class='config-default-field-hint'>" + escapeHtml(field.hint) + "</span>" : "") +
            "</div>" +
            renderConfigDefaultControl(field) +
            "</div>";
    }

    function renderConfigDefaultControl(field) {
        const tooltip = configDefaultTooltip(field);
        if (field.kind === "toggle") {
            return "<label class='toggle-inline' data-tooltip='" + escapeAttr(tooltip) + "'>" +
                "<input type='checkbox' name='" + escapeAttr(field.macro || "") + "'" + (field.enabled ? " checked" : "") + " data-tooltip='" + escapeAttr(tooltip) + "'>" +
                "<span class='toggle-switch' aria-hidden='true'></span><span class='toggle-label'>enabled</span>" +
                "</label>";
        }
        if (field.kind === "layer") {
            let layers = layersForUi().map((layer) => [layer.name, layer.name]);
            const fieldValue = field.value || layers[0]?.[0] || "";
            if (fieldValue && !layers.some(([value]) => value === fieldValue)) {
                layers = [[fieldValue, fieldValue + " (missing)"]].concat(layers);
            }
            return "<label data-tooltip='" + escapeAttr(tooltip) + "'><span>default</span><select name='" + escapeAttr(field.macro || "") + "' data-validate='layer' data-tooltip='" + escapeAttr(tooltip) + "'>" + optionsWithLabels(layers, fieldValue) + "</select></label>";
        }
        const validate = configDefaultValidationRule(field);
        const inputMode = configDefaultInputMode(field);
        return "<label data-tooltip='" + escapeAttr(tooltip) + "'><span>default</span><input name='" + escapeAttr(field.macro || "") + "' value='" + escapeAttr(field.value || "") + "' spellcheck='false' data-tooltip='" + escapeAttr(tooltip) + "'" +
            (inputMode ? " inputmode='" + escapeAttr(inputMode) + "'" : "") +
            (validate ? " data-validate='" + escapeAttr(validate) + "'" : "") +
            "></label>";
    }

    function configDefaultTooltip(field) {
        return field.tooltip || ("Edits " + (field.macro || "this config.h macro") + " in config.h.");
    }

    function configDefaultValidationRule(field) {
        if (field.validate === "timing-ms") return "timing-ms";
        if (["positive-int", "nonnegative-int", "uint8", "identifier", "safe-expression", "layer"].includes(field.validate)) return field.validate;
        return "";
    }

    function configDefaultInputMode(field) {
        if (["timing-ms", "positive-int", "nonnegative-int", "uint8"].includes(field.validate)) return "numeric";
        return "";
    }

    function renderRgbStudio() {
        const rgb = model.rgb || {};
        return "<div class='stack'>" +
            panel("RGB LED Group Builder", renderRgbBuilderWorkspace(rgb), true) +
            panel("Layer Colors", renderLayerRgbSection(rgb), true) +
            panel("Auto-mouse Fade", renderAutomouseCard(rgb.automouseFade), false) +
            panel("Pointing-mode Colors", renderPdModeRgbSection(rgb), true) +
            panel("Combo Feedback", renderComboFeedbackSection(rgb), false) +
            panel("Key Behavior Feedback", renderKeyBehaviorFeedbackSection(rgb), true) +
            "</div>";
    }

    function renderRgbBuilderWorkspace(rgb) {
        return "<div class='rgb-builder-workspace'>" +
            renderRgbGroupBuilder() +
            renderReusableLedGroupsSection(rgb) +
            "</div>";
    }

    function renderRgbGroupBuilder() {
        normalizeRgbGroupState();
        const layer = currentLayer();
        const targetOptions = [
            ["layer", "Layer LED groups"],
            ["pdMode", "Pointing-mode LED groups"],
            ["combo", "Combo feedback LED groups"],
            ["keyBehavior", "Key-behavior LED groups"]
        ];
        const ownerChoices = rgbGroupOwnerOptions(rgbGroupTarget);
        const ownerTooltip = rgbGroupOwnerTooltip(rgbGroupTarget);
        const ownerControl = ownerChoices.length
            ? "<label data-tooltip='" + escapeAttr(ownerTooltip) + "'><span>" + escapeHtml(rgbGroupOwnerLabel(rgbGroupTarget)) + "</span><select name='owner' data-tooltip='" + escapeAttr(ownerTooltip) + "'>" + optionsWithLabels(ownerChoices, rgbGroupOwner) + "</select></label>"
            : "<label data-tooltip='Combo feedback LED group rows apply to the shared combo feedback stage, so no separate owner is needed.'><span>owner</span><input name='owner' disabled value='combo feedback' data-tooltip='Combo feedback LED group rows apply to the shared combo feedback stage, so no separate owner is needed.'></label>";
        const pendingLedIndices = effectiveRgbBuilderLedIndices();
        const selected = pendingLedIndices.length
            ? pendingLedIndices.map((led) => "<code>" + led + "</code>").join("")
            : "<span class='muted'>No LEDs selected</span>";
        const selectedLabel = rgbBuilderUsesReusableGroup() ? rgbLedGroupSource : "new row";
        const definedLedIndices = rgbBuilderDefinedLedIndices();
        const defined = definedLedIndices.length
            ? definedLedIndices.map((led) => "<code>" + led + "</code>").join("")
            : "<span class='muted'>No defined LEDs for this table</span>";
        return "<div id='rgbGroupBuilder' class='rgb-builder-card' data-dirty-section>" +
            renderLayerTabs(false, false) +
            "<div class='layout-with-key-editor rgb-builder-layout'>" +
            renderRgbGroupBoard(layer) +
            "<div class='layout-selected-key-column'>" +
            "<div class='layout-sidecar-stack'>" +
            "<div class='card selected-key-edit-card rgb-builder-sidecar'>" +
            "<h3>LED group row</h3>" +
            "<div class='selected-key-edit-fields'>" +
            "<div class='form-grid four'>" +
            "<label><span>table</span><select name='target'>" + optionsWithLabels(targetOptions, rgbGroupTarget) + "</select></label>" +
            ownerControl +
            renderRgbLedGroupSourceControl() +
            "</div>" +
            renderRgbBuilderColorControl() +
            "<div class='rgb-selected-list' data-tooltip='" + escapeAttr(rgbBuilderSelectionTooltip()) + "'><span class='rgb-led-list-label'>" + escapeHtml(selectedLabel) + "</span>" + selected + "</div>" +
            "<div class='rgb-selected-list rgb-defined-list' data-tooltip='" + escapeAttr(rgbBuilderDefinedTooltip()) + "'><span class='rgb-led-list-label'>defined</span>" + defined + "</div>" +
            "<div class='toolbar'>" +
            "<button data-action='clearRgbSelection'>Clear Selected LEDs</button>" +
            "<button data-action='addRgbLedGroup' data-dirty-button class='primary'>Add LED group row</button>" +
            "</div>" +
            "</div>" +
            "</div>" +
            "</div>" +
            "</div>" +
            "</div>" +
            "</div>";
    }

    function renderRgbLedGroupSourceControl() {
        const reusableOptions = rgbReusableLedGroups().map((group) => [
            group.name,
            group.name + " (" + numericLedIndices(group.ledIndices).length + " LEDs)"
        ]);
        const options = [["inline", "Inline LED selection"]].concat(reusableOptions);
        return "<label><span>LED group</span><select name='ledGroupSource'>" + optionsWithLabels(options, rgbLedGroupSource) + "</select></label>";
    }

    function rgbGroupOwnerTooltip(target) {
        if (target === "layer") return "Layer or all-layer owner for the new layer_led_groups_data[] row.";
        if (target === "pdMode") return "Pointing mode or all-modes owner for the new pd_mode_led_groups_data[] row.";
        if (target === "keyBehavior") return "Feedback semantic that owns the new key_behavior_feedback_led_groups_data[] row.";
        return "Owner for the selected RGB LED group table.";
    }

    function rgbBuilderSelectionTooltip() {
        if (rgbBuilderUsesReusableGroup()) {
            return "LEDs from the selected reusable RGB_LED_GROUP_* definition. The new table row references that reusable group instead of writing inline LED indices.";
        }
        return "Inline LED indices selected on the board for the new table row. These are written as RGB_LED_GROUP(...).";
    }

    function rgbBuilderDefinedTooltip() {
        return "LED indices already covered by enabled rows in the selected target table. They are previewed on the board but are not automatically selected for the new row.";
    }

    function renderReusableLedGroupsSection(rgb) {
        const groups = rgb.ledGroups || [];
        const selected = rgbSelectedLeds.length
            ? rgbSelectedLeds.map((led) => "<code>" + led + "</code>").join("")
            : "<span class='muted'>Select LEDs on the RGB group builder board.</span>";
        const editing = Boolean(rgbReusableGroupOriginalName);
        return "<div id='rgbReusableGroups' class='card' data-dirty-section>" +
            "<h3 class='rgb-builder-subsection-title'>Reusable LED Groups</h3>" +
            "<div class='form-grid four'>" +
            "<label><span>group name</span><input id='rgbReusableGroupName' data-validate='rgb-led-group-name' value='" + escapeAttr(rgbReusableGroupDraftName) + "' placeholder='RGB_LED_GROUP_THUMBS' spellcheck='false'></label>" +
            "<div class='rgb-selected-list' data-tooltip='LED indices currently selected on the builder board. Saving a reusable group stores this LED membership only; color and owner stay in table rows.'><span class='rgb-led-list-label'>selected LEDs</span>" + selected + "</div>" +
            "<button type='button' data-action='clearReusableLedGroupDraft'>Clear editor</button>" +
            "<button type='button' data-action='saveRgbReusableLedGroup' data-dirty-button class='primary'>" + (editing ? "Save group" : "Create group") + "</button>" +
            "</div>" +
            (editing ? "<p class='muted' data-tooltip='Saving replaces the reusable group definition with the current name and selected LEDs. Existing table rows that reference the group keep using the updated LED set.'>Editing <code>" + escapeHtml(rgbReusableGroupOriginalName) + "</code>; selected LEDs replace the reusable group definition.</p>" : "") +
            renderReusableLedGroupsTable(groups) +
            "</div>";
    }

    function renderReusableLedGroupsTable(groups) {
        if (!groups.length) {
            return "<p class='muted'>No reusable RGB_LED_GROUP_* definitions were parsed.</p>";
        }
        return "<table><thead><tr>" +
            renderTooltipHeader("Group", "Reusable RGB_LED_GROUP_* definition name and source expression from rgb_config.c.") +
            renderTooltipHeader("LEDs", "Physical LED indices stored by this reusable group.") +
            renderTooltipHeader("Used by", "Enabled LED group table rows that reference this reusable definition.") +
            renderTooltipHeader("Actions", "Load, use, or delete this reusable LED group definition.") +
            "</tr></thead><tbody>" +
            groups.map(renderReusableLedGroupRow).join("") +
            "</tbody></table>";
    }

    function renderReusableLedGroupRow(group) {
        const leds = numericLedIndices(group.ledIndices);
        const deleteDisabled = group.usageCount > 0 ? " disabled" : "";
        return "<tr>" +
            "<td><code>" + escapeHtml(group.name) + "</code><div class='muted'>" + escapeHtml(group.expression || "") + "</div></td>" +
            "<td><code>" + escapeHtml(leds.join(", ")) + "</code><div class='muted'>" + leds.length + " LEDs</div></td>" +
            "<td>" + renderReusableLedGroupUsages(group) + "</td>" +
            "<td><div class='toolbar'>" +
            "<button type='button' data-action='useReusableLedGroup' data-led-group='" + escapeAttr(group.name) + "'>Use in row</button>" +
            "<button type='button' data-action='editReusableLedGroup' data-led-group='" + escapeAttr(group.name) + "'>Edit</button>" +
            "<button type='button' data-action='deleteRgbReusableLedGroup' data-led-group='" + escapeAttr(group.name) + "'" + deleteDisabled + ">Delete</button>" +
            "</div></td>" +
            "</tr>";
    }

    function renderReusableLedGroupUsages(group) {
        const usages = group.usages || [];
        if (!usages.length) return "<span class='muted'>unused</span>";
        return usages.map((usage) => "<div data-tooltip='" + escapeAttr("This reusable LED group is referenced by " + reusableLedGroupUsageLabel(usage) + " in an enabled LED group table row.") + "'><code>" + escapeHtml(reusableLedGroupUsageLabel(usage)) + "</code>" +
            (usage.color ? " <code class='muted'>" + escapeHtml(usage.color) + "</code>" : "") +
            "</div>").join("");
    }

    function reusableLedGroupUsageLabel(usage) {
        const owner = usage.owner || "";
        if (usage.target === "layer") return owner === rgbLayerAllGroups ? "all layers" : owner;
        if (usage.target === "pdMode") return owner === rgbPdModeAllGroups ? "all pointing modes" : owner;
        if (usage.target === "combo") return "combo feedback";
        if (usage.target === "keyBehavior") return owner === keyBehaviorAllGroups ? "all feedback groups" : keyBehaviorRgbSemanticLabel(owner);
        return owner || usage.target || "unknown";
    }

    function rgbGroupOwners(target) {
        if (target === "layer") return [rgbLayerAllGroups].concat(layersForUi().map((layer) => layer.name));
        if (target === "pdMode") return [rgbPdModeAllGroups].concat((model.rgb?.pdModeColors || []).map((row) => row.pointingMode));
        if (target === "keyBehavior") return keyBehaviorRgbSemantics;
        return [];
    }

    function rgbGroupOwnerOptions(target) {
        if (target === "layer") {
            return rgbGroupOwners(target).map((owner) => [owner, owner === rgbLayerAllGroups ? "All layers" : owner]);
        }
        if (target === "pdMode") {
            return rgbGroupOwners(target).map((owner) => [owner, owner === rgbPdModeAllGroups ? "All pointing modes" : owner]);
        }
        if (target === "keyBehavior") {
            return keyBehaviorRgbSemantics.map((semantic) => [semantic, keyBehaviorRgbSemanticLabel(semantic)]);
        }
        return rgbGroupOwners(target).map((owner) => [owner, owner]);
    }

    function rgbGroupOwnerLabel(target) {
        if (target === "pdMode") return "pointing mode";
        if (target === "keyBehavior") return "semantic";
        return "layer";
    }

    function rgbBuilderGroupRowsForTarget(target) {
        const rgb = model.rgb || {};
        if (target === "layer") return rgb.layerLedGroups || [];
        if (target === "pdMode") return rgb.pdModeLedGroups || [];
        if (target === "combo") return rgb.comboFeedbackLedGroups || [];
        if (target === "keyBehavior") return rgb.keyBehaviorFeedbackLedGroups || [];
        return [];
    }

    function rgbBuilderDefinedRows() {
        return rgbBuilderGroupRowsForTarget(rgbGroupTarget);
    }

    function rgbReusableLedGroups() {
        return model.rgb?.ledGroups || [];
    }

    function rgbReusableLedGroupByName(name) {
        return rgbReusableLedGroups().find((group) => group.name === name);
    }

    function rgbBuilderUsesReusableGroup() {
        return Boolean(rgbLedGroupSource && rgbLedGroupSource !== "inline" && rgbReusableLedGroupByName(rgbLedGroupSource));
    }

    function effectiveRgbBuilderLedIndices() {
        if (rgbBuilderUsesReusableGroup()) {
            return numericLedIndices(rgbReusableLedGroupByName(rgbLedGroupSource)?.ledIndices || []);
        }
        return numericLedIndices(rgbSelectedLeds);
    }

    function numericLedIndices(values) {
        const seen = new Set();
        const result = [];
        for (const value of Array.isArray(values) ? values : []) {
            const index = Number(value);
            if (!Number.isInteger(index) || seen.has(index)) {
                continue;
            }
            seen.add(index);
            result.push(index);
        }
        return result;
    }

    function rgbBuilderDefinedLedIndices() {
        const seen = new Set();
        for (const row of rgbBuilderDefinedRows()) {
            for (const led of row.ledIndices || []) {
                const ledIndex = Number(led);
                if (Number.isInteger(ledIndex)) {
                    seen.add(ledIndex);
                }
            }
        }
        return Array.from(seen).sort((left, right) => left - right);
    }

    function rgbBuilderDefinedPreviewForLed(ledIndex) {
        const rows = rgbBuilderDefinedRows();
        const matchingRows = rows.filter((row) => rgbLedRowContainsLed(row, ledIndex));
        if (!matchingRows.length) return undefined;
        if (rgbGroupTarget === "keyBehavior") {
            const gradientRows = rgbBuilderDefinedFeedbackGradientRows(ledIndex);
            if (gradientRows.length > 1) {
                return {
                    fill: "url(#rgb-feedback-defined-gradient-" + ledIndex + ")",
                    text: "#ffffff",
                    allFeedback: true
                };
            }
        }
        const row = matchingRows[matchingRows.length - 1];
        const previewColor = rgbGroupColorInherits(row.color) ? rgbLedGroupInheritedColor(row, rgbGroupTarget) : row.color;
        const fill = hsvToHex(previewColor) || rgbBuilderHex();
        return {
            fill,
            text: idealText(fill),
            allFeedback: false
        };
    }

    function rgbLedRowContainsLed(row, ledIndex) {
        return (row.ledIndices || []).some((led) => Number(led) === ledIndex);
    }

    function rgbBuilderDefinedFeedbackGradientRows(ledIndex) {
        if (rgbGroupTarget !== "keyBehavior") return [];
        const matchingRows = rgbBuilderDefinedRows().filter((row) => rgbLedRowContainsLed(row, ledIndex));
        if (!matchingRows.length) return [];
        const hasAll = matchingRows.some((row) => row.owner === keyBehaviorAllGroups);
        const specificRows = matchingRows.filter((row) => row.owner !== keyBehaviorAllGroups);
        if (!hasAll && specificRows.length < 2) return [];
        const rows = hasAll
            ? keyBehaviorRgbSemanticColorRows()
            : keyBehaviorRgbSemanticColorRows().filter((row) => specificRows.some((specific) => specific.owner === row.semantic));
        for (const specific of specificRows) {
            for (const target of rows.filter((row) => row.semantic === specific.owner)) {
                target.color = specific.color;
            }
        }
        return rows;
    }

    function renderRgbBuilderColorControl() {
        if (rgbGroupTarget === "keyBehavior" && rgbGroupOwner === keyBehaviorAllGroups && rgbGroupColorInherits(defaultRgbBuilderColor())) {
            const rows = keyBehaviorRgbSemanticColorRows();
            return "<div class='color-control'>" +
                "<div class='muted'>KEY_FEEDBACK_GROUP_ALL with HSV(0, 0, 0) inherits the active feedback semantic color at render time.</div>" +
                "<input name='h' type='hidden' value='0'><input name='s' type='hidden' value='0'><input name='v' type='hidden' value='0'>" +
                "<div class='rgb-all-color-grid'>" + rows.map((row) =>
                    "<div class='rgb-all-color-chip'>" +
                    renderInlineSwatch(row.color) +
                    "<span>" + escapeHtml(row.label) + "</span>" +
                    "<code class='muted'>" + escapeHtml(colorExpression(row.color)) + "</code>" +
                    "</div>"
                ).join("") + "</div>" +
                "</div>";
        }
        return renderHsvColorControl(defaultRgbBuilderColor());
    }

    function defaultRgbBuilderColor() {
        if (rgbBuilderColor) {
            return rgbBuilderColor;
        }
        if (rgbGroupOwner === rgbLayerAllGroups || rgbGroupOwner === rgbPdModeAllGroups || rgbGroupOwner === keyBehaviorAllGroups) {
            return { h: "0", s: "0", v: "0" };
        }
        if (rgbGroupTarget === "layer") {
            return colorForLayer(rgbGroupOwner || activeLayer)?.color || { h: "0", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
        }
        if (rgbGroupTarget === "pdMode") {
            return colorForPdMode(rgbGroupOwner)?.color || { h: "0", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
        }
        if (rgbGroupTarget === "combo") {
            return model.rgb?.comboFeedback?.color || { h: "191", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
        }
        if (rgbGroupTarget === "keyBehavior" && rgbGroupOwner === keyBehaviorAllGroups) {
            return keyBehaviorSemanticColor("KEY_FEEDBACK_GROUP_TAP_COMMITTED") || { h: "0", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
        }
        return keyBehaviorSemanticColor(rgbGroupOwner) || { h: "0", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
    }

    function keyBehaviorRgbSemanticLabel(semantic) {
        return keyBehaviorRgbSemanticLabels[semantic] || semantic;
    }

    function keyBehaviorRgbSemanticColorRows() {
        const feedback = model.rgb?.keyBehaviorFeedback || {};
        const fallback = { h: "0", s: "255", v: "RGB_MATRIX_MAXIMUM_BRIGHTNESS" };
        const rows = [
            {
                semantic: "KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH",
                label: keyBehaviorRgbSemanticLabel("KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH"),
                color: feedback.tapPendingColor || fallback
            }
        ];
        const branchColors = feedback.tapBranchColors || [];
        if (branchColors.length) {
            for (let index = 0; index < branchColors.length; index += 1) {
                rows.push({
                    semantic: "KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED",
                    label: "Tap count " + (index + 2) + " branch committed",
                    color: branchColors[index] || fallback
                });
            }
        } else {
            rows.push({
                semantic: "KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED",
                label: keyBehaviorRgbSemanticLabel("KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED"),
                color: fallback
            });
        }
        rows.push(
            {
                semantic: "KEY_FEEDBACK_GROUP_TAP_COMMITTED",
                label: keyBehaviorRgbSemanticLabel("KEY_FEEDBACK_GROUP_TAP_COMMITTED"),
                color: feedback.tapCommittedColor || fallback
            },
            {
                semantic: "KEY_FEEDBACK_GROUP_HOLD_ACTIVE",
                label: keyBehaviorRgbSemanticLabel("KEY_FEEDBACK_GROUP_HOLD_ACTIVE"),
                color: feedback.holdActiveColor || fallback
            },
            {
                semantic: "KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE",
                label: keyBehaviorRgbSemanticLabel("KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE"),
                color: feedback.longHoldActiveColor || fallback
            }
        );
        return rows;
    }

    function keyBehaviorSemanticColor(semantic) {
        const feedback = model.rgb?.keyBehaviorFeedback || {};
        return {
            KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH: feedback.tapPendingColor,
            KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED: (feedback.tapBranchColors || [])[0],
            KEY_FEEDBACK_GROUP_TAP_COMMITTED: feedback.tapCommittedColor,
            KEY_FEEDBACK_GROUP_HOLD_ACTIVE: feedback.holdActiveColor,
            KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: feedback.longHoldActiveColor
        }[semantic];
    }

    function rgbBuilderHex() {
        return hsvToHex(rgbBuilderPreviewColor()) || "#000000";
    }

    function rgbBuilderPreviewColor() {
        const color = defaultRgbBuilderColor();
        if (rgbGroupColorInherits(color)) {
            return rgbLedGroupInheritedColor({ owner: rgbGroupOwner }, rgbGroupTarget) || baseLayerVisiblePreviewColor() || color;
        }
        return color;
    }

    function rgbBuilderUsesAllFeedbackPreview() {
        return rgbGroupTarget === "keyBehavior" && rgbGroupOwner === keyBehaviorAllGroups && rgbGroupColorInherits(defaultRgbBuilderColor());
    }

    function rgbBuilderPreviewFill() {
        return rgbBuilderUsesAllFeedbackPreview() ? "url(#rgb-feedback-all-gradient)" : rgbBuilderHex();
    }

    function rgbBuilderPreviewText() {
        return rgbBuilderUsesAllFeedbackPreview() ? "#ffffff" : idealText(rgbBuilderHex());
    }

    function renderRgbAllFeedbackGradientDefs() {
        const defs = [];
        if (rgbBuilderUsesAllFeedbackPreview()) {
            defs.push(renderRgbGradientDef("rgb-feedback-all-gradient", keyBehaviorRgbSemanticColorRows()));
        }
        for (const ledIndex of rgbBuilderDefinedLedIndices()) {
            const rows = rgbBuilderDefinedFeedbackGradientRows(ledIndex);
            if (rows.length > 1) {
                defs.push(renderRgbGradientDef("rgb-feedback-defined-gradient-" + ledIndex, rows));
            }
        }
        if (!defs.length) return "";
        return "<defs>" + defs.join("") + "</defs>";
    }

    function renderRgbGradientDef(id, rows) {
        if (!rows.length) return "";
        const step = 100 / rows.length;
        const stops = rows.map((row, index) => {
            const color = hsvToHex(row.color) || "#000000";
            const start = formatPercent(index * step);
            const end = formatPercent((index + 1) * step);
            return "<stop offset='" + start + "' stop-color='" + color + "'></stop>" +
                "<stop offset='" + end + "' stop-color='" + color + "'></stop>";
        }).join("");
        return "<linearGradient id='" + escapeAttr(id) + "' x1='0%' y1='0%' x2='100%' y2='0%'>" + stops + "</linearGradient>";
    }

    function formatPercent(value) {
        return Number(value.toFixed(4)) + "%";
    }

    function updateRgbSelectionPreview() {
        const builder = document.getElementById("rgbGroupBuilder");
        if (!builder) return;
        const fill = rgbBuilderPreviewFill();
        const text = rgbBuilderPreviewText();
        for (const key of builder.querySelectorAll(".svg-key.rgb-selected")) {
            key.querySelector("[data-rgb-led-preview]")?.setAttribute("fill", fill);
            for (const label of key.querySelectorAll("text, tspan")) {
                label.setAttribute("fill", text);
            }
        }
        for (const led of builder.querySelectorAll(".extra-led.rgb-selected")) {
            led.querySelector("[data-rgb-led-preview]")?.setAttribute("fill", fill);
            for (const label of led.querySelectorAll("text")) {
                label.setAttribute("fill", text);
            }
        }
    }

    function renderRgbGroupBoard(layer) {
        if (!layer) {
            return "<p class='muted'>No layer layout is available for LED selection.</p>";
        }
        const viewBox = keyboardGeometry.layoutViewBox;
        return "<div class='board layout-board-card'>" +
            "<div class='layout-board-header'>" +
            "<h3 class='layout-board-title'>LED group selector</h3>" +
            "<p class='layout-board-subtitle' data-tooltip='Click board LEDs to build an inline RGB_LED_GROUP(...) row or a reusable RGB_LED_GROUP_* definition. The active layer only provides labels and geometry.'>Physical LED indices - " + escapeHtml(layer.name) + "</p>" +
            "</div>" +
            "<div class='layout-board-stage'>" +
            "<svg class='keyboard-svg layout-board-svg' viewBox='" + viewBox.x + " " + viewBox.y + " " + viewBox.width + " " + viewBox.height + "' preserveAspectRatio='xMidYMid meet' role='img' aria-label='RGB LED group selector'>" +
            renderRgbAllFeedbackGradientDefs() +
            layer.positions.map(renderRgbSvgKey).join("") +
            renderExtraLed(56, 698, 522) +
            "</svg>" +
            "</div>" +
            "</div>";
    }

    function renderRgbSvgKey(position) {
        const ledIndex = layoutToLedIndex[position.layoutIndex];
        const visual = keyVisual(position.layoutIndex);
        const selected = effectiveRgbBuilderLedIndices().includes(ledIndex);
        const definedPreview = rgbBuilderDefinedPreviewForLed(ledIndex);
        const style = keyStyle(position);
        const cx = visual.x + keyboardGeometry.keyWidth / 2;
        const cy = visual.y + keyboardGeometry.keyHeight / 2;
        const allPreview = selected ? rgbBuilderUsesAllFeedbackPreview() : Boolean(definedPreview?.allFeedback);
        const selectedFill = selected ? rgbBuilderPreviewFill() : definedPreview?.fill || style.fill;
        const selectedText = selected ? rgbBuilderPreviewText() : definedPreview?.text || style.text;
        const state = selected ? "Selected for the pending row." : definedPreview ? "Already covered by an enabled row in this target table." : "Not selected for the pending row.";
        const alternate = alternateOutputTooltip(position.keycode);
        const tooltipText = "LED " + ledIndex + " for " + (position.display || position.keycode) + " (" + position.keycode + "). " + (alternate ? alternate + " " : "") + state + " Click to add or remove it from the inline selection.";
        const transform = visual.angle ? " transform='rotate(" + visual.angle + " " + cx + " " + cy + ")'" : "";
        return "<g class='svg-key " + (selected ? "rgb-selected" : "") + (definedPreview ? " rgb-defined" : "") + (allPreview ? " rgb-all-preview" : "") + "' data-action='toggleRgbLed' data-led='" + ledIndex + "' data-tooltip='" + escapeAttr(tooltipText) + "'" + transform + ">" +
            "<rect data-rgb-led-preview x='" + visual.x + "' y='" + visual.y + "' width='" + keyboardGeometry.keyWidth + "' height='" + keyboardGeometry.keyHeight + "' rx='" + keyboardGeometry.radius + "' fill='" + selectedFill + "' stroke='" + (selected ? "#ffffff" : style.stroke) + "'></rect>" +
            renderLayoutSvgLabel(position, position.display || position.keycode, cx, cy, selectedText) +
            renderRgbLedIndexLabel(ledIndex, visual, selectedText) +
            "</g>";
    }

    function renderRgbLedIndexLabel(ledIndex, visual, textColor) {
        return "<text x='" + (visual.x + keyboardGeometry.keyWidth - 7) + "' y='" + (visual.y + keyboardGeometry.keyHeight - 7) + "' fill='" + escapeAttr(textColor || "#e7ecef") + "' font-size='8.5' text-anchor='end' dominant-baseline='central' font-weight='700'>" + escapeHtml(ledIndex) + "</text>";
    }

    function renderExtraLed(ledIndex, cx, cy) {
        const selected = effectiveRgbBuilderLedIndices().includes(ledIndex);
        const definedPreview = rgbBuilderDefinedPreviewForLed(ledIndex);
        const allPreview = selected ? rgbBuilderUsesAllFeedbackPreview() : Boolean(definedPreview?.allFeedback);
        const fallbackFill = hsvToHex(trackballUnderlyingPreviewColor()) || "#20262a";
        const fill = selected ? rgbBuilderPreviewFill() : definedPreview?.fill || fallbackFill;
        const textColor = selected ? rgbBuilderPreviewText() : definedPreview?.text || idealText(fallbackFill);
        const state = selected ? "Selected for the pending row." : definedPreview ? "Already covered by an enabled row in this target table." : "Not selected for the pending row.";
        const tooltipText = "Trackball LED " + ledIndex + ". " + state + " Click to add or remove it from the inline selection.";
        return "<g class='extra-led " + (selected ? "rgb-selected" : "") + (definedPreview ? " rgb-defined" : "") + (allPreview ? " rgb-all-preview" : "") + "' data-action='toggleRgbTrackball' data-tooltip='" + escapeAttr(tooltipText) + "'>" +
            "<circle data-rgb-led-preview cx='" + cx + "' cy='" + cy + "' r='13' fill='" + fill + "' stroke='" + (selected ? "#ffffff" : "#31c6a4") + "'></circle>" +
            "<text x='" + cx + "' y='" + (cy + 1) + "' fill='" + textColor + "' font-size='10' text-anchor='middle' dominant-baseline='middle'>" + ledIndex + "</text>" +
            "</g>";
    }

    function renderLayerRgbSection(rgb) {
        return "<div class='card-list'>" +
            (rgb.layerColors || []).map(renderLayerColorCard).join("") +
            renderLedGroupSubsection("Layer LED Groups", rgb.layerLedGroups || [], "Layer", "layer") +
            "</div>";
    }

    function renderPdModeRgbSection(rgb) {
        return "<div class='card-list'>" +
            (rgb.pdModeColors || []).map(renderPdColorCard).join("") +
            renderLedGroupSubsection("Pointing-mode LED Groups", rgb.pdModeLedGroups || [], "Pointing mode", "pdMode") +
            "</div>";
    }

    function renderComboFeedbackSection(rgb) {
        return "<div class='card-list'>" +
            renderComboFeedbackCard(rgb.comboFeedback) +
            renderLedGroupSubsection("Combo Feedback LED Groups", rgb.comboFeedbackLedGroups || [], "", "combo") +
            "</div>";
    }

    function renderKeyBehaviorFeedbackSection(rgb) {
        return "<div class='card-list'>" +
            renderKeyBehaviorFeedbackCard(rgb.keyBehaviorFeedback) +
            renderLedGroupSubsection("Key Behavior Feedback LED Groups", rgb.keyBehaviorFeedbackLedGroups || [], "Semantic", "keyBehavior") +
            "</div>";
    }

    function renderLayerColorCard(row) {
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section data-layer='" + escapeAttr(row.layer) + "'>" +
            renderRgbConfigSummary(row.layer, row.color, row.mode, {
                ...layerRgbSummaryOptions(row),
                summaryTooltip: "Layer color row for " + row.layer + ". Click to edit its HSV color and render mode; LED group rows can override specific LEDs."
            }) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(row.color, "", "", { pickerColor: layerPreviewColor(row) || row.color }) +
            layerPassthroughNote(row) +
            "<div class='form-grid four'>" +
            "<label data-tooltip='Layer render mode. ALL_KEYS paints the whole layer; KEYS_MAPPED_ON_THIS_LAYER_ONLY paints only keys mapped on this layer and lets other LEDs pass through.'><span>mode</span><select name='mode' data-tooltip='Layer render mode. ALL_KEYS paints the whole layer; KEYS_MAPPED_ON_THIS_LAYER_ONLY paints only keys mapped on this layer and lets other LEDs pass through.'>" + options(["ALL_KEYS", "KEYS_MAPPED_ON_THIS_LAYER_ONLY"], row.mode) + "</select></label>" +
            "<button data-action='updateLayerColor' data-dirty-button class='primary'>Apply</button>" +
            "</div></div></details>";
    }

    function renderPdColorCard(row) {
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section data-mode='" + escapeAttr(row.pointingMode) + "'>" +
            renderRgbConfigSummary(row.pointingMode, row.color, row.locality, {
                summaryTooltip: "Pointing-mode feedback color for " + row.pointingMode + ". Click to edit HSV color and locality for this active mode."
            }) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(row.color) +
            "<div class='form-grid four'>" +
            "<label data-tooltip='Where this pointing-mode feedback paints while the mode is active.'><span>locality</span><select name='locality' data-tooltip='Where this pointing-mode feedback paints while the mode is active.'>" + options(rgbLocalities, row.locality) + "</select></label>" +
            "<button data-action='updatePdModeColor' data-dirty-button class='primary'>Apply</button>" +
            "</div></div></details>";
    }

    function renderAutomouseCard(config) {
        if (!config) {
            return "<p class='muted'>No active automouse fade config parsed.</p>";
        }
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section>" +
            renderRgbConfigSummary("Fade destination", config.end_color, config.mode, {
                summaryTooltip: "Auto-mouse fade destination. Click to edit the HSV color and fade mode used as auto-mouse approaches timeout."
            }) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(config.end_color) +
            "<div class='form-grid four'>" +
            "<label data-tooltip='Auto-mouse fade mode. It controls whether the timeout fade ends on the real destination color, where the base effect would show, or on all keys.'><span>mode</span><select name='mode' data-tooltip='Auto-mouse fade mode. It controls whether the timeout fade ends on the real destination color, where the base effect would show, or on all keys.'>" + options(automouseFadeModes, config.mode) + "</select></label>" +
            "<button data-action='updateAutomouseFade' data-dirty-button class='primary'>Apply</button>" +
            "</div>" +
            "</div></details>";
    }

    function renderComboFeedbackCard(config) {
        if (!config) {
            return "<p class='muted'>No active combo feedback config parsed.</p>";
        }
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section>" +
            renderRgbConfigSummary("Active combo color", config.color, config.locality, {
                summaryTooltip: "Combo feedback color shown while combo member keys are active. Click to edit HSV color and locality."
            }) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(config.color) +
            "<div class='form-grid four'>" +
            "<label data-tooltip='Where combo feedback paints while combo member keys are active.'><span>locality</span><select name='locality' data-tooltip='Where combo feedback paints while combo member keys are active.'>" + options(rgbLocalities, config.locality) + "</select></label>" +
            "<button data-action='updateComboFeedback' data-dirty-button class='primary'>Apply</button>" +
            "</div>" +
            "</div></details>";
    }

    function renderKeyBehaviorFeedbackCard(config) {
        if (!config) {
            return "<p class='muted'>No active key behavior feedback config parsed.</p>";
        }
        const colorRows = [
            ["Tap pending", "tapPendingColor", config.tapPendingColor],
            ["Tap committed", "tapCommittedColor", config.tapCommittedColor],
            ["Hold active", "holdActiveColor", config.holdActiveColor],
            ["Long hold active", "longHoldActiveColor", config.longHoldActiveColor],
        ];
        const branchRows = (config.tapBranchColors || []).map((color, index) => ["Tap count " + (index + 2), "tapBranchColor" + (index + 2), color]);
        return "<div id='keyBehaviorFeedbackCard' class='card-list' data-dirty-section>" +
            "<details class='card collapsible-card'>" +
            "<summary data-tooltip='Key behavior feedback policy. Click to edit which tap branches show confirmation, when tap-commit feedback appears, and where feedback paints.'><h3>Policy</h3></summary>" +
            "<div class='rgb-subsection-body'>" +
            "<div class='form-grid four'>" +
            "<label><span>branch confirm mode</span><select name='branchConfirmMode'>" + options(keyFeedbackBranchConfirmModes, config.branchConfirmMode) + "</select></label>" +
            "<label><span>tap commit mode</span><select name='tapCommitMode'>" + options(keyFeedbackTapCommitModes, config.tapCommitMode) + "</select></label>" +
            "<label data-tooltip='Where key-behavior feedback paints for tap, hold, long-hold, and tap-count states.'><span>locality</span><select name='locality' data-tooltip='Where key-behavior feedback paints for tap, hold, long-hold, and tap-count states.'>" + options(rgbLocalities, config.locality) + "</select></label>" +
            "</div>" +
            "</div></details>" +
            colorRows.map(([label, id, color]) =>
                renderRgbColorSubpanel(label, id, color)
            ).join("") +
            branchRows.map(([label, id, color]) =>
                renderRgbColorSubpanel(label, id, color, " data-tap-branch-color")
            ).join("") +
            "<button data-action='updateKeyBehaviorFeedback' data-dirty-button class='primary'>Apply key behavior feedback</button>" +
            "</div>";
    }

    function renderRgbColorSubpanel(label, id, color, extraAttrs = "") {
        return "<details class='card rgb-subsection'>" +
            renderRgbConfigSummary(label, color, "", { summaryTooltip: rgbColorSubpanelTooltip(label) }) +
            "<div class='rgb-subsection-body'>" + renderHsvColorControl(color, id, extraAttrs) + "</div>" +
            "</details>";
    }

    function rgbColorSubpanelTooltip(label) {
        const key = normalizeTooltipKey(label);
        if (key === "tap pending") return "Color shown while a key behavior is waiting to decide which tap-count branch will win. Click to edit HSV channels.";
        if (key === "tap committed") return "Color shown after a base tap commits when tap-commit feedback is enabled. Click to edit HSV channels.";
        if (key === "hold active") return "Color shown while a key behavior hold action is active. Click to edit HSV channels.";
        if (key === "long hold active") return "Color shown while a key behavior long-hold action is active. Click to edit HSV channels.";
        if (key.startsWith("tap count ")) return "Color shown during the branch-confirm window for this committed tap-count branch. Click to edit HSV channels.";
        return "RGB feedback color row for " + label + ". Click to edit HSV channels.";
    }

    function layerRgbSummaryOptions(row) {
        if (!layerColorIsPassthrough(row.color)) return {};
        const defaultColor = model.rgb?.defaultColor;
        const previewColor = row.layer === "LAYER_BASE" && defaultColor ? defaultColor : undefined;
        const behavior = row.layer === "LAYER_BASE"
            ? "HSV(0, 0, 0) leaves the base layer unpainted, so the default RGB Matrix effect shows."
            : "HSV(0, 0, 0) leaves this layer unpainted, so lower active layers or the default RGB Matrix effect show.";
        return {
            previewColor: previewColor || row.color,
            extraMeta: row.layer === "LAYER_BASE" ? "default RGB" : "pass-through",
            passThroughSwatch: !previewColor,
            swatchTooltip: behavior + (previewColor ? " Preview uses " + colorExpression(previewColor) + "." : "")
        };
    }

    function layerPassthroughNote(row) {
        if (!layerColorIsPassthrough(row.color)) return "";
        if (row.layer === "LAYER_BASE" && model.rgb?.defaultColor) {
            return "<p class='muted'>HSV(0, 0, 0) leaves the base layer unpainted; the preview swatch shows the default RGB Matrix color " + escapeHtml(colorExpression(model.rgb.defaultColor)) + ".</p>";
        }
        return "<p class='muted'>HSV(0, 0, 0) leaves this layer unpainted, so lower active layers or the default RGB Matrix effect show through.</p>";
    }

    function renderRgbConfigSummary(label, color, meta = "", options = {}) {
        const expression = options.expression || colorExpression(color);
        const previewColor = options.previewColor || color;
        const summaryTooltip = options.summaryTooltip || ("RGB row for " + label + ". Click to expand HSV color controls." + (meta ? " Current policy: " + meta + "." : ""));
        return "<summary data-tooltip='" + escapeAttr(summaryTooltip) + "'><span class='rgb-summary'>" +
            "<span class='rgb-summary-title'>" + escapeHtml(label) + "</span>" +
            renderSummarySwatch(previewColor, options.swatchTooltip || ("Collapsed color preview: " + expression), options) +
            "<span class='rgb-summary-meta'>" +
            "<code class='muted' data-summary-expression>" + escapeHtml(expression) + "</code>" +
            (meta ? "<code class='muted'>" + escapeHtml(meta) + "</code>" : "") +
            (options.extraMeta ? "<code class='muted'>" + escapeHtml(options.extraMeta) + "</code>" : "") +
            "</span>" +
            "</span></summary>";
    }

    function renderSummarySwatch(color, tooltip, options = {}) {
        const fill = hsvToHex(color) || "#000000";
        const label = tooltip || "Collapsed color preview: " + colorExpression(color);
        if (options.passThroughSwatch) {
            return "<svg class='rgb-summary-swatch' viewBox='0 0 80 26' role='img' aria-label='" + escapeAttr(label) + "' data-tooltip='" + escapeAttr(label) + "'>" +
                "<rect x='1' y='1' width='78' height='24' rx='5' fill='#323d43' stroke='#ffffff' stroke-opacity='0.38' stroke-width='1'></rect>" +
                "<path d='M-8 26 L26 -8 M10 34 L52 -8 M36 34 L78 -8 M60 34 L88 6' stroke='#b9c4c9' stroke-opacity='0.58' stroke-width='3'></path>" +
                "</svg>";
        }
        return "<svg class='rgb-summary-swatch' viewBox='0 0 80 26' role='img' aria-label='" + escapeAttr(label) + "' data-tooltip='" + escapeAttr(label) + "'>" +
            "<rect data-summary-swatch x='1' y='1' width='78' height='24' rx='5' fill='" + fill + "' stroke='#ffffff' stroke-opacity='0.38' stroke-width='1'></rect>" +
            "</svg>";
    }

    function renderLedGroupSubsection(title, rows, ownerLabel, tableKind = "") {
        return "<details class='card collapsible-card'>" +
            "<summary><h3>" + escapeHtml(title) + "</h3></summary>" +
            "<div class='rgb-subsection-body'>" + renderLedGroupTable(rows, ownerLabel, tableKind) + "</div>" +
            "</details>";
    }

    function renderLedGroupTable(rows, ownerLabel, tableKind = "") {
        if (!rows.length) {
            return "<p class='muted'>No active LED group rows are enabled in this table.</p>";
        }
        const ownerHeader = ownerLabel ? renderTooltipHeader(ownerLabel, ledGroupOwnerHeaderTooltip(ownerLabel, tableKind)) : "";
        return "<table><thead><tr>" + ownerHeader +
            renderTooltipHeader("Color", "Color used by this LED group row. HSV(0, 0, 0) inherits the owning stage color.") +
            renderTooltipHeader("LED group", "Reusable or inline RGB_LED_GROUP expression authored in rgb_config.c.") +
            renderTooltipHeader("LEDs", "Physical LED indices contained by this row after resolving reusable groups.") +
            "</tr></thead><tbody>" +
            rows.map((row) => "<tr>" +
                (ownerLabel ? "<td>" + renderLedGroupOwnerCell(row, tableKind) + "</td>" : "") +
                renderLedGroupColorCell(row, tableKind) +
                "<td>" + renderLedGroupExpressionCell(row) + "</td>" +
                "<td data-tooltip='" + escapeAttr("Resolved physical LED indices for this row: " + ((row.ledIndices || []).join(", ") || "none")) + "'><code>" + escapeHtml((row.ledIndices || []).join(", ")) + "</code></td>" +
                "</tr>").join("") +
            "</tbody></table>";
    }

    function ledGroupOwnerHeaderTooltip(ownerLabel, tableKind = "") {
        if (tableKind === "layer") return "Layer owner for this LED group row. All layers means the row applies to every layer.";
        if (tableKind === "pdMode") return "Pointing-mode owner for this LED group row. All pointing modes means the row applies to every pointing mode.";
        if (tableKind === "keyBehavior") return "Key-behavior feedback semantic that owns this LED group row.";
        return ownerLabel + " owner for this LED group row.";
    }

    function renderLedGroupExpressionCell(row) {
        const kind = row.ledGroupKind === "reusable" ? "reusable" : "inline";
        const tooltip = kind === "reusable"
            ? "Reusable RGB_LED_GROUP_* reference. Editing the reusable definition changes every row that uses it."
            : "Inline RGB_LED_GROUP(...) expression stored directly on this table row.";
        return "<code data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(row.ledGroup || "") + "</code><div class='muted'>" + kind + "</div>";
    }

    function renderLedGroupOwnerCell(row, tableKind = "") {
        const owner = row.owner || "";
        const label = rgbLedGroupOwnerLabel(owner, tableKind);
        const tooltip = "Owner for this row: " + (label || owner || "shared combo feedback") + ".";
        if (label === owner) {
            return "<code data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(owner) + "</code>";
        }
        return "<code data-tooltip='" + escapeAttr(tooltip) + "'>" + escapeHtml(owner) + "</code><div class='muted'>" + escapeHtml(label) + "</div>";
    }

    function rgbLedGroupOwnerLabel(owner, tableKind = "") {
        if (tableKind === "layer" && owner === rgbLayerAllGroups) return "All layers";
        if (tableKind === "pdMode" && owner === rgbPdModeAllGroups) return "All pointing modes";
        if (tableKind === "keyBehavior" && owner === keyBehaviorAllGroups) return "All feedback groups";
        if (tableKind === "keyBehavior") return keyBehaviorRgbSemanticLabel(owner);
        return owner || "";
    }

    function renderLedGroupColorCell(row, tableKind = "") {
        if (rgbGroupColorInherits(row.color)) {
            if (tableKind === "keyBehavior" && row.owner === keyBehaviorAllGroups) {
                return "<td data-tooltip='HSV(0, 0, 0) for all feedback groups inherits whichever key-behavior feedback semantic is active at render time.'><div class='toolbar'>" + keyBehaviorRgbSemanticColorRows().map((semanticRow) => renderInlineSwatch(semanticRow.color)).join("") + "</div><code class='muted'>inherits active feedback color</code></td>";
            }
            const inherited = rgbLedGroupInheritedColor(row, tableKind);
            if (inherited) {
                return "<td data-tooltip='" + escapeAttr("HSV(0, 0, 0) inherits the owning stage color: " + rgbLedGroupInheritedLabel(row, tableKind) + ".") + "'>" + renderInlineSwatch(inherited) + "<code class='muted'>" + escapeHtml(rgbLedGroupInheritedLabel(row, tableKind)) + "</code></td>";
            }
            return "<td data-tooltip='" + escapeAttr("HSV(0, 0, 0) inherits the owning stage color: " + rgbLedGroupInheritedLabel(row, tableKind) + ".") + "'><code class='muted'>" + escapeHtml(rgbLedGroupInheritedLabel(row, tableKind)) + "</code></td>";
        }
        return "<td data-tooltip='" + escapeAttr("Explicit HSV color for this LED group row: " + (row.color?.expression || colorExpression(row.color))) + "'>" + renderInlineSwatch(row.color) + "<code>" + escapeHtml(row.color?.expression || "") + "</code></td>";
    }

    function rgbGroupColorInherits(color) {
        return String(color?.h || "") === "0" && String(color?.s || "") === "0" && String(color?.v || "") === "0";
    }

    function baseLayerVisiblePreviewColor() {
        const baseLayer = colorForLayer("LAYER_BASE");
        return layerPreviewColor(baseLayer) || model.rgb?.defaultColor || baseLayer?.color;
    }

    function layerLedGroupInheritedPreviewColor(layerName) {
        const layerColor = colorForLayer(layerName);
        return layerPreviewColor(layerColor) || baseLayerVisiblePreviewColor() || layerColor?.color;
    }

    function trackballUnderlyingPreviewColor() {
        const activeLayerColor = colorForLayer(activeLayer);
        if (activeLayerColor?.mode === "ALL_KEYS" && !layerColorIsPassthrough(activeLayerColor.color)) {
            return activeLayerColor.color;
        }
        return baseLayerVisiblePreviewColor();
    }

    function rgbLedGroupInheritedColor(row, tableKind = "") {
        if (tableKind === "layer" && row.owner === rgbLayerAllGroups) {
            return layerLedGroupInheritedPreviewColor(activeLayer);
        }
        if (tableKind === "layer" && row.owner !== rgbLayerAllGroups) {
            return layerLedGroupInheritedPreviewColor(row.owner);
        }
        if (tableKind === "pdMode" && row.owner === rgbPdModeAllGroups) {
            return (model.rgb?.pdModeColors || [])[0]?.color;
        }
        if (tableKind === "pdMode" && row.owner !== rgbPdModeAllGroups) {
            return colorForPdMode(row.owner)?.color;
        }
        if (tableKind === "combo") {
            return model.rgb?.comboFeedback?.color;
        }
        if (tableKind === "keyBehavior" && row.owner !== keyBehaviorAllGroups) {
            return keyBehaviorSemanticColor(row.owner);
        }
        return undefined;
    }

    function rgbLedGroupInheritedLabel(row, tableKind = "") {
        if (tableKind === "layer") return row.owner === rgbLayerAllGroups ? "inherits each active layer color" : "inherits layer color";
        if (tableKind === "pdMode") return row.owner === rgbPdModeAllGroups ? "inherits each active pointing-mode color" : "inherits pointing-mode color";
        if (tableKind === "combo") return "inherits combo feedback color";
        if (tableKind === "keyBehavior") return "inherits active feedback color";
        return "inherits stage color";
    }

    function renderHsvColorControl(color, id, extraAttrs = "", options = {}) {
        const hex = hsvToHex(options.pickerColor || color) || "#000000";
        const idAttr = id ? " data-color-id='" + escapeAttr(id) + "'" : "";
        const expression = colorExpression(color);
        return "<div class='color-control' data-color-control" + idAttr + extraAttrs + ">" +
            "<div class='color-row'>" +
            "<label><span>picker</span><input type='color' data-color-picker value='" + hex + "'></label>" +
            hsvInputs(color) +
            "</div>" +
            "<code class='muted' data-color-expression data-tooltip='" + escapeAttr("Authored HSV expression that will be written back to rgb_config.c: " + expression) + "'>" + escapeHtml(expression) + "</code>" +
            "</div>";
    }

    function hsvInputs(color) {
        return "<label><span>h</span><input name='h' data-hsv-channel='h' data-validate='uint8' inputmode='numeric' value='" + escapeAttr(color?.h || "") + "'></label>" +
            "<label><span>s</span><input name='s' data-hsv-channel='s' data-validate='uint8' inputmode='numeric' value='" + escapeAttr(color?.s || "") + "'></label>" +
            "<label><span>v</span><input name='v' data-hsv-channel='v' data-validate='hsv-value' value='" + escapeAttr(color?.v || "") + "'></label>";
    }

    function renderInlineSwatch(color, extraAttrs = "") {
        const fill = hsvToHex(color) || "#000000";
        const label = "Color preview: " + colorExpression(color);
        return "<svg class='inline-swatch' viewBox='0 0 34 16' role='img' aria-label='" + escapeAttr(label) + "' data-tooltip='" + escapeAttr(label) + "'" + extraAttrs + ">" +
            "<rect x='1' y='1' width='32' height='14' rx='4' fill='" + fill + "' stroke='#ffffff' stroke-opacity='0.32'></rect>" +
            "</svg>";
    }

    function colorExpression(color) {
        return color?.expression || "HSV(" + [color?.h || "", color?.s || "", color?.v || ""].join(", ") + ")";
    }

    function updateColorControl(control) {
        const color = {
            h: value(control, "h"),
            s: value(control, "s"),
            v: value(control, "v")
        };
        const preview = previewOptionsForColorControl(control, color);
        const previewColor = preview.previewColor || color;
        const hex = hsvToHex(previewColor) || "#000000";
        const picker = control.querySelector("[data-color-picker]");
        const expression = control.querySelector("[data-color-expression]");
        if (picker && /^#[0-9a-f]{6}$/i.test(hex)) {
            picker.value = hex;
        }
        if (expression) {
            const nextExpression = colorExpression(color);
            expression.textContent = nextExpression;
            expression.setAttribute("data-tooltip", "Authored HSV expression that will be written back to rgb_config.c: " + nextExpression);
        }
        const subsection = control.closest(".rgb-subsection");
        if (subsection) {
            const summarySwatch = subsection.querySelector("[data-summary-swatch]");
            const summaryExpression = subsection.querySelector("[data-summary-expression]");
            if (summarySwatch) {
                summarySwatch.setAttribute("fill", hex);
                const summaryPreview = summarySwatch.closest("svg");
                if (summaryPreview) {
                    const label = preview.swatchTooltip || "Collapsed color preview: " + colorExpression(color);
                    summaryPreview.setAttribute("data-tooltip", label);
                    summaryPreview.setAttribute("aria-label", label);
                }
            }
            if (summaryExpression) {
                summaryExpression.textContent = colorExpression(color);
            }
        }
    }

    function previewOptionsForColorControl(control, color) {
        const layerCard = control.closest(".rgb-subsection[data-layer]");
        if (!layerCard) return { previewColor: color };
        return layerRgbSummaryOptions({
            layer: layerCard.dataset.layer || "",
            color
        });
    }

    function hsvToHex(color) {
        if (!color) return "";
        const h = Number(color.h);
        const s = Number(color.s);
        const v = numericChannel(color.v);
        if (!Number.isFinite(h) || !Number.isFinite(s) || !Number.isFinite(v) || v < 0) return "";
        const rgb = hsvToRgb(h / 255, s / 255, v / 255);
        return rgbToHex(rgb.r, rgb.g, rgb.b);
    }

    function hexToHsv(hex) {
        const match = /^#?([0-9a-f]{6})$/i.exec(hex || "");
        if (!match) return undefined;
        const value = match[1];
        const r = parseInt(value.slice(0, 2), 16) / 255;
        const g = parseInt(value.slice(2, 4), 16) / 255;
        const b = parseInt(value.slice(4, 6), 16) / 255;
        const max = Math.max(r, g, b);
        const min = Math.min(r, g, b);
        const delta = max - min;
        let h = 0;
        if (delta !== 0) {
            if (max === r) h = ((g - b) / delta) % 6;
            else if (max === g) h = (b - r) / delta + 2;
            else h = (r - g) / delta + 4;
            h /= 6;
            if (h < 0) h += 1;
        }
        const s = max === 0 ? 0 : delta / max;
        return {
            h: Math.round(h * 255),
            s: Math.round(s * 255),
            v: Math.round(max * 255)
        };
    }

    function numericChannel(value) {
        if (value === "RGB_MATRIX_MAXIMUM_BRIGHTNESS" || value === "RGB_MATRIX_DEFAULT_VAL") return 200;
        const number = Number(value);
        return Number.isFinite(number) ? number : NaN;
    }

    function hsvToRgb(h, s, v) {
        const sector = Math.floor(h * 6);
        const f = h * 6 - sector;
        const p = v * (1 - s);
        const q = v * (1 - f * s);
        const t = v * (1 - (1 - f) * s);
        let r, g, b;
        switch (sector % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
    }

    function rgbToHex(r, g, b) {
        return "#" + [r, g, b].map((value) => Math.max(0, Math.min(255, value)).toString(16).padStart(2, "0")).join("");
    }

    function shadeColor(hex, amount) {
        const match = /^#?([0-9a-f]{6})$/i.exec(hex || "");
        if (!match) return "#64717a";
        const value = match[1];
        const parts = [0, 2, 4].map((offset) => Math.max(0, Math.min(255, parseInt(value.slice(offset, offset + 2), 16) + amount)));
        return rgbToHex(parts[0], parts[1], parts[2]);
    }

    function idealText(hex) {
        const match = /^#?([0-9a-f]{6})$/i.exec(hex || "");
        if (!match) return "#f7f7f4";
        const value = match[1];
        const r = parseInt(value.slice(0, 2), 16);
        const g = parseInt(value.slice(2, 4), 16);
        const b = parseInt(value.slice(4, 6), 16);
        return (r * 0.299 + g * 0.587 + b * 0.114) > 150 ? "#18201d" : "#f7f7f4";
    }

    function options(values, selected) {
        return values.map((value) => "<option value='" + escapeAttr(value) + "' " + (value === selected ? "selected" : "") + ">" + escapeHtml(value) + "</option>").join("");
    }

    function helperOptions(values, selected) {
        const helpers = values.filter(Boolean);
        const selectedHelper = selected || helpers[0] || "";
        return helpers.map((value) => "<option value='" + escapeAttr(value) + "' " + (value === selectedHelper ? "selected" : "") + ">" + escapeHtml(helperLabel(value)) + "</option>").join("");
    }

    function helperLabel(value) {
        return {
            TAP_SENDS: "Tap sends",
            PRESS_AND_HOLD_UNTIL_RELEASE: "Press and hold until release",
            TAP_AT_HOLD_THRESHOLD: "Tap at hold threshold",
            TAP_ON_RELEASE_AFTER_HOLD: "Tap on release after hold",
            REPEAT_WHILE_HELD: "Repeat while held"
        }[value] || titleCase(value);
    }

    function optionsWithLabels(values, selected) {
        return values.map(([value, label]) => "<option value='" + escapeAttr(value) + "' " + (value === selected ? "selected" : "") + ">" + escapeHtml(label) + "</option>").join("");
    }

    function renderMacroStudio() {
        normalizeMacroBuilderState();
        const slots = model.viaMacros || [];
        if (!slots.length) {
            return staticPanel("Macro Builder", "<p class='muted'>No VIA_MACROS(MACRO) rows were parsed.</p>");
        }
        const active = activeMacroSlot() || slots[0];
        return staticPanel("Macro Builder",
            renderMacroUsageSummary(slots) +
            "<div class='macro-builder-grid'>" +
            renderMacroSlotBrowser(slots) +
            renderMacroWorkbench(active) +
            "</div>"
        );
    }

    function renderMacroUsageSummary(slots) {
        const payloads = slots.map((slot) => macroPayloadForSlot(slot));
        const filled = payloads.filter((payload) => payload.length > 0).length;
        const edited = slots.filter((slot) => macroSlotDirty(slot.keycode)).length;
        const totalChars = payloads.reduce((sum, payload) => sum + payload.length, 0);
        return "<div class='macro-stat-row macro-builder-summary'>" +
            renderMacroChip("slots", filled + " / " + slots.length + " filled", "", "Filled VIA macro slots out of all parsed VIA_MACROS(MACRO) rows.") +
            renderMacroChip("payload chars", String(totalChars), "", "Total source characters across all current macro payload drafts.") +
            (edited ? renderMacroChip("edited", String(edited), "warning", "Slots with local payload drafts that differ from keymap.c and still need Apply macro.") : "") +
            renderMacroChip("target", "VIA_MACROS(MACRO)", "", "Macro payloads are written to the VIA_MACROS(MACRO) table in keymap.c.") +
            "</div>";
    }

    function renderMacroSlotBrowser(slots) {
        return "<div class='card macro-slot-browser'>" +
            "<h3 data-tooltip='Parsed VIA macro slots from the VIA_MACROS(MACRO) table. Select a slot to edit its payload draft.'>VIA Macro Slots</h3>" +
            "<div class='macro-slot-list' role='listbox' aria-label='VIA macro slots'>" +
            slots.map(renderMacroSlotButton).join("") +
            "</div>" +
            "</div>";
    }

    function renderMacroSlotButton(slot) {
        const payload = macroPayloadForSlot(slot);
        const active = slot.keycode === activeMacroKeycode;
        const dirty = macroSlotDirty(slot.keycode);
        const empty = payload.length === 0;
        const label = "VIA " + macroSlotNumber(slot.keycode);
        const state = dirty ? "edited" : empty ? "empty" : payload.length + " chars";
        const classes = ["macro-slot-button", active ? "active" : "", dirty ? "dirty" : "", empty ? "empty" : ""].filter(Boolean).join(" ");
        const tooltip = macroSlotTooltip(label, state, payload);
        return "<button type='button' role='option' aria-selected='" + (active ? "true" : "false") + "' class='" + classes + "' data-action='selectMacroSlot' data-keycode='" + escapeAttr(slot.keycode) + "' data-tooltip='" + escapeAttr(tooltip) + "'>" +
            "<span class='macro-slot-title'>" + escapeHtml(label) + "</span>" +
            "<span class='macro-slot-state'>" + escapeHtml(state) + "</span>" +
            "</button>";
    }

    function macroSlotTooltip(label, state, payload) {
        const text = String(payload || "");
        const preview = text ? text : "empty";
        return label + " - " + state + "\\nSelect to edit this VIA macro slot. Payload draft: " + truncateTooltipText(preview, 420);
    }

    function truncateTooltipText(value, limit) {
        const text = String(value || "");
        if (text.length <= limit) return text;
        return text.slice(0, Math.max(0, limit - 3)) + "...";
    }

    function renderMacroWorkbench(slot) {
        const payload = macroPayloadForSlot(slot);
        return "<div class='macro-builder-main' data-macro-workbench data-keycode='" + escapeAttr(slot.keycode) + "'>" +
            renderMacroEditor(slot, payload) +
            "<div class='macro-tool-row'>" +
            renderMacroRecorder(slot) +
            renderMacroComposer() +
            "</div>" +
            renderMacroPreview(payload) +
            "</div>";
    }

    function renderMacroEditor(slot, payload) {
        const parsed = parseMacroPayloadPreview(payload);
        const status = parsed.error ? "invalid" : payload ? "ready" : "empty";
        return "<div class='card macro-editor-card' data-dirty-section data-macro-editor data-keycode='" + escapeAttr(slot.keycode) + "'>" +
            "<div class='macro-builder-head'>" +
            "<div><h3>" + escapeHtml(displayKeyExpression(slot.keycode)) + "</h3>" +
            "<div class='muted' data-tooltip='" + escapeAttr(macroEditorStatusTooltip(slot, status, parsed.error)) + "'><code>" + escapeHtml(slot.keycode) + "</code> - " + escapeHtml(status) + "</div></div>" +
            "<button data-action='updateViaMacro' data-dirty-button class='primary'>Apply macro</button>" +
            "</div>" +
            "<label><span>Payload</span><textarea class='macro-payload-textarea monospace' data-macro-payload data-validate='macro-payload' spellcheck='false'>" + escapeHtml(payload) + "</textarea></label>" +
            "</div>";
    }

    function macroEditorStatusTooltip(slot, status, error) {
        const keycode = displayKeyExpression(slot?.keycode || "");
        if (status === "invalid") return keycode + " payload has a parse error: " + error;
        if (status === "ready") return keycode + " has a non-empty payload draft. Apply macro writes it back to keymap.c.";
        return keycode + " is empty. Add text, insert steps, or record events before applying.";
    }

    function renderMacroComposer() {
        const typeOptions = [
            ["tap", "Tap key or chord"],
            ["text", "Type text"],
            ["down", "Key down"],
            ["up", "Key up"],
            ["delay", "Delay"]
        ];
        return "<div class='card macro-composer-card' data-macro-composer>" +
            "<div class='macro-builder-head macro-composer-header'>" +
            "<div><h3 data-tooltip='Build one payload fragment and insert it into the selected macro draft at the cursor.'>Step Builder</h3>" +
            "<div class='muted macro-sidecar-state-spacer' aria-hidden='true'>idle</div></div>" +
            "</div>" +
            "<div class='macro-composer-grid'>" +
            "<label><span>Step type</span><select id='macroStepType' name='macroStepType'>" + optionsWithLabels(typeOptions, "tap") + "</select></label>" +
            "<div class='macro-step-fields'>" +
            "<label data-macro-step-field='text' hidden><span>Text</span><input id='macroStepText' placeholder='hello'></label>" +
            renderKeyPickerInput("macroStepKeys", "Macro keys", "", "A, Cmd+Space, KC_LGUI, KC_SPC", "list", "", "data-macro-step-field='tap'", "data-validate='macro-key-list'") +
            renderKeyPickerInput("macroStepKey", "Macro key", "", "Shift", "single", "", "data-macro-step-field='down up' hidden", "data-validate='macro-key-single'") +
            "<label data-macro-step-field='delay' hidden><span>Delay ms</span><input id='macroStepDelay' data-validate='positive-int' inputmode='numeric' value='250'></label>" +
            "</div>" +
            "<div class='macro-composer-actions'>" +
            "<button type='button' data-action='insertMacroStep' class='primary' disabled>Insert step</button>" +
            "<button type='button' data-action='clearMacroPayload'>Clear</button>" +
            "</div>" +
            "</div>" +
            "</div>";
    }

    function renderMacroRecorder(slot) {
        return "<div class='card macro-recorder-card' data-macro-recorder>" + renderMacroRecorderBody(slot) + "</div>";
    }

    function renderMacroRecorderBody(slot) {
        const recordedPayload = recordedMacroPayload();
        const eventCount = macroRecordedEvents.filter((event) => event.kind === "key").length;
        const stateText = macroRecording ? "recording" + (macroRecorderNotice ? " - " + macroRecorderNotice : "") : (macroRecorderNotice || "idle");
        const state = macroRecording
            ? "<span class='macro-recorder-dot' aria-hidden='true'></span> recording" + (macroRecorderNotice ? " - " + escapeHtml(macroRecorderNotice) : "")
            : escapeHtml(macroRecorderNotice || "idle");
        const modeOptions = [
            ["compact", "Compact taps/chords"],
            ["exact", "Exact down/up"]
        ];
        const delayFieldClass = "macro-recorder-delay-fields" + (macroRecordDelays ? "" : " inactive");
        const delayFieldAttrs = macroRecordDelays ? " data-validate='positive-int'" : " tabindex='-1'";
        return "<div class='macro-builder-head macro-recorder-header'>" +
            "<div><h3 data-tooltip='Record keyboard events in the browser and append the generated payload to the selected VIA macro draft.'>Record Macro</h3>" +
            "<div class='muted macro-recorder-state' data-tooltip='" + escapeAttr("Recorder state for " + displayKeyExpression(slot.keycode) + ": " + stateText) + "'>" + state + "</div></div>" +
            "<div class='macro-stat-row macro-recorder-stats'>" +
            renderMacroChip("events", String(eventCount), "", "Captured key down/up events in the current recording take.") +
            renderMacroChip("generated chars", String(recordedPayload.length), "", "Characters generated from the current recording take, excluding the payload that existed before recording started.") +
            (macroRecordDelays ? renderMacroChip("delays", "on", "", "Delay recording is enabled; qualifying elapsed gaps become {number} commands.") : renderMacroChip("delays", "off", "", "Delay recording is disabled; only key events are captured.")) +
            "</div>" +
            "</div>" +
            "<div class='macro-recorder-controls'>" +
            "<div class='macro-recorder-primary-row'>" +
            "<label><span>Record mode</span><select id='macroRecorderMode' name='macroRecorderMode'>" + optionsWithLabels(modeOptions, macroRecorderMode) + "</select></label>" +
            "<label class='macro-recorder-toggle'><span class='macro-recorder-toggle-label'>Record delays</span><input id='macroRecordDelays' name='macroRecordDelays' type='checkbox' " + (macroRecordDelays ? "checked" : "") + "><span class='macro-recorder-toggle-track' aria-hidden='true'></span></label>" +
            "</div>" +
            "<div class='" + delayFieldClass + "' aria-disabled='" + (macroRecordDelays ? "false" : "true") + "'>" +
            "<label><span>Delay threshold ms</span><input id='macroRecorderDelayThreshold' name='macroRecorderDelayThreshold'" + delayFieldAttrs + " inputmode='numeric' value='" + escapeAttr(macroRecorderDelayThreshold) + "'></label>" +
            "<label><span>Delay round ms</span><input id='macroRecorderDelayRound' name='macroRecorderDelayRound'" + delayFieldAttrs + " inputmode='numeric' value='" + escapeAttr(macroRecorderDelayRound) + "'></label>" +
            "</div>" +
            "</div>" +
            "<div class='macro-recorder-actions'>" +
            "<button type='button' data-action='clearMacroRecording'>Clear take</button>" +
            (macroRecording
                ? "<button type='button' data-action='stopMacroRecording' class='primary wide record-action'>Stop</button>"
                : "<button type='button' data-action='startMacroRecording' class='primary wide record-action'>Record</button>") +
            "</div>";
    }

    function renderMacroPreview(payload) {
        return "<div class='card macro-preview-card' data-macro-preview>" + renderMacroPreviewBody(payload) + "</div>";
    }

    function renderMacroPreviewBody(payload) {
        const parsed = parseMacroPayloadPreview(payload || "");
        const stats = macroPayloadStats(parsed, payload || "");
        return "<h3 data-tooltip='Parsed preview of the selected macro payload draft. It shows how text, taps, chords, holds, releases, and delays will be interpreted.'>Payload Preview</h3>" +
            "<div class='macro-stat-row'>" +
            renderMacroChip("source chars", String((payload || "").length), "", "Characters in the raw payload source string.") +
            renderMacroChip("encoded bytes", parsed.error ? "unknown" : String(stats.bytes), "", "Approximate encoded macro payload bytes after parsing commands.") +
            renderMacroChip("steps", String(parsed.steps.length), "", "Parsed text, tap/chord, down/up, and delay steps in this payload.") +
            (parsed.error ? renderMacroChip("invalid", parsed.error, "warning", "Payload parse error. Apply macro is disabled until this is fixed.") : "") +
            "</div>" +
            (parsed.steps.length ? "<div class='macro-preview-list'>" + parsed.steps.map(renderMacroPreviewStep).join("") + "</div>" : "<p class='muted' data-tooltip='This slot currently has an empty payload draft.'>This macro is empty.</p>");
    }

    function renderMacroPreviewStep(step) {
        const tooltip = macroPreviewStepTooltip(step);
        return "<div class='macro-preview-step' data-tooltip='" + escapeAttr(tooltip) + "'>" +
            "<div class='macro-preview-kind'>" + escapeHtml(step.kindLabel) + "</div>" +
            "<div class='macro-preview-detail'>" + escapeHtml(step.detail) + (step.raw ? "<br><code class='muted'>" + escapeHtml(step.raw) + "</code>" : "") + "</div>" +
            "</div>";
    }

    function macroPreviewStepTooltip(step) {
        if (!step) return "Parsed macro payload step.";
        if (step.kind === "text") return "Plain text step. The macro types these ASCII characters directly.";
        if (step.kind === "tap") return "Tap step. The macro presses and releases this key.";
        if (step.kind === "chord") return "Chord step. The macro taps these keys together.";
        if (step.kind === "down") return "Key-down step. The key remains held until a matching key-up step releases it.";
        if (step.kind === "up") return "Key-up step. Releases a key previously held by a key-down step.";
        if (step.kind === "delay") return "Delay step. The macro waits this many milliseconds before continuing.";
        return "Parsed macro payload step.";
    }

    function renderMacroChip(label, value, extraClass = "", tooltip = "") {
        const text = tooltip || (label + ": " + value);
        return "<span class='macro-chip " + escapeAttr(extraClass) + "' data-tooltip='" + escapeAttr(text) + "'><strong>" + escapeHtml(label) + "</strong> " + escapeHtml(value) + "</span>";
    }

    function activeMacroSlot() {
        return (model.viaMacros || []).find((slot) => slot.keycode === activeMacroKeycode);
    }

    function macroSlotByKeycode(keycode) {
        return (model.viaMacros || []).find((slot) => slot.keycode === keycode);
    }

    function macroPayloadForSlot(slot) {
        if (!slot) return "";
        if (Object.prototype.hasOwnProperty.call(macroDrafts, slot.keycode)) {
            return String(macroDrafts[slot.keycode] || "");
        }
        return String(slot.payload || "");
    }

    function macroSlotDirty(keycode) {
        const slot = macroSlotByKeycode(keycode);
        return Boolean(slot && Object.prototype.hasOwnProperty.call(macroDrafts, keycode) && String(macroDrafts[keycode] || "") !== String(slot.payload || ""));
    }

    function macroSlotNumber(keycode) {
        return String(keycode || "").match(/_(\\d+)$/)?.[1] || "?";
    }

    function setMacroDraft(keycode, payload) {
        const slot = macroSlotByKeycode(keycode);
        if (!slot) return;
        const nextPayload = String(payload || "");
        if (nextPayload === String(slot.payload || "")) {
            const nextDrafts = { ...macroDrafts };
            delete nextDrafts[keycode];
            macroDrafts = nextDrafts;
            return;
        }
        macroDrafts = { ...macroDrafts, [keycode]: nextPayload };
    }

    function captureActiveMacroDraft() {
        captureMacroDraftFromControl(document.querySelector("[data-macro-payload]"));
    }

    function captureMacroDraftFromControl(control) {
        if (!control) return;
        const editor = control.closest("[data-macro-editor]");
        const keycode = editor?.dataset.keycode || activeMacroKeycode;
        if (!keycode) return;
        setMacroDraft(keycode, control.value || "");
    }

    function parseMacroPayloadPreview(payload) {
        const steps = [];
        let textStart = 0;
        let index = 0;

        const flushText = (end) => {
            if (end <= textStart) return;
            const text = payload.slice(textStart, end);
            steps.push({
                kind: "text",
                kindLabel: "Text",
                detail: text,
                raw: text.length > 48 ? text.slice(0, 48) + "..." : text
            });
        };

        while (index < payload.length) {
            const char = payload[index];
            if (char.charCodeAt(0) > 0x7F) {
                return { steps, error: "Macro payloads only support ASCII text." };
            }
            if (char === "}") {
                return { steps, error: "Unexpected } outside a macro command." };
            }
            if (char !== "{") {
                index += 1;
                continue;
            }

            flushText(index);
            const close = payload.indexOf("}", index + 1);
            if (close === -1) {
                return { steps, error: "Missing } for macro command." };
            }

            const commandText = payload.slice(index + 1, close);
            const command = parseMacroCommandPreview(commandText);
            if (command.error) {
                return { steps, error: command.error };
            }
            steps.push(command.step);
            index = close + 1;
            textStart = index;
        }

        flushText(payload.length);
        return { steps, error: "" };
    }

    function parseMacroCommandPreview(commandText) {
        const command = String(commandText || "").trim();
        if (!command) {
            return { error: "Empty macro command." };
        }
        if (/^\\d+$/.test(command)) {
            return {
                step: {
                    kind: "delay",
                    kindLabel: "Delay",
                    detail: command + " ms",
                    raw: "{" + command + "}",
                    bytes: 3
                }
            };
        }
        if (/^[+-]/.test(command)) {
            const sign = command[0];
            const key = command.slice(1).trim();
            if (!key || key.includes(",")) {
                return { error: "Key down/up commands need exactly one key." };
            }
            const keyError = macroPayloadKeyListError([key]);
            if (keyError) {
                return { error: keyError };
            }
            return {
                step: {
                    kind: sign === "+" ? "down" : "up",
                    kindLabel: sign === "+" ? "Down" : "Up",
                    detail: displayKeyExpression(key),
                    raw: "{" + sign + key + "}",
                    bytes: 2
                }
            };
        }

        const keys = splitLayoutArguments(command).map((item) => item.trim()).filter(Boolean);
        if (!keys.length) {
            return { error: "Tap commands need at least one key." };
        }
        const keyError = macroPayloadKeyListError(keys);
        if (keyError) {
            return { error: keyError };
        }
        return {
            step: {
                kind: "tap",
                kindLabel: keys.length > 1 ? "Chord" : "Tap",
                detail: keys.map(displayKeyExpression).join(" + "),
                raw: "{" + keys.join(", ") + "}",
                bytes: 2 + keys.length
            }
        };
    }

    function macroPayloadStats(parsed, payload) {
        if (parsed.error) {
            return { bytes: 0 };
        }
        let bytes = 0;
        for (const step of parsed.steps) {
            if (step.kind === "text") {
                const length = step.detail.length;
                bytes += length + Math.ceil(length / 255) * 2;
            } else {
                bytes += step.bytes || 0;
            }
        }
        return { bytes, sourceChars: String(payload || "").length };
    }

    function insertMacroStep(target) {
        const workbench = target.closest("[data-macro-workbench]");
        const textarea = workbench?.querySelector("[data-macro-payload]");
        if (!workbench || !textarea) return false;

        const snippet = macroStepSnippet(workbench);
        if (!snippet) return false;

        const start = Number.isInteger(textarea.selectionStart) ? textarea.selectionStart : textarea.value.length;
        const end = Number.isInteger(textarea.selectionEnd) ? textarea.selectionEnd : start;
        textarea.value = textarea.value.slice(0, start) + snippet + textarea.value.slice(end);
        const cursor = start + snippet.length;
        textarea.focus();
        textarea.setSelectionRange(cursor, cursor);
        captureMacroDraftFromControl(textarea);
        validateControl(textarea);
        refreshMacroPreview(workbench);
        updateDirtySection(workbench.querySelector("[data-macro-editor]"));
        return true;
    }

    function clearMacroPayload(target) {
        const workbench = target.closest("[data-macro-workbench]");
        const textarea = workbench?.querySelector("[data-macro-payload]");
        if (!workbench || !textarea) return false;
        textarea.value = "";
        textarea.focus();
        captureMacroDraftFromControl(textarea);
        validateControl(textarea);
        refreshMacroPreview(workbench);
        updateDirtySection(workbench.querySelector("[data-macro-editor]"));
        return true;
    }

    function startMacroRecording(target) {
        const workbench = target?.closest?.("[data-macro-workbench]") || document.querySelector("[data-macro-workbench]");
        const slot = activeMacroSlot();
        if (!workbench || !slot) return false;
        captureActiveMacroDraft();
        captureMacroRecorderSettings();
        macroRecording = true;
        macroRecordedEvents = [];
        macroRecordingPrefix = macroPayloadForSlot(slot);
        macroRecorderLastEventAt = 0;
        macroRecorderHeldKeys = {};
        macroRecorderNotice = "Recording into " + displayKeyExpression(slot.keycode) + ".";
        macroRecorderHistoryStart = currentLocalSnapshot || serializeLocalState();
        refreshMacroRecorderPanel(workbench);
        return true;
    }

    function stopMacroRecording(commit) {
        if (!macroRecording) return false;
        macroRecording = false;
        macroRecorderHeldKeys = {};
        macroRecorderLastEventAt = 0;
        macroRecorderNotice = macroRecordedEvents.length ? "Recording stopped." : "Recording stopped with no captured events.";
        if (commit && macroRecorderHistoryStart) {
            commitLocalHistory(macroRecorderHistoryStart);
        }
        macroRecorderHistoryStart = "";
        refreshMacroRecorderPanel();
        return true;
    }

    function clearMacroRecording(target) {
        const hadState = macroRecordedEvents.length > 0 || Boolean(macroRecordingPrefix) || Boolean(macroRecorderNotice);
        const workbench = target?.closest?.("[data-macro-workbench]") || document.querySelector("[data-macro-workbench]");
        const restorePayload = macroRecordingPrefix || macroPayloadForSlot(activeMacroSlot());
        macroRecording = false;
        macroRecordedEvents = [];
        macroRecordingPrefix = restorePayload;
        macroRecorderLastEventAt = 0;
        macroRecorderHeldKeys = {};
        macroRecorderNotice = "Recording take cleared.";
        macroRecorderHistoryStart = "";
        if (workbench) {
            syncMacroRecordedPayloadDraft(true);
            refreshMacroRecorderPanel(workbench);
        }
        return hadState;
    }

    function clearMacroRecorderSession(resetPrefix) {
        macroRecording = false;
        macroRecordedEvents = [];
        macroRecorderLastEventAt = 0;
        macroRecorderHeldKeys = {};
        macroRecorderNotice = "";
        macroRecorderHistoryStart = "";
        if (resetPrefix) {
            macroRecordingPrefix = macroPayloadForSlot(activeMacroSlot());
        } else {
            macroRecordingPrefix = "";
        }
    }

    function resetMacroRecorderState() {
        macroRecording = false;
        macroRecordDelays = true;
        macroRecorderMode = "compact";
        macroRecorderDelayThreshold = "30";
        macroRecorderDelayRound = "10";
        macroRecordedEvents = [];
        macroRecordingPrefix = "";
        macroRecorderLastEventAt = 0;
        macroRecorderHeldKeys = {};
        macroRecorderNotice = "";
        macroRecorderHistoryStart = "";
    }

    function handleMacroRecorderKeyEvent(event) {
        if (!macroRecording || activeView !== "macros") return;
        event.preventDefault();
        event.stopPropagation();
        if (event.repeat) return;

        const phase = event.type === "keyup" ? "up" : "down";
        const recordKey = event.code || event.key || "";
        let keycode = macroRecorderKeycodeForEvent(event);
        if (phase === "up" && macroRecorderHeldKeys[recordKey]) {
            keycode = macroRecorderHeldKeys[recordKey];
        }
        if (!keycode) {
            macroRecorderNotice = "Unsupported key event: " + (event.code || event.key || "unknown");
            macroRecorderLastEventAt = macroRecorderNow();
            refreshMacroRecorderPanel();
            return;
        }
        if (macroPayloadKeycodes.size && !macroPayloadKeycodes.has(keycode)) {
            macroRecorderNotice = "Unsupported macro keycode: " + keycode;
            macroRecorderLastEventAt = macroRecorderNow();
            refreshMacroRecorderPanel();
            return;
        }
        const heldBeforeEvent = Object.keys(macroRecorderHeldKeys).length;
        if (phase === "down") {
            if (macroRecorderHeldKeys[recordKey]) return;
            macroRecorderHeldKeys[recordKey] = keycode;
        } else if (!macroRecorderHeldKeys[recordKey]) {
            macroRecorderNotice = "Ignored key-up without a matching key-down: " + displayKeyExpression(keycode);
            macroRecorderLastEventAt = macroRecorderNow();
            refreshMacroRecorderPanel();
            return;
        } else {
            delete macroRecorderHeldKeys[recordKey];
        }

        appendMacroRecorderDelay(phase, heldBeforeEvent);
        macroRecordedEvents = macroRecordedEvents.concat([{
            kind: "key",
            phase,
            keycode,
            code: event.code || "",
            key: event.key || ""
        }]);
        macroRecorderLastEventAt = macroRecorderNow();
        macroRecorderNotice = "Captured " + displayKeyExpression(keycode) + " " + phase + ".";
        syncMacroRecordedPayloadDraft();
        refreshMacroRecorderPanel();
    }

    function macroRecorderKeycodeForEvent(event) {
        const direct = macroRecorderEventCodeMap[event.code || ""];
        if (direct) return direct;
        const fallback = macroRecorderKeyFallbackMap[event.key || ""];
        if (fallback) return fallback;
        const key = String(event.key || "");
        if (/^[a-z]$/i.test(key)) return "KC_" + key.toUpperCase();
        if (/^\\d$/.test(key)) return "KC_" + key;
        return "";
    }

    function appendMacroRecorderDelay(phase, heldBeforeEvent) {
        const now = macroRecorderNow();
        if (!macroRecordDelays || !macroRecorderLastEventAt) return;
        if (macroRecorderMode === "compact" && (phase !== "down" || heldBeforeEvent > 0)) return;
        const elapsed = now - macroRecorderLastEventAt;
        const threshold = macroRecorderPositiveNumber(macroRecorderDelayThreshold, 30);
        const delay = roundedMacroRecorderDelay(elapsed);
        if (delay >= threshold) {
            macroRecordedEvents = macroRecordedEvents.concat([{ kind: "delay", value: delay }]);
        }
    }

    function roundedMacroRecorderDelay(value) {
        const round = macroRecorderPositiveNumber(macroRecorderDelayRound, 10);
        return Math.max(1, Math.round(Number(value || 0) / round) * round);
    }

    function macroRecorderPositiveNumber(value, fallback) {
        const number = Number(value);
        return Number.isFinite(number) && number > 0 ? number : fallback;
    }

    function macroRecorderNow() {
        return window.performance?.now ? window.performance.now() : Date.now();
    }

    function captureMacroRecorderSettings() {
        const mode = document.getElementById("macroRecorderMode")?.value || macroRecorderMode;
        macroRecorderMode = mode === "exact" ? "exact" : "compact";
        const delays = document.getElementById("macroRecordDelays");
        if (delays) {
            macroRecordDelays = Boolean(delays.checked);
        }
        const threshold = document.getElementById("macroRecorderDelayThreshold");
        if (threshold) {
            macroRecorderDelayThreshold = threshold.value || "";
        }
        const round = document.getElementById("macroRecorderDelayRound");
        if (round) {
            macroRecorderDelayRound = round.value || "";
        }
    }

    function syncMacroRecordedPayloadDraft(force = false) {
        if (!force && !macroRecording && !macroRecordedEvents.length && !macroRecordingPrefix) return;
        const slot = activeMacroSlot();
        if (!slot) return;
        const payload = macroRecordingPrefix + recordedMacroPayload();
        setMacroDraft(slot.keycode, payload);

        const workbench = document.querySelector("[data-macro-workbench]");
        if (workbench?.dataset.keycode === slot.keycode) {
            const textarea = workbench.querySelector("[data-macro-payload]");
            if (textarea) {
                textarea.value = payload;
                validateControl(textarea);
            }
            refreshMacroPreview(workbench);
            updateDirtySection(workbench.querySelector("[data-macro-editor]"));
        }
        refreshMacroSlotButton(slot.keycode);
    }

    function refreshMacroRecorderPanel(workbench = document.querySelector("[data-macro-workbench]")) {
        const slot = activeMacroSlot();
        const recorder = workbench?.querySelector("[data-macro-recorder]");
        if (!slot || !recorder) return;
        recorder.innerHTML = renderMacroRecorderBody(slot);
        validateSection(recorder, false);
        hydrateTooltips();
        scheduleMacroSlotBrowserHeightSync();
    }

    function refreshMacroSlotButton(keycode) {
        const slot = macroSlotByKeycode(keycode);
        if (!slot) return;
        for (const button of document.querySelectorAll(".macro-slot-button[data-keycode]")) {
            if (button.dataset.keycode !== keycode) continue;
            button.outerHTML = renderMacroSlotButton(slot);
            hydrateTooltips();
            return;
        }
    }

    function recordedMacroPayload() {
        const items = macroRecorderMode === "exact"
            ? exactRecordedMacroItems(macroRecordedEvents)
            : compactRecordedMacroItems(macroRecordedEvents);
        return recordedMacroItemsToPayload(items);
    }

    function exactRecordedMacroItems(events) {
        return events.map((event) => {
            if (event.kind === "delay") return { kind: "delay", value: event.value };
            return { kind: event.phase, keycode: event.keycode };
        });
    }

    function compactRecordedMacroItems(events) {
        const output = [];
        let segment = [];
        const flush = () => {
            if (!segment.length) return;
            output.push(...compactRecordedMacroSegment(segment));
            segment = [];
        };
        for (const event of events) {
            if (event.kind === "delay") {
                flush();
                output.push({ kind: "delay", value: event.value });
            } else {
                segment.push(event);
            }
        }
        flush();
        return output;
    }

    function compactRecordedMacroSegment(segment) {
        const output = [];
        let index = 0;
        while (index < segment.length) {
            const chord = compactRecordedChordAt(segment, index);
            if (chord) {
                output.push(chord.item);
                index += chord.length;
                continue;
            }
            const event = segment[index];
            const next = segment[index + 1];
            if (event.phase === "down" && next?.phase === "up" && next.keycode === event.keycode) {
                output.push({ kind: "tap", keycodes: [event.keycode] });
                index += 2;
                continue;
            }
            output.push({ kind: event.phase, keycode: event.keycode });
            index += 1;
        }
        return output;
    }

    function compactRecordedChordAt(segment, start) {
        const remaining = segment.length - start;
        const maxLength = remaining % 2 === 0 ? remaining : remaining - 1;
        for (let length = maxLength; length >= 2; length -= 2) {
            const half = length / 2;
            const slice = segment.slice(start, start + length);
            const downs = slice.slice(0, half);
            const ups = slice.slice(half);
            if (!downs.every((event) => event.phase === "down") || !ups.every((event) => event.phase === "up")) continue;
            if (!ups.every((event, index) => event.keycode === downs[half - index - 1].keycode)) continue;
            const keycodes = downs.map((event) => event.keycode);
            if (new Set(keycodes).size !== keycodes.length) continue;
            return {
                length,
                item: keycodes.length === 1 ? { kind: "tap", keycodes } : { kind: "chord", keycodes }
            };
        }
        return undefined;
    }

    function recordedMacroItemsToPayload(items) {
        let payload = "";
        let text = "";
        const flushText = () => {
            if (!text) return;
            payload += text;
            text = "";
        };
        for (const item of items) {
            if (item.kind === "tap" || item.kind === "chord") {
                const character = macroRecorderTextForKeys(item.keycodes || []);
                if (character) {
                    text += character;
                    continue;
                }
            }
            flushText();
            payload += macroRecorderCommandForItem(item);
        }
        flushText();
        return payload;
    }

    function macroRecorderCommandForItem(item) {
        if (item.kind === "delay") return "{" + Math.max(1, Math.round(Number(item.value || 0))) + "}";
        if (item.kind === "down") return "{+" + item.keycode + "}";
        if (item.kind === "up") return "{-" + item.keycode + "}";
        if (item.kind === "tap" || item.kind === "chord") return "{" + (item.keycodes || []).join(",") + "}";
        return "";
    }

    function macroRecorderTextForKeys(keycodes) {
        const keys = (keycodes || []).filter(Boolean);
        if (!keys.length) return "";
        const shiftKeys = new Set(["KC_LSFT", "KC_RSFT", "KC_LEFT_SHIFT", "KC_RIGHT_SHIFT"]);
        const blockedModifiers = new Set(["KC_LCTL", "KC_RCTL", "KC_LEFT_CTRL", "KC_RIGHT_CTRL", "KC_LALT", "KC_RALT", "KC_LEFT_ALT", "KC_RIGHT_ALT", "KC_LGUI", "KC_RGUI", "KC_LEFT_GUI", "KC_RIGHT_GUI"]);
        if (keys.some((key) => blockedModifiers.has(key))) return "";
        const shift = keys.some((key) => shiftKeys.has(key));
        const printable = keys.filter((key) => !shiftKeys.has(key));
        if (printable.length !== 1) return "";
        const pair = macroRecorderTextKeyMap[printable[0]];
        if (!pair) return "";
        const character = pair[shift ? 1 : 0];
        if (!macroRecorderCanEmitText(character)) return "";
        return character;
    }

    function macroRecorderCanEmitText(character) {
        if (!character || character === "{" || character === "}") return false;
        const code = character.charCodeAt(0);
        return code >= 0x20 && code <= 0x7E;
    }

    function macroStepSnippet(workbench) {
        const type = workbench.querySelector("#macroStepType")?.value || "tap";
        if (type === "text") {
            const input = workbench.querySelector("#macroStepText");
            const text = input?.value || "";
            if (!text) {
                reportFieldError(input, "Enter text to insert.");
                return "";
            }
            if (/[{}]/.test(text)) {
                reportFieldError(input, "Text steps cannot include { or }.");
                return "";
            }
            setFieldError(input, "");
            return text;
        }
        if (type === "delay") {
            const input = workbench.querySelector("#macroStepDelay");
            const delay = String(input?.value || "").trim();
            const error = validatePositiveInteger(delay, "Enter a positive delay in milliseconds.");
            if (error) {
                reportFieldError(input, error);
                return "";
            }
            setFieldError(input, "");
            return "{" + delay + "}";
        }
        if (type === "tap") {
            const input = workbench.querySelector("#macroStepKeys");
            const keys = canonicalMacroKeySequence(input?.value || "");
            if (!keys.length) {
                reportFieldError(input, "Choose at least one key.");
                return "";
            }
            const keyError = macroPayloadKeyListError(keys);
            if (keyError) {
                reportFieldError(input, keyError);
                return "";
            }
            setFieldError(input, "");
            return "{" + keys.join(",") + "}";
        }
        if (type === "down" || type === "up") {
            const input = workbench.querySelector("#macroStepKey");
            const keys = canonicalMacroKeySequence(input?.value || "");
            if (keys.length !== 1) {
                reportFieldError(input, "Choose exactly one key.");
                return "";
            }
            const keyError = macroPayloadKeyListError(keys);
            if (keyError) {
                reportFieldError(input, keyError);
                return "";
            }
            setFieldError(input, "");
            return "{" + (type === "down" ? "+" : "-") + keys[0] + "}";
        }
        return "";
    }

    function canonicalMacroKeySequence(value) {
        return splitLayoutArguments(String(value || ""))
            .flatMap(canonicalMacroKeyPart)
            .filter(Boolean);
    }

    function canonicalMacroKeyPart(value) {
        const normalized = normalizeDisplayExpression(value);
        if (!normalized) return [];

        const chordParts = normalized.split("+").map((part) => part.trim()).filter(Boolean);
        if (chordParts.length > 1) {
            const key = canonicalMacroPlainKey(chordParts[chordParts.length - 1]);
            const modifiers = chordParts.slice(0, -1).map(normalizeLayoutModifier);
            if (!key || modifiers.some((modifier) => !modifier)) return [];
            return uniqueMacroKeys(modifiers.map(macroModifierKeycode).concat([key]));
        }

        const canonical = canonicalLayoutKeyExpression(normalized);
        const call = canonical.match(/^([A-Z][A-Z0-9_]*)\\((.+)\\)$/);
        if (call && modWrapperLabels[call[1]]) {
            const key = canonicalMacroPlainKey(call[2]);
            const modifiers = (modWrapperLabels[call[1]] || []).map(macroModifierKeycode);
            return key ? uniqueMacroKeys(modifiers.concat([key])) : [];
        }
        return canonical ? [canonical] : [];
    }

    function canonicalMacroPlainKey(value) {
        const canonical = canonicalLayoutKeyExpression(value);
        if (!canonical || /^([A-Z][A-Z0-9_]*)\\(/.test(canonical)) return "";
        return canonical;
    }

    function macroModifierKeycode(modifier) {
        switch (normalizeLayoutModifier(modifier)) {
            case "Ctrl": return "KC_LCTL";
            case "Shift": return "KC_LSFT";
            case "Alt": return "KC_LALT";
            case "Cmd": return "KC_LGUI";
            case "Right Ctrl": return "KC_RCTL";
            case "Right Shift": return "KC_RSFT";
            case "Right Alt": return "KC_RALT";
            case "Right Cmd": return "KC_RGUI";
            default: return "";
        }
    }

    function macroPayloadKeyListError(keys) {
        for (const key of keys || []) {
            if (/^[A-Z][A-Z0-9_]*\\(/.test(key)) {
                return "Macro payloads need raw keycodes; use a key list such as KC_LGUI, KC_SPC instead of a wrapper.";
            }
            if (macroPayloadKeycodes.size && !macroPayloadKeycodes.has(key)) {
                return "Unsupported macro keycode: " + key;
            }
        }
        return "";
    }

    function uniqueMacroKeys(keys) {
        const seen = new Set();
        return keys.filter((key) => {
            if (!key || seen.has(key)) return false;
            seen.add(key);
            return true;
        });
    }

    function updateMacroComposerFields(root = document) {
        const type = root.querySelector("#macroStepType")?.value || "tap";
        for (const field of root.querySelectorAll("[data-macro-step-field]")) {
            const values = String(field.dataset.macroStepField || "").split(/\\s+/);
            const visible = values.includes(type);
            field.hidden = !visible;
            field.style.display = visible ? "" : "none";
            for (const control of field.querySelectorAll("input, select, textarea")) {
                if (!visible || macroStepEmptyFieldShouldStayNeutral(control, String(control.value || "").trim())) {
                    clearFieldError(control);
                } else {
                    validateControl(control);
                }
            }
        }
        updateMacroComposerInsertState(root);
        scheduleMacroSlotBrowserHeightSync();
    }

    function updateMacroComposerInsertState(root = document) {
        const composer = root.querySelector?.("[data-macro-composer]") || root.closest?.("[data-macro-composer]") || root;
        const button = composer?.querySelector?.("[data-action='insertMacroStep']");
        if (!button) return;
        button.disabled = Boolean(macroStepInsertError(root));
    }

    function macroStepInsertError(root = document) {
        const type = root.querySelector("#macroStepType")?.value || "tap";
        if (type === "text") {
            const text = root.querySelector("#macroStepText")?.value || "";
            if (!text) return "Enter text to insert.";
            return /[{}]/.test(text) ? "Text steps cannot include { or }." : "";
        }
        if (type === "delay") {
            const delay = String(root.querySelector("#macroStepDelay")?.value || "").trim();
            return validatePositiveInteger(delay, "Enter a positive delay in milliseconds.");
        }
        if (type === "tap") {
            const keys = canonicalMacroKeySequence(root.querySelector("#macroStepKeys")?.value || "");
            if (!keys.length) return "Choose at least one key.";
            return macroPayloadKeyListError(keys);
        }
        if (type === "down" || type === "up") {
            const keys = canonicalMacroKeySequence(root.querySelector("#macroStepKey")?.value || "");
            if (keys.length !== 1) return "Choose exactly one key.";
            return macroPayloadKeyListError(keys);
        }
        return "Choose a step type.";
    }

    function refreshMacroPreview(workbench) {
        if (!workbench) return;
        const preview = workbench.querySelector("[data-macro-preview]");
        const payload = workbench.querySelector("[data-macro-payload]")?.value || "";
        if (preview) {
            preview.innerHTML = renderMacroPreviewBody(payload);
            hydrateTooltips();
            scheduleMacroSlotBrowserHeightSync();
        }
    }

    function currentLayer() {
        return layerWithPendingLayoutEdits(layersForUi().find((layer) => layer.name === activeLayer) || layersForUi()[0]);
    }

    function layerWithPendingLayoutEdits(layer) {
        if (!layer) return layer;
        const edits = layerPendingLayoutEdits(layer.name);
        if (!Object.keys(edits).length) return layer;
        return {
            ...layer,
            positions: layer.positions.map((position) => {
                const keycode = edits[position.layoutIndex];
                if (!keycode) return position;
                return {
                    ...position,
                    keycode,
                    display: displayKeyExpression(keycode),
                    editLabel: displayKeyExpression(keycode),
                    pending: true,
                };
            })
        };
    }

    function displayAction(value) {
        return displayKeyExpression(value);
    }

    function editableActionExpression(value) {
        const normalized = normalizeDisplayExpression(value);
        const match = normalized.match(/^([A-Z][A-Z0-9_]*)\\((.+)\\)$/);
        if (match && modWrapperLabels[match[1]]) {
            return displayKeyExpression(normalized);
        }
        return qmkKeyLabels[normalized] || normalized;
    }

    function displayKeyExpression(value) {
        const normalized = normalizeDisplayExpression(value);
        if (!normalized) return "";
        const internal = authoredInternalKeyExpression(normalized);
        if (internal) return internal;
        if (normalized === "_______") return normalized;
        if (qmkKeyLabels[normalized]) return qmkKeyLabels[normalized];

        let match = normalized.match(/^LT\\(LAYER_([^,]+),\\s*(.+)\\)$/);
        if (match) return displayKeyExpression(match[2]) + ", hold " + titleCase(match[1]);

        match = normalized.match(/^MO\\(LAYER_([^)]+)\\)$/);
        if (match) return "Hold " + titleCase(match[1]);

        match = normalized.match(/^LOCK_LAYER\\(LAYER_([^)]+)\\)$/);
        if (match) return "Lock " + titleCase(match[1]);

        match = normalized.match(/^([A-Z][A-Z0-9_]*)\\((.+)\\)$/);
        if (match && modWrapperLabels[match[1]]) {
            return modWrapperLabels[match[1]].join("+") + "+" + displayKeyExpression(match[2]);
        }
        if (match) return normalized;

        match = normalized.match(/^VIA_MACRO_(\\d+)$/);
        if (match) return "VIA Macro " + match[1];

        match = normalized.match(/^MACRO_(\\d+)$/);
        if (match) return "Macro " + match[1];

        if (normalized.endsWith("_MODE_LOCK")) return titleCase(normalized.replace(/_MODE_LOCK$/, "")) + " Lock";
        if (normalized.endsWith("_MODE")) return titleCase(normalized.replace(/_MODE$/, ""));
        if (normalized.endsWith("_LOCK")) return titleCase(normalized.replace(/_LOCK$/, "")) + " Lock";
        if (normalized.startsWith("KC_")) return titleCase(normalized.slice(3));
        return normalized;
    }

    function normalizeDisplayExpression(value) {
        return String(value || "").replace(/\\s+/g, " ").replace(/\\s*,\\s*/g, ", ").trim();
    }

    function titleCase(value) {
        return String(value || "")
            .toLowerCase()
            .split("_")
            .filter(Boolean)
            .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
            .join(" ");
    }

    function escapeHtml(value) {
        return String(value == null ? "" : value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
    }

    function escapeAttr(value) {
        return escapeHtml(value);
    }

    post({ type: "ready" });
}());
`;
}

function getNonce() {
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let nonce = "";
    for (let index = 0; index < 32; index += 1) {
        nonce += alphabet[Math.floor(Math.random() * alphabet.length)];
    }
    return nonce;
}

module.exports = {
    activate,
    deactivate,
};
