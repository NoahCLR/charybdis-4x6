"use strict";

const vscode = require("vscode");
const fs = require("fs/promises");
const path = require("path");

const KEYMAP_RELATIVE_PATH = path.join(
    "keyboards",
    "bastardkb",
    "charybdis",
    "4x6",
    "keymaps",
    "noah",
    "keymap.c"
);
const RGB_RELATIVE_PATH = path.join(
    "keyboards",
    "bastardkb",
    "charybdis",
    "4x6",
    "keymaps",
    "noah",
    "rgb_config.c"
);
const KEYMAP_CONFIG_RELATIVE_PATH = path.join(
    "keyboards",
    "bastardkb",
    "charybdis",
    "4x6",
    "keymaps",
    "noah",
    "config.h"
);
const QMK_KEYCODE_DATA_RELATIVE_PATH = path.join("data", "constants", "keycodes");

const LAYOUT_SLOT_COUNT = 56;
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
    "KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS",
];
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
    disabled: "XXXXXXX",
    none: "XXXXXXX",
    no: "XXXXXXX",
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

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand("charybdisProfileStudio.open", () => openStudio(context))
    );
    setupNativeEntryPoints(context);
}

function deactivate() {}

async function setupNativeEntryPoints(context) {
    const root = await findProfileRoot();
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
    const root = await findProfileRoot();
    if (!root) {
        vscode.window.showErrorMessage(
            `Could not find ${KEYMAP_RELATIVE_PATH} and ${RGB_RELATIVE_PATH} in the open workspace.`
        );
        return;
    }

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
                await handleWebviewMessage(panel, root, message);
            } catch (error) {
                const text = error instanceof Error ? error.message : String(error);
                panel.webview.postMessage({ type: "error", message: text });
                vscode.window.showErrorMessage(`Charybdis Profile Studio: ${text}`);
            }
        },
        undefined,
        context.subscriptions
    );

    await postModel(panel, root);
}

async function findProfileRoot() {
    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        const root = folder.uri.fsPath;
        if (await fileExists(path.join(root, KEYMAP_RELATIVE_PATH)) && await fileExists(path.join(root, RGB_RELATIVE_PATH))) {
            return root;
        }
    }

    const activeFile = vscode.window.activeTextEditor?.document?.uri.fsPath;
    if (activeFile) {
        let cursor = path.dirname(activeFile);
        while (cursor !== path.dirname(cursor)) {
            if (
                await fileExists(path.join(cursor, KEYMAP_RELATIVE_PATH)) &&
                await fileExists(path.join(cursor, RGB_RELATIVE_PATH))
            ) {
                return cursor;
            }
            cursor = path.dirname(cursor);
        }
    }

    return undefined;
}

async function fileExists(filePath) {
    try {
        await fs.access(filePath);
        return true;
    } catch {
        return false;
    }
}

async function handleWebviewMessage(panel, root, message) {
    switch (message?.type) {
        case "ready":
        case "refresh":
            await postModel(panel, root);
            return;
        case "openSource":
            await openSource(root, message.file);
            return;
        case "updateLayoutKeys":
            await patchLayoutKeys(root, message.layer, message.changes);
            await postModel(panel, root, "Updated keymap.c layout keys.");
            return;
        case "updateLayerColor":
            await patchLayerColor(root, message.layer, message.hue, message.sat, message.val, message.mode);
            await postModel(panel, root, "Updated rgb_config.c layer color.");
            return;
        case "updatePdModeColor":
            await patchPdModeColor(root, message.pointingMode, message.hue, message.sat, message.val, message.locality);
            await postModel(panel, root, "Updated rgb_config.c pointing-mode color.");
            return;
        case "updateAutomouseFade":
            await patchAutomouseFade(root, message.mode, message.hue, message.sat, message.val);
            await postModel(panel, root, "Updated rgb_config.c auto-mouse fade.");
            return;
        case "updateComboFeedback":
            await patchComboFeedback(root, message.hue, message.sat, message.val, message.locality);
            await postModel(panel, root, "Updated rgb_config.c combo feedback.");
            return;
        case "updateKeyBehaviorFeedback":
            await patchKeyBehaviorFeedback(root, message.config);
            await postModel(panel, root, "Updated rgb_config.c key behavior feedback.");
            return;
        case "addRgbLedGroup":
            await appendRgbLedGroup(root, message.group);
            await postModel(panel, root, "Added rgb_config.c LED group row.");
            return;
        case "updateViaMacro":
            await patchViaMacro(root, message.keycode, message.payload);
            await postModel(panel, root, "Updated keymap.c VIA macro payload.");
            return;
        case "addCombo":
            await appendCombo(root, message.output, message.inputs);
            await postModel(panel, root, "Added keymap.c combo row.");
            return;
        case "addBehavior":
            await appendKeyBehavior(root, message.behavior);
            await postModel(panel, root, "Added keymap.c key behavior row.");
            return;
        case "saveBehavior":
            await saveKeyBehavior(root, message.behavior);
            await postModel(panel, root, "Saved keymap.c key behavior row.");
            return;
        default:
            throw new Error(`Unknown studio message: ${message?.type}`);
    }
}

async function openSource(root, file) {
    const relative = file === "rgb" ? RGB_RELATIVE_PATH : KEYMAP_RELATIVE_PATH;
    const document = await vscode.workspace.openTextDocument(path.join(root, relative));
    await vscode.window.showTextDocument(document, vscode.ViewColumn.Beside);
}

async function postModel(panel, root, notice) {
    const model = await buildModel(root);
    panel.webview.postMessage({ type: "model", model, notice });
}

async function buildModel(root) {
    const keymapPath = path.join(root, KEYMAP_RELATIVE_PATH);
    const rgbPath = path.join(root, RGB_RELATIVE_PATH);
    const configPath = path.join(root, KEYMAP_CONFIG_RELATIVE_PATH);
    const [keymapText, rgbText, configText] = await Promise.all([
        fs.readFile(keymapPath, "utf8"),
        fs.readFile(rgbPath, "utf8"),
        fs.readFile(configPath, "utf8").catch(() => ""),
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
    const qmkKeycodeCatalog = await loadQmkKeycodeCatalog(root).catch((error) => {
        diagnostics.push(`qmkKeycodes: ${error instanceof Error ? error.message : String(error)}`);
        return fallbackQmkKeycodeCatalog();
    });

    return {
        root,
        files: {
            keymap: KEYMAP_RELATIVE_PATH,
            rgb: RGB_RELATIVE_PATH,
        },
        layers: safe("layers", [], () => parseLayers(keymapText)),
        keyBehaviors: safe("keyBehaviors", [], () => parseKeyBehaviors(keymapText)),
        combos: safe("combos", [], () => parseMacroTable(keymapText, "COMBOS", "COMBO").map(parseComboRow)),
        viaMacros: safe("viaMacros", [], () => parseMacroTable(keymapText, "VIA_MACROS", "MACRO").map((row) => parseMacroSlot(row, "via"))),
        hardcodedMacros: safe("hardcodedMacros", [], () =>
            parseMacroTable(keymapText, "HARDCODED_MACROS", "MACRO").map((row) => parseMacroSlot(row, "hardcoded"))
        ),
        behaviorTimingDefaults: safe("behaviorTimingDefaults", {}, () => resolveBehaviorTimingDefaults(configMacros)),
        rgb: safe("rgb", {}, () => parseRgbConfig(rgbText, configMacros)),
        qmkKeycodes: qmkKeycodeCatalog.entries,
        qmkKeyLabels: qmkKeycodeCatalog.labels,
        qmkKeycodeSource: qmkKeycodeCatalog.source,
        diagnostics,
    };
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
    return body.match(new RegExp(`"${escapeRegex(field)}"\\s*:\\s*"([^"]+)"`))?.[1] || "";
}

function extractHjsonStringListField(body, field) {
    const match = body.match(new RegExp(`"${escapeRegex(field)}"\\s*:\\s*\\[([\\s\\S]*?)\\]`));
    if (!match) {
        return [];
    }
    return Array.from(match[1].matchAll(/"([^"]+)"/g)).map((item) => item[1]);
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
        labels: {...QMK_KEY_LABELS},
    };
}

function compareQmkKeycodes(left, right) {
    const groupOrder = ["internal", "basic", "modifiers", "media", "system", "mouse", "rgb", "rgb_matrix", "led_matrix", "backlight", "underglow", "magic", "quantum"];
    const leftGroup = groupOrder.indexOf(left.group);
    const rightGroup = groupOrder.indexOf(right.group);
    if (leftGroup !== rightGroup) {
        return (leftGroup === -1 ? 999 : leftGroup) - (rightGroup === -1 ? 999 : rightGroup);
    }
    return String(left.label || left.value).localeCompare(String(right.label || right.value), undefined, {numeric: true});
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
            const keycode = argsBody.slice(tokenRange.start, tokenRange.end).trim();
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
                branchConfirmTerm: normalizeExpr(fields[".branch_confirm_term"] || ""),
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

function resolveBehaviorTimingDefaults(macros) {
    return {
        tappingTerm: normalizeExpr(macros.TAPPING_TERM || ""),
        tapHoldTerm: normalizeExpr(macros.CUSTOM_TAP_HOLD_TERM || ""),
        longerHoldTerm: normalizeExpr(macros.CUSTOM_LONGER_HOLD_TERM || ""),
        multiTapTerm: normalizeExpr(macros.CUSTOM_MULTI_TAP_TERM || ""),
        branchConfirmTerm: normalizeExpr(macros.CUSTOM_TAP_BRANCH_CONFIRM_TERM || ""),
    };
}

function parseRgbConfig(text, configMacros = {}) {
    return {
        layerColors: parseLayerColors(text),
        layerLedGroups: parseRgbLedGroupTable(text, "layer_led_groups_data", ".layer"),
        pdModeColors: parsePdModeColors(text),
        pdModeLedGroups: parseRgbLedGroupTable(text, "pd_mode_led_groups_data", ".pointing_mode"),
        comboFeedback: parseSimpleColorStruct(text, /combo_feedback_colors\s*=/, [".color", ".locality"]),
        comboFeedbackLedGroups: parseRgbLedGroupTable(text, "combo_feedback_led_groups_data"),
        automouseFade: parseSimpleColorStruct(text, /automouse_fade_end_config\s*=/, [".mode", ".end_color"]),
        keyBehaviorFeedback: parseKeyBehaviorFeedback(text),
        keyBehaviorFeedbackLedGroups: parseRgbLedGroupTable(text, "key_behavior_feedback_led_groups_data", ".semantic"),
        defaultColor: resolveDefaultRgbColor(configMacros),
    };
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

function parseRgbLedGroupTable(text, tableName, ownerField) {
    let body;
    try {
        body = findCallBody(text, new RegExp(`${escapeRegex(tableName)}\\s*\\[\\]\\s*=\\s*RGB_LED_GROUP_TABLE`));
    } catch {
        return [];
    }

    const macros = parseRgbLedGroupMacros(text);
    return splitTopLevelWithRanges(body)
        .map((item) => stripComments(body.slice(item.start, item.end)).trim())
        .map(trimOuterInitializer)
        .filter(Boolean)
        .map((entry) => {
            const fields = parseDesignatedFields(entry);
            const ledGroup = parseLedGroupExpression(fields[".led_group"] || "", macros);
            const row = {
                color: parseHsv(fields[".color"]),
                ledGroup: normalizeExpr(fields[".led_group"] || ""),
                ledIndices: ledGroup,
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
    const macros = {};
    const pattern = /#\s*define\s+(RGB_LED_GROUP_[A-Z0-9_]+)\s+RGB_LED_GROUP\s*\(([^)]*)\)/g;
    let match;
    while ((match = pattern.exec(text)) !== null) {
        macros[match[1]] = splitTopLevel(match[2]).map(normalizeExpr).filter(Boolean);
    }
    return macros;
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

async function patchLayoutKeys(root, layer, changes) {
    const normalizedChanges = normalizeLayoutKeyChanges(changes);
    if (!normalizedChanges.length) {
        return;
    }

    const context = await readLayoutSlotContext(root, layer);
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

function normalizeLayoutKeyChanges(changes) {
    const byIndex = new Map();
    for (const change of Array.isArray(changes) ? changes : []) {
        const layoutIndex = Number(change?.layoutIndex);
        assertLayoutIndex(layoutIndex);
        const keycode = normalizeUserKeyExpression(change?.keycode || "");
        assertSafeExpression(keycode, "keycode");
        byIndex.set(layoutIndex, normalizeExpr(keycode));
    }
    return Array.from(byIndex.entries()).map(([layoutIndex, keycode]) => ({ layoutIndex, keycode }));
}

async function readLayoutSlotContext(root, layer) {
    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
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

async function patchLayerColor(root, layer, hue, sat, val, mode) {
    assertSafeIdentifier(layer, "layer");
    assertSafeHsv(hue, sat, val);
    assertAllowed(mode, LAYER_COLOR_MODES, "layer color mode");

    const filePath = path.join(root, RGB_RELATIVE_PATH);
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

async function patchPdModeColor(root, pointingMode, hue, sat, val, locality) {
    assertSafeIdentifier(pointingMode, "pointing mode");
    assertSafeHsv(hue, sat, val);
    assertAllowed(locality, RGB_LOCALITIES, "RGB locality");

    const filePath = path.join(root, RGB_RELATIVE_PATH);
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

async function patchAutomouseFade(root, mode, hue, sat, val) {
    assertAllowed(mode, AUTOMOUSE_FADE_MODES, "auto-mouse fade mode");
    assertSafeHsv(hue, sat, val);

    const filePath = path.join(root, RGB_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /automouse_fade_end_config\s*=/);
    let next = patchFieldExpressionInRange(text, initializer.bodyStart, initializer.bodyEnd, ".mode", mode);
    next = patchFieldInInitializer(next, /automouse_fade_end_config\s*=/, ".end_color", hsvExpression(hue, sat, val));
    await writeText(filePath, next);
}

async function patchComboFeedback(root, hue, sat, val, locality) {
    assertSafeHsv(hue, sat, val);
    assertAllowed(locality, RGB_LOCALITIES, "combo feedback locality");

    const filePath = path.join(root, RGB_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /combo_feedback_colors\s*=/);
    let next = patchFieldExpressionInRange(text, initializer.bodyStart, initializer.bodyEnd, ".locality", locality);
    next = patchFieldInInitializer(next, /combo_feedback_colors\s*=/, ".color", hsvExpression(hue, sat, val));
    await writeText(filePath, next);
}

async function patchKeyBehaviorFeedback(root, config) {
    const colors = {
        tapPendingColor: normalizeHsvRequest(config?.tapPendingColor, "tap pending color"),
        tapCommittedColor: normalizeHsvRequest(config?.tapCommittedColor, "tap committed color"),
        holdActiveColor: normalizeHsvRequest(config?.holdActiveColor, "hold active color"),
        longHoldActiveColor: normalizeHsvRequest(config?.longHoldActiveColor, "long-hold active color"),
        tapBranchColors: Array.isArray(config?.tapBranchColors)
            ? config.tapBranchColors.map((color, index) => normalizeHsvRequest(color, `tap branch ${index} color`))
            : [],
    };
    const tapCommitMode = normalizeExpr(config?.tapCommitMode || "");
    const locality = normalizeExpr(config?.locality || "");
    assertAllowed(tapCommitMode, KEY_FEEDBACK_TAP_COMMIT_MODES, "tap commit mode");
    assertAllowed(locality, RGB_LOCALITIES, "key behavior feedback locality");

    const filePath = path.join(root, RGB_RELATIVE_PATH);
    let text = await fs.readFile(filePath, "utf8");
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_pending_color", colors.tapPendingColor.expression);
    text = patchRgbTapBranchColorsInInitializer(text, /key_behavior_feedback_colors\s*=/, colors.tapBranchColors);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_committed_color", colors.tapCommittedColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".tap_commit_mode", tapCommitMode);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".hold_active_color", colors.holdActiveColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".long_hold_active_color", colors.longHoldActiveColor.expression);
    text = patchFieldInInitializer(text, /key_behavior_feedback_colors\s*=/, ".locality", locality);
    await writeText(filePath, text);
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

async function appendRgbLedGroup(root, group) {
    const target = normalizeExpr(group?.target || "");
    const config = RGB_LED_GROUP_TARGETS[target];
    if (!config) {
        throw new Error(`Invalid RGB LED group target: ${target}`);
    }

    const ledIndices = normalizeLedIndices(group?.ledIndices);
    if (!ledIndices.length) {
        throw new Error("Select at least one LED for the RGB group.");
    }

    assertSafeHsv(group?.hue, group?.sat, group?.val);

    const fields = [];
    if (config.ownerField) {
        const owner = normalizeExpr(group?.owner || "");
        assertSafeIdentifier(owner, config.ownerLabel);
        fields.push(`${config.ownerField} = ${owner}`);
    }
    fields.push(`.color = HSV(${normalizeExpr(group?.hue)}, ${normalizeExpr(group?.sat)}, ${normalizeExpr(group?.val)})`);
    fields.push(`.led_group = RGB_LED_GROUP(${ledIndices.join(", ")})`);

    const filePath = path.join(root, RGB_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
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

async function patchViaMacro(root, keycode, payload) {
    assertSafeIdentifier(keycode, "VIA macro keycode");

    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
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

async function appendCombo(root, output, inputs) {
    output = normalizeUserKeyExpression(output || "");
    const inputList = splitTopLevel(String(inputs || ""))
        .map(normalizeUserKeyExpression)
        .filter(Boolean);
    assertSafeExpression(output, "combo output");
    if (inputList.length < 2) {
        throw new Error("Combo inputs must include at least two keycodes.");
    }
    for (const input of inputList) {
        assertSafeExpression(input, "combo input");
    }

    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const block = findMacroDefinitionBlock(text, "COMBOS");
    const lines = block.text.split(/\r?\n/);
    const insertLine = lines.findIndex((line, index) => index > 0 && line.includes("/* COMBO("));
    const insertionIndex = insertLine === -1 ? Math.max(lines.length - 1, 1) : insertLine;
    const row = `    COMBO(${output}, (${inputList.join(", ")}))                          \\`;
    lines.splice(insertionIndex, 0, row);
    await writeText(filePath, replaceRange(text, block.start, block.end, lines.join("\n")));
}

async function appendKeyBehavior(root, behavior) {
    const row = renderBehaviorRowFromRequest(behavior);
    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /key_behaviors\s*\[\]\s*=/);
    const insertion = `\n${row}\n`;
    await writeText(filePath, replaceRange(text, initializer.bodyEnd, initializer.bodyEnd, insertion));
}

async function saveKeyBehavior(root, behavior) {
    const normalizedKeycode = normalizeUserKeyExpression(behavior?.keycode || "");
    assertSafeExpression(normalizedKeycode, "behavior keycode");

    const row = renderBehaviorRowFromRequest({ ...behavior, keycode: normalizedKeycode });
    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const initializer = findInitializerBody(text, /key_behaviors\s*\[\]\s*=/);
    const existing = findKeyBehaviorEntry(initializer.body, normalizedKeycode);

    if (existing) {
        await writeText(filePath, replaceRange(text, initializer.bodyStart + existing.start, initializer.bodyStart + existing.end, row));
        return;
    }

    await writeText(filePath, replaceRange(text, initializer.bodyEnd, initializer.bodyEnd, `\n${row}\n`));
}

function findKeyBehaviorEntry(body, keycode) {
    for (const entry of splitTopLevelWithRanges(body)) {
        const text = body.slice(entry.start, entry.end).trim();
        const inner = trimOuterInitializer(text);
        if (!inner) {
            continue;
        }
        const fields = parseDesignatedFields(inner);
        if (normalizeExpr(fields[".keycode"] || "") === keycode) {
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
    const branchConfirmTerm = normalizeOptionalBranchConfirmTerm(behavior?.branchConfirmTerm);

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

    return renderKeyBehaviorRow(normalizedKeycode, { tapHoldTerm, longerHoldTerm, multiTapTerm, branchConfirmTerm }, steps);
}

function normalizeOptionalTerm(value, label) {
    const term = normalizeExpr(value || "");
    if (!term) {
        return "";
    }
    if (/^\d+$/.test(term) || /^KEY_BEHAVIOR_TERM\(\d+\)$/.test(term)) {
        return term;
    }
    throw new Error(`${label} must be a positive integer when provided.`);
}

function normalizeOptionalBranchConfirmTerm(value) {
    const term = normalizeExpr(value || "");
    if (!term) {
        return "";
    }
    if (/^\d+$/.test(term)) {
        return `KEY_BEHAVIOR_TERM(${term})`;
    }
    if (/^KEY_BEHAVIOR_TERM\(\d+\)$/.test(term)) {
        return term;
    }
    throw new Error("branch_confirm_term must be a positive integer or KEY_BEHAVIOR_TERM(ms) when provided.");
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
    if (timings.branchConfirmTerm) timingFields.push(`                .branch_confirm_term = ${timings.branchConfirmTerm},`);
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
        throw new Error(`Expected ${items.length} tap branch colors, got ${colors.length}.`);
    }

    let next = text;
    for (let index = items.length - 1; index >= 0; index -= 1) {
        const item = items[index];
        const itemText = args.slice(item.start, item.end);
        const hsvMatch = /HSV\s*\([^)]*\)/.exec(itemText);
        if (!hsvMatch) {
            throw new Error(`Could not patch tap branch color ${index}.`);
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
    if (QMK_KEY_LABELS[normalized]) {
        return QMK_KEY_LABELS[normalized];
    }

    let match = normalized.match(/^LT\(LAYER_([^,]+),\s*(.+)\)$/);
    if (match) {
        return `${displayKeyExpression(match[2])} / hold ${titleCase(match[1])}`;
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
        h1, h2, h3 { margin: 0; font-weight: 650; }
        h1 { font-size: 18px; }
        h2 { font-size: 15px; margin-bottom: 10px; }
        h3 { font-size: 13px; margin-bottom: 8px; color: var(--muted); }
        button, input, select {
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
        button:hover { border-color: var(--accent); }
        input, select {
            min-height: 32px;
            padding: 5px 8px;
            width: 100%;
        }
        input:not(:disabled), select:not(:disabled) {
            border-color: #60707a;
            background: #20262a;
            box-shadow: inset 0 0 0 1px rgba(49, 198, 164, 0.08);
        }
        input.invalid, select.invalid {
            border-color: var(--danger);
            box-shadow: inset 0 0 0 1px rgba(255, 107, 107, 0.36), 0 0 0 1px rgba(255, 107, 107, 0.24);
        }
        input:disabled, select:disabled {
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
        .stack { display: grid; gap: 14px; }
        .view-tabs {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            margin-bottom: 14px;
        }
        .view-tab {
            min-width: 104px;
            text-align: center;
        }
        .view-tab.active {
            border-color: var(--accent);
            background: #1f5d52;
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
        .board {
            overflow-x: auto;
            display: grid;
            min-width: 0;
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
            padding: 22px 32px 0;
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
        .layout-board-apply {
            position: absolute;
            left: 32px;
            bottom: 22px;
            z-index: 1;
        }
        .layout-board-apply button {
            min-width: 190px;
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
        .layout-board-svg .svg-key.drag-source {
            opacity: 0.62;
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
            list-style-position: outside;
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
            max-width: 340px;
            padding: 7px 9px;
            border: 1px solid #6b7c85;
            border-radius: 6px;
            background: #151a1d;
            color: var(--text);
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.36);
            pointer-events: none;
            white-space: normal;
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
            flex: 0 0 calc(var(--key-units, 1) * 42px);
            min-width: 46px;
            min-height: 38px;
            padding: 6px 8px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            text-align: center;
        }
        .key-picker-keyboard {
            display: grid;
            gap: 8px;
            min-width: 0;
        }
        .key-picker-grid:not(.key-picker-keyboard) .key-picker-row {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(92px, 1fr));
            gap: 6px;
        }
        .key-picker-grid:not(.key-picker-keyboard) .key-picker-key {
            flex: initial;
            width: 100%;
            min-width: 0;
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
            .layout-with-key-editor {
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
</head>
<body>
    <header>
        <div>
            <h1>Charybdis Profile Studio</h1>
            <div id="subtitle" class="muted">Loading keymap.c and rgb_config.c</div>
        </div>
        <div class="toolbar">
            <button id="openKeymap">Open keymap.c</button>
            <button id="openRgb">Open rgb_config.c</button>
            <button id="refresh" class="primary">Refresh</button>
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
    let activeView = "layout";
    let rgbGroupTarget = "layer";
    let rgbGroupOwner = "";
    let rgbSelectedLeds = [];
    let rgbBuilderColor = undefined;
    let layoutComboPicking = false;
    let layoutComboSelection = [];
    let layoutComboOutput = "";
    let layoutComboInputs = "";
    let pendingLayoutEdits = {};
    let copiedLayoutKey = "";
    let layoutDragState = undefined;
    let suppressNextLayoutClick = false;
    let lastLayoutKeyClick = { index: undefined, time: 0 };
    let keyPicker = undefined;
    let notice = "";
    let localUndoStack = [];
    let localRedoStack = [];
    let currentLocalSnapshot = "";
    let restoringLocalSnapshot = false;
    const localHistoryLimit = 100;
    const tapCountNames = ${JSON.stringify(TAP_COUNT_NAMES)};
    const views = [
        ["layout", "Layout"],
        ["macros", "Macros & combos"],
        ["rgb", "RGB"]
    ];
    const headerTooltips = {
        openKeymap: "Open keymap.c beside the studio so you can inspect or hand-edit the source.",
        openRgb: "Open rgb_config.c beside the studio so you can inspect or hand-edit the source.",
        refresh: "Re-read keymap.c and rgb_config.c from disk and rebuild the studio model."
    };
    const viewTooltips = {
        layout: "Edit layer keys and behavior rows using the physical keyboard layout as the filter.",
        macros: "Edit VIA macro payloads and append combo rows in keymap.c.",
        rgb: "Edit rgb_config.c colors, feedback policies, and LED group tables."
    };
    const panelTooltips = {
        Status: "Parser messages, write status, and warnings from the current studio model.",
        Layout: "Physical keyboard preview, selected-key editor, and selected-key behavior editor for the active layer.",
        "Layer Overview": "Behavior rows, macros, combos, and pointing modes reachable from keys on the active layer.",
        "VIA Macros": "Payload strings for the VIA_MACROS(MACRO) table in keymap.c.",
        "Combo Builder": "Append a new COMBOS(COMBO) row to keymap.c.",
        "RGB LED Group Builder": "Select physical LEDs and append a row to one of the rgb_config.c LED group tables.",
        "Layer Colors": "Edit layer_colors[] HSV values and layer render mode.",
        "Layer LED Groups": "Inspect layer-specific LED group rows from rgb_config.c.",
        "Auto-mouse Fade": "Edit the auto-mouse fade destination color and fade mode.",
        "Pointing-mode Colors": "Edit pd_mode_colors[] HSV values and locality.",
        "Pointing-mode LED Groups": "Inspect pointing-mode-specific LED group rows from rgb_config.c.",
        "Combo Feedback": "Edit combo feedback color and locality.",
        "Combo Feedback LED Groups": "Inspect combo feedback LED group rows from rgb_config.c.",
        "Key Behavior Feedback": "Edit tap, hold, long-hold, and tap-branch feedback colors and policy.",
        "Key Behavior Feedback LED Groups": "Inspect key-behavior feedback LED group rows from rgb_config.c."
    };
    const actionTooltips = {
        applyKey: "Stage the selected key value as a pending layout edit.",
        saveSelectedBehavior: "Create or replace the key_behaviors[] row for this selected keycode.",
        addBehavior: "Append a simple key_behaviors[] row to keymap.c.",
        updateLayerColor: "Write this layer color and render mode back to rgb_config.c.",
        updatePdModeColor: "Write this pointing-mode color and locality back to rgb_config.c.",
        updateAutomouseFade: "Write the auto-mouse fade color and mode back to rgb_config.c.",
        updateComboFeedback: "Write combo feedback color and locality back to rgb_config.c.",
        updateKeyBehaviorFeedback: "Write all key behavior feedback colors and policy fields back to rgb_config.c.",
        addRgbLedGroup: "Append a new LED group row using the selected LEDs and current color.",
        clearRgbSelection: "Remove all currently selected LEDs from the group builder.",
        toggleRgbTrackball: "Add or remove the trackball LED index 56 from the group builder.",
        updateViaMacro: "Write this VIA macro payload string back to keymap.c.",
        addCombo: "Append a combo row with the entered output and input keys.",
        addLayoutCombo: "Append a combo row using the selected layout keys as inputs.",
        applyLayoutChanges: "Write pending layout drag/drop and paste edits back to keymap.c.",
        toggleLayoutComboPicking: "Toggle layout combo input selection.",
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
        "addRgbLedGroup",
        "updateViaMacro",
        "addCombo",
        "addLayoutCombo"
    ]);
    const fieldTooltips = {
        layer: "The active firmware layer. This is read from the LAYOUT() block and is not edited here.",
        "layout index": "The physical LAYOUT() slot index for the selected key. It is fixed by the keyboard geometry.",
        key: "User-facing key label or expression to write into the selected LAYOUT() slot, for example A, Enter, Space, _______, or Shift+Esc.",
        source: "The raw C expression currently stored in keymap.c.",
        tap_hold_term: "Optional milliseconds before a tap can become a hold for this behavior row.",
        longer_hold_term: "Optional milliseconds before a hold can become a long hold.",
        multi_tap_term: "Optional milliseconds used to detect repeated taps.",
        branch_confirm_term: "Optional milliseconds before a tap branch is committed. Plain numbers are written as KEY_BEHAVIOR_TERM(ms).",
        table: "Choose which rgb_config.c LED group table will receive the new row.",
        owner: "The owner value for the target LED group table. Combo feedback groups do not need one.",
        "pointing mode": "The pointing mode whose color or LED group is being edited.",
        semantic: "The key-behavior feedback semantic that owns this LED group.",
        mode: "Select the authored mode for this row, such as layer render mode or auto-mouse fade mode.",
        locality: "Choose which keyboard half or key region receives this RGB feedback.",
        "tap commit mode": "Choose when tap commit feedback is shown for key behavior taps.",
        picker: "Pick an approximate RGB color. The studio converts it into HSV channel values.",
        h: "HSV hue channel as QMK stores it, usually 0-255.",
        s: "HSV saturation channel as QMK stores it, usually 0-255.",
        v: "HSV value/brightness channel. Constants such as RGB_MATRIX_MAXIMUM_BRIGHTNESS are allowed.",
        output: "The key or action produced by a combo.",
        "output behavior": "The key behavior row that runs when this combo output keycode has authored behavior.",
        inputs: "Comma-separated combo input keys, such as D, F.",
        slot: "The VIA macro keycode slot.",
        payload: "The string payload sent by this VIA macro slot.",
        leds: "The physical RGB LED indices contained in this group.",
        "led group": "The authored LED group expression in rgb_config.c.",
        color: "The HSV color expression used by this row.",
        rgb: "The RGB color and locality associated with this reachable pointing mode.",
        badge: "The small badge shown on the layout preview for this combo.",
        behavior: "The key behavior row attached to this keycode.",
        steps: "Tap branch actions for this behavior row.",
        "key on layer": "Keys on the active layer that use this behavior row.",
        "reachable via": "The visible key or behavior action that can reach this pointing mode."
    };
    const qmkKeyLabels = ${JSON.stringify(QMK_KEY_LABELS)};
    const qmkKeycodeSectionGroups = ${JSON.stringify(QMK_KEYCODE_SECTION_GROUPS)};
    const keyPickerKeyboardSvgLayout = ${JSON.stringify(KEY_PICKER_KEYBOARD_SVG_LAYOUT)};
    const modWrapperLabels = ${JSON.stringify(MOD_WRAPPER_LABELS)};
    const keyBehaviorAllGroups = ${JSON.stringify(KEY_FEEDBACK_GROUP_ALL)};
    const rgbLocalities = ${JSON.stringify(RGB_LOCALITIES)};
    const automouseFadeModes = ${JSON.stringify(AUTOMOUSE_FADE_MODES)};
    const keyFeedbackTapCommitModes = ${JSON.stringify(KEY_FEEDBACK_TAP_COMMIT_MODES)};
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
        KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED: "Tap branch committed",
        KEY_FEEDBACK_GROUP_TAP_COMMITTED: "Tap committed",
        KEY_FEEDBACK_GROUP_HOLD_ACTIVE: "Hold active",
        KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: "Long hold active"
    };
    keyBehaviorRgbSemanticLabels[keyBehaviorAllGroups] = "All feedback groups";
    const keyPickerModifiers = ["Ctrl", "Shift", "Alt", "Cmd", "Right Ctrl", "Right Shift", "Right Alt", "Right Cmd"];
    const keyPickerLayerTapPrefix = "__LT_LAYER__:";
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
                ["_______", "XXXXXXX"],
                ["LEFT_THUMB", "RIGHT_THUMB", "CLICK_SPAM"]
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
    let activeTooltipTarget = undefined;

    document.getElementById("refresh").addEventListener("click", () => post({ type: "refresh" }));
    document.getElementById("openKeymap").addEventListener("click", () => post({ type: "openSource", file: "keymap" }));
    document.getElementById("openRgb").addEventListener("click", () => post({ type: "openSource", file: "rgb" }));
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
    });
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
            model = event.data.model;
            Object.assign(qmkKeyLabels, model.qmkKeyLabels || {});
            notice = event.data.notice || "";
            if (!activeLayer && model.layers.length) {
                activeLayer = model.layers[0].name;
            }
            reconcilePendingLayoutEdits();
            normalizeLayoutComboState();
            normalizeRgbGroupState();
            render();
            resetLocalHistory();
        }
        if (event.data.type === "error") {
            notice = event.data.message || "Unknown error";
            render();
            resetLocalHistory();
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
        if (action === "selectLayer") {
            activeLayer = target.dataset.layer;
            selectedKey = 0;
            layoutComboPicking = false;
            layoutComboSelection = [];
            layoutComboOutput = "";
            layoutComboInputs = "";
            lastLayoutKeyClick = { index: undefined, time: 0 };
            render();
            resetLocalHistory();
        } else if (action === "selectView") {
            activeView = target.dataset.view;
            lastLayoutKeyClick = { index: undefined, time: 0 };
            render();
            resetLocalHistory();
        } else if (action === "selectKey") {
            const index = Number(target.dataset.index);
            const now = Date.now();
            const isDoubleClick = lastLayoutKeyClick.index === index && now - lastLayoutKeyClick.time < 450;
            selectedKey = index;
            lastLayoutKeyClick = { index, time: isDoubleClick ? 0 : now };
            render();
            if (isDoubleClick) {
                openKeyPicker("keycodeInput", "single");
            }
            resetLocalHistory();
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
            render();
            commitLocalHistory(before);
        } else if (action === "clearLayoutComboSelection") {
            const before = currentLocalSnapshot || serializeLocalState();
            captureLayoutComboBuilderInputs();
            layoutComboSelection = [];
            layoutComboInputs = "";
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
            render();
            commitLocalHistory(before);
        } else if (action === "openKeyPicker") {
            openKeyPicker(target.dataset.target, target.dataset.mode || "single");
        } else if (action === "applyKey") {
            const input = document.getElementById("keycodeInput");
            const before = currentLocalSnapshot || serializeLocalState();
            if (stageLayoutKey(selectedKey, input.value)) {
                render();
                commitLocalHistory(before);
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
                    tapCommittedColor: readColorControl(card, "tapCommittedColor"),
                    holdActiveColor: readColorControl(card, "holdActiveColor"),
                    longHoldActiveColor: readColorControl(card, "longHoldActiveColor"),
                    tapCommitMode: value(card, "tapCommitMode"),
                    locality: value(card, "locality")
                }
            });
        } else if (action === "addRgbLedGroup") {
            const form = document.getElementById("rgbGroupBuilder");
            post({
                type: "addRgbLedGroup",
                group: readRgbLedGroupBuilder(form)
            });
        } else if (action === "updateViaMacro") {
            const row = target.closest("tr");
            post({
                type: "updateViaMacro",
                keycode: row.dataset.keycode,
                payload: row.querySelector("input").value
            });
        } else if (action === "addCombo") {
            post({ type: "addCombo", ...readComboBuilder(target) });
        } else if (action === "addLayoutCombo") {
            const payload = readComboBuilder(target);
            layoutComboPicking = false;
            layoutComboSelection = [];
            layoutComboOutput = "";
            layoutComboInputs = "";
            post({ type: "addCombo", ...payload });
        } else if (action === "applyLayoutChanges") {
            const changes = pendingLayoutChanges(activeLayer);
            if (changes.length) {
                post({ type: "updateLayoutKeys", layer: activeLayer, changes });
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
        if (event.target?.name === "owner" && event.target.closest("#rgbGroupBuilder")) {
            rgbGroupOwner = event.target.value;
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
            return;
        }
        const valid = new Set(layer.positions.map((position) => position.layoutIndex));
        const seen = new Set();
        layoutComboSelection = layoutComboSelection.filter((index) => {
            if (!Number.isInteger(index) || !valid.has(index) || seen.has(index)) return false;
            seen.add(index);
            return true;
        });
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

    function syncLayoutComboInputsFromSelection() {
        const layer = currentLayer();
        if (!layer) {
            layoutComboInputs = "";
            return;
        }
        layoutComboInputs = layoutComboSelectedPositions(layer).map((position) => position.keycode).join(", ");
    }

    function syncLayoutComboSelectionFromInputs() {
        const layer = currentLayer();
        if (!layer) return;
        const unusedPositions = layer.positions.slice();
        layoutComboSelection = layoutComboInputs.split(",")
            .map((part) => part.trim())
            .filter(Boolean)
            .map((keycode) => {
                const index = unusedPositions.findIndex((position) => position.keycode === keycode);
                if (index === -1) return undefined;
                const [position] = unusedPositions.splice(index, 1);
                return position.layoutIndex;
            })
            .filter((index) => Number.isInteger(index));
    }

    function currentLayoutPosition(layoutIndex = selectedKey) {
        const layer = currentLayer();
        if (!layer) return undefined;
        return layer.positions.find((position) => position.layoutIndex === layoutIndex) || layer.positions[layoutIndex];
    }

    function baseLayer(layerName = activeLayer) {
        return model.layers.find((layer) => layer.name === layerName) || model.layers[0];
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

    function stageLayoutKey(layoutIndex, keycode, layerName = activeLayer) {
        const base = baseLayer(layerName);
        const original = base?.positions.find((position) => position.layoutIndex === layoutIndex);
        const value = canonicalLayoutKeyExpression(keycode);
        if (!base || !original || !value) return false;
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
                if (original && keycode && !layoutKeyEquivalent(keycode, original.keycode)) {
                    layerEdits[index] = canonicalLayoutKeyExpression(keycode);
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
        if (!stageLayoutKey(position.layoutIndex, value)) return false;
        render();
        return true;
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
        }
        event.preventDefault();
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
        render();
        commitLocalHistory(before);
    }

    function cancelLayoutKeyDrag() {
        cleanupLayoutKeyDrag();
    }

    function cleanupLayoutKeyDrag() {
        document.body.classList.remove("layout-key-dragging");
        for (const key of document.querySelectorAll(".layout-board-svg .svg-key.drag-source, .layout-board-svg .svg-key.drag-target")) {
            key.classList.remove("drag-source", "drag-target");
        }
        layoutDragState = undefined;
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

    function layoutKeyElement(layoutIndex) {
        return document.querySelector(".layout-board-svg .svg-key[data-index='" + String(layoutIndex) + "']");
    }

    function normalizeRgbGroupState() {
        if (!["layer", "pdMode", "combo", "keyBehavior"].includes(rgbGroupTarget)) {
            rgbGroupTarget = "layer";
        }
        const owners = rgbGroupOwners(rgbGroupTarget);
        if (!owners.length) {
            rgbGroupOwner = "";
        } else if (!owners.includes(rgbGroupOwner)) {
            rgbGroupOwner = rgbGroupTarget === "layer" ? activeLayer || owners[0] : owners[0];
        }
    }

    function post(message) {
        notice = "Working...";
        render();
        resetLocalHistory();
        vscode.postMessage(message);
    }

    function render() {
        if (!model) {
            app.innerHTML = "<section>Loading...</section>";
            hydrateTooltips();
            return;
        }

        subtitle.textContent = model.root;
        app.innerHTML = renderDiagnostics() + renderViewTabs() + renderActiveView();
        initializeDirtyTracking();
        hydrateTooltips();
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
            rgbGroupTarget,
            rgbGroupOwner,
            rgbSelectedLeds,
            rgbBuilderColor,
            layoutComboPicking,
            layoutComboSelection,
            layoutComboOutput,
            layoutComboInputs,
            pendingLayoutEdits,
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
            rgbGroupTarget = state.rgbGroupTarget || rgbGroupTarget;
            rgbGroupOwner = state.rgbGroupOwner || "";
            rgbSelectedLeds = Array.isArray(state.rgbSelectedLeds) ? state.rgbSelectedLeds : [];
            rgbBuilderColor = state.rgbBuilderColor || undefined;
            layoutComboPicking = Boolean(state.layoutComboPicking);
            layoutComboSelection = Array.isArray(state.layoutComboSelection) ? state.layoutComboSelection : [];
            layoutComboOutput = state.layoutComboOutput || "";
            layoutComboInputs = state.layoutComboInputs || "";
            pendingLayoutEdits = state.pendingLayoutEdits && typeof state.pendingLayoutEdits === "object" ? state.pendingLayoutEdits : {};
            normalizeLayoutComboState();
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

    function initializeDirtyTracking() {
        for (const section of document.querySelectorAll("[data-dirty-section]")) {
            section.dataset.dirtyBaseline = dirtySnapshot(section);
            validateSection(section, false);
            updateDirtySection(section);
        }
    }

    function updateDirtyFromEvent(event) {
        const section = event.target?.closest?.("[data-dirty-section]");
        if (section) {
            updateDirtySection(section);
        }
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
        return section.id === "rgbGroupBuilder" && rgbSelectedLeds.length > 0;
    }

    function setDirtyButtonState(button, dirty) {
        const cleanLabel = button.dataset.cleanLabel || button.textContent.trim();
        button.dataset.cleanLabel = cleanLabel;
        button.textContent = dirty ? "Unsaved - " + cleanLabel : cleanLabel;
        button.classList.toggle("dirty", dirty);
        button.setAttribute("aria-label", dirty ? "Unsaved changes: " + cleanLabel : cleanLabel);
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
            firstInvalid.focus();
            firstInvalid.reportValidity?.();
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
        let error = "";
        if (rule === "uint8") {
            error = validateUint8(value, "Enter an integer from 0 to 255.");
        } else if (rule === "hsv-value") {
            error = validateHsvValue(value);
        } else if (rule === "optional-term") {
            error = validateOptionalTerm(value, "Enter a positive integer or KEY_BEHAVIOR_TERM(ms).");
        } else if (rule === "optional-branch-term") {
            error = validateOptionalTerm(value, "Enter a positive integer, or KEY_BEHAVIOR_TERM(ms).");
        } else if (rule === "positive-int") {
            error = validatePositiveInteger(value, "Enter a positive integer.");
        }
        setFieldError(control, error);
        return !error;
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

    function validateOptionalTerm(value, message) {
        if (!value) return "";
        if (/^\\d+$/.test(value)) return validatePositiveInteger(value, message);
        const wrapped = value.match(/^KEY_BEHAVIOR_TERM\\((\\d+)\\)$/);
        if (wrapped) return validatePositiveInteger(wrapped[1], message);
        return message;
    }

    function validatePositiveInteger(value, message) {
        if (!/^\\d+$/.test(value)) return message;
        return Number(value) > 0 ? "" : message;
    }

    function setFieldError(control, message) {
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
            return "Behavior actions for the " + title + ". Click to expand or collapse this tap branch.";
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
            return "Choose the helper that controls this behavior action.";
        }
        if (key.endsWith(" action")) {
            return "User-facing key or action for this behavior branch, for example Esc, Shift+\`, Cmd+Q, or a pointing-mode key.";
        }
        if (key.endsWith(" repeat hz")) {
            return "Repeat frequency used only when the helper is REPEAT_WHILE_HELD.";
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
        tooltip.textContent = text;
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

    function renderDiagnostics() {
        const items = [];
        if (notice) items.push("<div class='notice'>" + escapeHtml(notice) + "</div>");
        for (const diagnostic of model.diagnostics || []) {
            items.push("<div class='warning'>" + escapeHtml(diagnostic) + "</div>");
        }
        return items.length ? panel("Status", items.join(""), true) : "";
    }

    function panel(title, body, open = true) {
        return "<details class='panel' " + (open ? "open" : "") + "><summary><h2>" + escapeHtml(title) + "</h2></summary><div class='panel-body'>" + body + "</div></details>";
    }

    function renderViewTabs() {
        return "<div class='view-tabs'>" + views.map(([id, label]) =>
            "<button class='view-tab " + (activeView === id ? "active" : "") + "' data-action='selectView' data-view='" + escapeAttr(id) + "'>" + escapeHtml(label) + "</button>"
        ).join("") + "</div>";
    }

    function renderActiveView() {
        if (activeView === "macros") return "<div class='stack'>" + renderMacroStudio() + renderComboStudio() + "</div>";
        if (activeView === "rgb") return renderRgbStudio();
        return renderLayerStudio();
    }

    function renderLayerStudio() {
        const layer = currentLayer();
        if (!layer) return panel("Layers", "<p class='muted'>No LAYOUT blocks found.</p>", true);
        const selected = layer.positions[selectedKey] || layer.positions[0];
        const selectedBehavior = behaviorForKey(selected.keycode);
        return "<div class='stack'>" +
            panel("Layout", renderLayerTabs() + renderLayoutWithSelectedKeyEditor(layer, selected) + renderSelectedBehaviorEditor(selected, selectedBehavior), true) +
            panel("Layer Overview", renderLayerOverview(layer), true) +
            "</div>";
    }

    function renderLayerTabs() {
        return "<div class='tabs'>" + model.layers.map((layer) =>
            "<button class='tab " + (layer.name === activeLayer ? "active" : "") + "' data-action='selectLayer' data-layer='" + escapeAttr(layer.name) + "'>" + escapeHtml(layer.name) + "</button>"
        ).join("") + "</div>";
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
            renderKeyPickerInput("keycodeInput", "Key", selected.editLabel || selected.display || selected.keycode, "A, Enter, Space, _______", "single") +
            "<div><span class='muted'>Source</span><br><code class='source-pill'>" + escapeHtml(selected.keycode) + "</code></div>" +
            "<button data-action='applyKey' data-dirty-button class='primary'>Stage key</button>" +
            "</div>" +
            "</div>";
    }

    function renderLayoutComboBuilder(layer) {
        const selectedPositions = layoutComboSelectedPositions(layer);
        const inputValue = layoutComboInputs || selectedPositions.map((position) => position.keycode).join(", ");
        const selectedList = selectedPositions.length
            ? selectedPositions.map((position) =>
                "<button type='button' data-action='toggleLayoutComboKey' data-index='" + position.layoutIndex + "'><span>" + escapeHtml(position.display || position.keycode) + "</span><code>" + escapeHtml(position.keycode) + "</code></button>"
            ).join("")
            : "<span class='muted'>No inputs selected</span>";
        return "<div id='layoutComboBuilder' class='card selected-key-edit-card layout-combo-builder-card' data-dirty-section data-combo-builder>" +
            "<h3>Create combo</h3>" +
            "<div class='selected-key-edit-fields'>" +
            renderKeyPickerInput("layoutComboOutput", "Output", layoutComboOutput, "Tab", "single", "", "data-combo-output") +
            renderKeyPickerInput("layoutComboInputs", "Inputs", inputValue, "D, F", "list", "", "data-combo-inputs") +
            "<div class='layout-combo-selected-list'>" + selectedList + "</div>" +
            "<div class='layout-combo-actions'>" +
            "<button type='button' class='" + (layoutComboPicking ? "active" : "") + "' data-action='toggleLayoutComboPicking'>" + (layoutComboPicking ? "Selecting inputs" : "Select inputs") + "</button>" +
            "<button type='button' data-action='clearLayoutComboSelection'>Clear</button>" +
            "</div>" +
            "<button data-action='addLayoutCombo' data-dirty-button class='primary'>Append combo row</button>" +
            "</div>" +
            "</div>";
    }

    function renderSelectedBehaviorEditor(selected, behavior) {
        const row = behavior || {
            keycode: selected.keycode,
            tapHoldTerm: "",
            longerHoldTerm: "",
            multiTapTerm: "",
            branchConfirmTerm: "",
            steps: []
        };
        const steps = [];
        for (let index = 0; index < 5; index += 1) {
            steps.push(row.steps.find((step) => step.tapCount === index) || { tapCount: index, tapCountName: tapBranchName(index) });
        }
        return "<div class='card selected-behavior-editor' data-dirty-section><h3>Behavior on this key</h3>" +
            "<input type='hidden' id='selectedBehaviorKeycode' value='" + escapeAttr(row.keycode) + "'>" +
            "<div><span class='muted'>Source</span><br><code class='source-pill'>" + escapeHtml(row.keycode) + "</code></div>" +
            "<div class='form-grid four'>" +
            renderTimingInput("selectedTapHoldTerm", "tap_hold_term", row.tapHoldTerm || "", row.keycode) +
            renderTimingInput("selectedLongerHoldTerm", "longer_hold_term", row.longerHoldTerm || "", row.keycode) +
            renderTimingInput("selectedMultiTapTerm", "multi_tap_term", row.multiTapTerm || "", row.keycode) +
            renderTimingInput("selectedBranchConfirmTerm", "branch_confirm_term", row.branchConfirmTerm || "", row.keycode) +
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
        const validation = field === "branch_confirm_term" ? "optional-branch-term" : "optional-term";
        return "<label data-tooltip='" + escapeAttr(tooltip) + "'><span>" + field + "</span><input id='" + id + "' data-validate='" + validation + "' inputmode='numeric' value='" + escapeAttr(value || "") + "' placeholder='" + escapeAttr(placeholder) + "' data-tooltip='" + escapeAttr(tooltip) + "'></label>";
    }

    function behaviorTimingDefault(field, keycode) {
        const defaults = model.behaviorTimingDefaults || {};
        const useLtTapTerm = field === "tap_hold_term" && /^LT\\(/.test(normalizeDisplayExpression(keycode));
        const expression = {
            tap_hold_term: useLtTapTerm ? defaults.tappingTerm : defaults.tapHoldTerm,
            longer_hold_term: defaults.longerHoldTerm,
            multi_tap_term: defaults.multiTapTerm,
            branch_confirm_term: defaults.branchConfirmTerm
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
        const wrapped = text.match(/^KEY_BEHAVIOR_TERM\\((\\d+)\\)$/);
        if (wrapped) return wrapped[1] + " ms";
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
            renderKeyPickerInput(id + "Action", label + " action", editableActionValue(action), "Esc, Shift+\`, Cmd+Q", "single", "", "data-helper-action-prefix='" + escapeAttr(id) + "'" + actionHidden) +
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

    function renderKeyPickerInput(id, label, value, placeholder, mode = "single", style = "", attrs = "") {
        const styleAttr = style ? " style='" + escapeAttr(style) + "'" : "";
        const extraAttrs = attrs ? " " + attrs : "";
        const buttonLabel = mode === "list" ? "Pick keycodes" : "Pick keycode";
        return "<label" + styleAttr + extraAttrs + "><span>" + escapeHtml(label) + "</span><span class='input-with-button'>" +
            "<input id='" + escapeAttr(id) + "' value='" + escapeAttr(value || "") + "' placeholder='" + escapeAttr(placeholder || "") + "'>" +
            "<button type='button' data-action='openKeyPicker' data-target='" + escapeAttr(id) + "' data-mode='" + escapeAttr(mode) + "'>" + buttonLabel + "</button>" +
            "</span></label>";
    }

    function openKeyPicker(targetId, mode) {
        const input = document.getElementById(targetId);
        if (!input) return;
        keyPicker = {
            targetId,
            mode: mode === "list" ? "list" : "single",
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
                    rows: model.layers.map((layer) => {
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
                    rows: [
                        (model.viaMacros || []).map((slot) => slot.keycode),
                        (model.hardcodedMacros || []).map((slot) => slot.keycode)
                    ]
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
            value: entry.value,
            label: entry.label || entry.value,
            tooltip: (entry.value || "") + (entry.key && entry.key !== entry.value ? " / " + entry.key : "") + ((entry.aliases || []).length ? " / " + entry.aliases.join(", ") : "")
        }));
        for (let index = 0; index < items.length; index += columns) {
            rows.push(items.slice(index, index + columns));
        }
        return rows;
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
        const input = document.getElementById(keyPicker.targetId);
        if (input) {
            input.value = keyPickerExpression();
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
        return "<div class='board layout-board-card'>" +
            "<div class='layout-board-header'>" +
            "<h3 class='layout-board-title'>" + escapeHtml(layer.name) + "</h3>" +
            "<p class='layout-board-subtitle'>" + escapeHtml(layerColorSubtitle(layerColor)) + "</p>" +
            "</div>" +
            "<div class='layout-board-stage'>" +
            "<svg class='keyboard-svg layout-board-svg' viewBox='" + viewBox.x + " " + viewBox.y + " " + viewBox.width + " " + viewBox.height + "' preserveAspectRatio='xMidYMid meet' role='img' aria-label='" + escapeAttr(layer.name + " keyboard layout") + "'>" +
            layer.positions.map(renderSvgKey).join("") +
            "</svg>" +
            "</div>" +
            renderLayoutBoardApplyButton(layer) +
            "</div>";
    }

    function renderLayoutBoardApplyButton(layer) {
        const changes = pendingLayoutChanges(layer.name);
        if (!changes.length) return "";
        const label = "Apply " + changes.length + " layout " + (changes.length === 1 ? "change" : "changes");
        return "<div class='layout-board-apply'>" +
            "<button type='button' data-action='applyLayoutChanges' class='primary'>" + escapeHtml(label) + "</button>" +
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
        const cx = visual.x + keyboardGeometry.keyWidth / 2;
        const cy = visual.y + keyboardGeometry.keyHeight / 2;
        const transform = visual.angle ? " transform='rotate(" + visual.angle + " " + cx + " " + cy + ")'" : "";
        const tooltipText = layoutComboPicking
            ? "Toggle combo input " + label + " (" + position.keycode + ")"
            : "Click to edit layout index " + position.layoutIndex + ": " + label + " (" + position.keycode + "). Double-click to pick a keycode, drag onto another key to swap, or copy/paste selected keys.";
        const action = layoutComboPicking ? "toggleLayoutComboKey" : "selectKey";
        return "<g class='svg-key " + (selected ? "selected" : "") + (comboSelected ? " combo-input-selected" : "") + (position.pending ? " pending" : "") + "' tabindex='0' role='button' data-action='" + action + "' data-index='" + position.layoutIndex + "' data-keycode='" + escapeAttr(position.keycode) + "' data-tooltip='" + escapeAttr(tooltipText) + "'" + transform + ">" +
            "<rect x='" + visual.x + "' y='" + visual.y + "' width='" + keyboardGeometry.keyWidth + "' height='" + keyboardGeometry.keyHeight + "' rx='" + keyboardGeometry.radius + "' fill='" + style.fill + "' stroke='" + style.stroke + "'></rect>" +
            renderSvgLabel(label, cx, cy, style.text) +
            renderBehaviorDots(dots, visual, style.text) +
            renderComboBadges(badges, visual) +
            "</g>";
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

    function renderSvgLabel(label, cx, cy, textColor) {
        const lines = fitLabelLines(label);
        const fontSize = lines.some((line) => line.length > 9) ? 9 : lines.length > 1 ? 10 : 12;
        const startY = cy - ((lines.length - 1) * fontSize * 0.58);
        return "<text font-size='" + fontSize + "' fill='" + escapeAttr(textColor || "#e7ecef") + "'>" + lines.map((line, index) =>
            "<tspan x='" + cx + "' y='" + (startY + index * fontSize * 1.18) + "'>" + escapeHtml(line) + "</tspan>"
        ).join("") + "</text>";
    }

    function fitLabelLines(label) {
        const clean = String(label || "").replace(/\\s+/g, " ").trim();
        if (clean.length <= 9) return [clean];
        if (clean.includes(" / ")) {
            return clean.split(" / ").slice(0, 2).map((part, index) => index === 0 ? part : "/ " + part);
        }
        const words = clean.split(" ");
        if (words.length > 1) {
            const midpoint = Math.ceil(words.length / 2);
            return [words.slice(0, midpoint).join(" "), words.slice(midpoint).join(" ")].filter(Boolean);
        }
        return [clean.slice(0, 9), clean.slice(9, 18)].filter(Boolean);
    }

    function colorForLayer(layerName) {
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

    function renderBehaviorDots(dots, visual, textColor) {
        if (!dots.length) return "";
        const startX = visual.x + keyboardGeometry.keyWidth - 10 - ((dots.length - 1) * 12);
        return dots.map((dot, index) => {
            const x = startX + index * 12;
            const y = visual.y + 10;
            const label = dot.count > 1 ? String(dot.count) : "";
            return "<circle cx='" + x + "' cy='" + y + "' r='5.5' fill='" + dot.color + "' stroke='" + escapeAttr(textColor || "#fff") + "' stroke-width='1.4'></circle>" +
                (label ? "<text x='" + x + "' y='" + (y + 1) + "' fill='" + idealText(dot.color) + "' font-size='7' text-anchor='middle' dominant-baseline='central'>" + label + "</text>" : "");
        }).join("");
    }

    function comboBadgesForKey(keycode) {
        return layerCombos(currentLayer()).filter((combo) => combo.inputs.includes(keycode)).map((combo) => combo.badge);
    }

    function renderComboBadges(badges, visual) {
        if (!badges.length) return "";
        const badgeHeight = 12;
        const widths = badges.map((badge) => Math.max(15, 7 + badge.length * 4));
        const total = widths.reduce((sum, width) => sum + width, 0) + (badges.length - 1) * 3;
        let x = visual.x + (keyboardGeometry.keyWidth - total) / 2;
        const y = visual.y + keyboardGeometry.keyHeight - badgeHeight - 5;
        return badges.map((badge, index) => {
            const width = widths[index];
            const out = "<rect x='" + x + "' y='" + y + "' width='" + width + "' height='" + badgeHeight + "' rx='5' fill='#141714' fill-opacity='0.94' stroke='#f5f5f3'></rect>" +
                "<text x='" + (x + width / 2) + "' y='" + (y + 8.4) + "' fill='#f5f5f3' font-size='7.5' text-anchor='middle' font-weight='700'>" + escapeHtml(badge) + "</text>";
            x += width + 3;
            return out;
        }).join("");
    }

    function behaviorForKey(keycode) {
        return model.keyBehaviors.find((behavior) => behavior.keycode === keycode);
    }

    function layoutComboSelectedPositions(layer) {
        normalizeLayoutComboState();
        const byIndex = new Map(layer.positions.map((position) => [position.layoutIndex, position]));
        return layoutComboSelection.map((index) => byIndex.get(index)).filter(Boolean);
    }

    function layerBehaviorRows(layer) {
        const seen = new Set();
        const rows = [];
        for (const position of layer.positions) {
            if (seen.has(position.keycode)) continue;
            const behavior = behaviorForKey(position.keycode);
            if (!behavior) continue;
            seen.add(position.keycode);
            rows.push({
                behavior,
                positions: layer.positions.filter((candidate) => candidate.keycode === position.keycode)
            });
        }
        return rows;
    }

    function renderLayerOverview(layer) {
        return "<div class='stack'>" +
            "<div><h3>Behaviors</h3>" + renderLayerBehaviorTable(layer) + "</div>" +
            "<div>" + renderLayerMacroTable(layer) + "</div>" +
            "<div>" + renderLayerComboTable(layer) + "</div>" +
            "<div>" + renderLayerPdModeTable(layer) + "</div>" +
            "</div>";
    }

    function renderLayerBehaviorTable(layer) {
        const rows = layerBehaviorRows(layer);
        if (!rows.length) {
            return "<p class='muted'>No authored behavior rows are active on this layer.</p>";
        }
        return "<table><thead><tr><th>Key on layer</th><th>Behavior</th><th>Steps</th></tr></thead><tbody>" +
            rows.map((row) =>
                "<tr><td>" + row.positions.map((position) => "<button data-action='selectKey' data-index='" + position.layoutIndex + "'>" + escapeHtml(position.display) + "</button>").join(" ") +
                "</td><td>" + escapeHtml(displayAction(row.behavior.keycode)) + "<br><code class='muted'>" + escapeHtml(row.behavior.keycode) + "</code></td><td>" +
                row.behavior.steps.map(renderStep).join("<br>") + "</td></tr>"
            ).join("") +
            "</tbody></table>";
    }

    function layerCombos(layer) {
        const keycodes = new Set(layer.positions.map((position) => position.keycode));
        const rows = [];
        for (const combo of model.combos) {
            if (combo.inputs.every((input) => keycodes.has(input))) {
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
        return "<h3>Combos</h3><table><thead><tr><th>Badge</th><th>Inputs</th><th>Output</th><th>Output behavior</th></tr></thead><tbody>" +
            combos.map(renderLayerComboRow).join("") +
            "</tbody></table>";
    }

    function renderLayerComboRow(combo) {
        const behavior = behaviorForKey(combo.output);
        return "<tr><td><code>" + combo.badge + "</code></td>" +
            "<td>" + escapeHtml((combo.inputDisplays || combo.inputs).join(" + ")) + "</td>" +
            "<td>" + escapeHtml(combo.outputDisplay || combo.output) + "<br><code class='muted'>" + escapeHtml(combo.output) + "</code></td>" +
            "<td>" + renderComboOutputBehavior(behavior) + "</td></tr>";
    }

    function renderComboOutputBehavior(behavior) {
        if (!behavior) {
            return "<span class='muted'>No key behavior row for this output.</span>";
        }
        return behavior.steps.map(renderStep).join("<br>");
    }

    function renderLayerMacroTable(layer) {
        const rows = collectLayerMacros(layer);
        if (!rows.length) {
            return "<h3>Macros</h3><p class='muted'>No macro keycodes are directly placed or reached by visible behavior actions on this layer.</p>";
        }
        return "<h3>Macros</h3><table><thead><tr><th>Reachable via</th><th>Macro</th><th>Payload</th></tr></thead><tbody>" +
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
        return "<h3>PD Modes</h3><table><thead><tr><th>Reachable via</th><th>Mode</th><th>RGB</th></tr></thead><tbody>" +
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
            "<h3>Append simple single tap branch row</h3>" +
            "<div class='form-grid'>" +
            "<label><span>Key</span><input id='behaviorKeycode' placeholder='A'></label>" +
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
            renderKeyPickerInput(prefix + "Action", label + " action", "", "Esc, Shift+\`, Cmd+Q", "single", "", "data-helper-action-prefix='" + escapeAttr(prefix) + "' hidden") +
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
            branchConfirmTerm: document.getElementById("selectedBranchConfirmTerm").value,
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
        const allKeyBehavior = target === "keyBehavior" && owner === keyBehaviorAllGroups;
        return {
            target,
            owner,
            hue: allKeyBehavior ? "0" : value(form, "h"),
            sat: allKeyBehavior ? "0" : value(form, "s"),
            val: allKeyBehavior ? "0" : value(form, "v"),
            ledIndices: rgbSelectedLeds
        };
    }

    function renderRgbStudio() {
        const rgb = model.rgb || {};
        return "<div class='stack'>" +
            panel("RGB LED Group Builder", renderRgbGroupBuilder(), true) +
            panel("Layer Colors", renderLayerRgbSection(rgb), true) +
            panel("Auto-mouse Fade", renderAutomouseCard(rgb.automouseFade), false) +
            panel("Pointing-mode Colors", renderPdModeRgbSection(rgb), true) +
            panel("Combo Feedback", renderComboFeedbackSection(rgb), false) +
            panel("Key Behavior Feedback", renderKeyBehaviorFeedbackSection(rgb), true) +
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
        const ownerControl = ownerChoices.length
            ? "<label><span>" + escapeHtml(rgbGroupOwnerLabel(rgbGroupTarget)) + "</span><select name='owner'>" + optionsWithLabels(ownerChoices, rgbGroupOwner) + "</select></label>"
            : "<label><span>owner</span><input name='owner' disabled value='combo feedback'></label>";
        const selected = rgbSelectedLeds.length
            ? rgbSelectedLeds.map((led) => "<code>" + led + "</code>").join("")
            : "<span class='muted'>No LEDs selected</span>";
        const definedLedIndices = rgbBuilderDefinedLedIndices();
        const defined = definedLedIndices.length
            ? definedLedIndices.map((led) => "<code>" + led + "</code>").join("")
            : "<span class='muted'>No defined LEDs for this table</span>";
        return "<div id='rgbGroupBuilder' class='card' data-dirty-section>" +
            "<div class='form-grid four'>" +
            "<label><span>table</span><select name='target'>" + optionsWithLabels(targetOptions, rgbGroupTarget) + "</select></label>" +
            ownerControl +
            "<div><button data-action='addRgbLedGroup' data-dirty-button class='primary'>Add LED group row</button></div>" +
            "</div>" +
            renderRgbBuilderColorControl() +
            "<div class='toolbar' style='margin: 10px 0'>" +
            "<button data-action='clearRgbSelection'>Clear LEDs</button>" +
            "<button data-action='toggleRgbTrackball'>Trackball LED 56</button>" +
            "<div class='rgb-selected-list'><span class='rgb-led-list-label'>new row</span>" + selected + "</div>" +
            "<div class='rgb-selected-list rgb-defined-list'><span class='rgb-led-list-label'>defined</span>" + defined + "</div>" +
            "</div>" +
            renderLayerTabs() +
            renderRgbGroupBoard(layer) +
            "</div>";
    }

    function rgbGroupOwners(target) {
        if (target === "layer") return model.layers.map((layer) => layer.name);
        if (target === "pdMode") return (model.rgb?.pdModeColors || []).map((row) => row.pointingMode);
        if (target === "keyBehavior") return keyBehaviorRgbSemantics;
        return [];
    }

    function rgbGroupOwnerOptions(target) {
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
        const fill = hsvToHex(row.color) || rgbBuilderHex();
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
        if (rgbGroupTarget === "keyBehavior" && rgbGroupOwner === keyBehaviorAllGroups) {
            const rows = keyBehaviorRgbSemanticColorRows();
            return "<div class='color-control'>" +
                "<div class='muted'>All mode writes one low-level KEY_FEEDBACK_GROUP_ALL row. The firmware uses the active feedback semantic's configured color at render time.</div>" +
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
                    label: "Tap branch " + index + " committed",
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
        return hsvToHex(defaultRgbBuilderColor()) || "#000000";
    }

    function rgbBuilderUsesAllFeedbackPreview() {
        return rgbGroupTarget === "keyBehavior" && rgbGroupOwner === keyBehaviorAllGroups;
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
        return "<div class='board'>" +
            "<svg class='keyboard-svg' viewBox='0 0 " + keyboardGeometry.width + " " + keyboardGeometry.height + "' role='img' aria-label='RGB LED group selector'>" +
            renderRgbAllFeedbackGradientDefs() +
            "<text x='32' y='40' fill='#dbe6e8' font-size='24' font-weight='650'>LED group selector</text>" +
            "<text x='32' y='68' fill='#a8b2b8' font-size='13'>Physical LED indices - " + escapeHtml(layer.name) + "</text>" +
            layer.positions.map(renderRgbSvgKey).join("") +
            renderExtraLed(56, 698, 522) +
            "</svg>" +
            "</div>";
    }

    function renderRgbSvgKey(position) {
        const ledIndex = layoutToLedIndex[position.layoutIndex];
        const visual = keyVisual(position.layoutIndex);
        const selected = rgbSelectedLeds.includes(ledIndex);
        const definedPreview = rgbBuilderDefinedPreviewForLed(ledIndex);
        const style = keyStyle(position);
        const cx = visual.x + keyboardGeometry.keyWidth / 2;
        const cy = visual.y + keyboardGeometry.keyHeight / 2;
        const allPreview = selected ? rgbBuilderUsesAllFeedbackPreview() : Boolean(definedPreview?.allFeedback);
        const selectedFill = selected ? rgbBuilderPreviewFill() : definedPreview?.fill || style.fill;
        const selectedText = selected ? rgbBuilderPreviewText() : definedPreview?.text || style.text;
        const tooltipText = "Click to add or remove LED " + ledIndex + " for " + (position.display || position.keycode) + " (" + position.keycode + ")";
        const transform = visual.angle ? " transform='rotate(" + visual.angle + " " + cx + " " + cy + ")'" : "";
        return "<g class='svg-key " + (selected ? "rgb-selected" : "") + (definedPreview ? " rgb-defined" : "") + (allPreview ? " rgb-all-preview" : "") + "' data-action='toggleRgbLed' data-led='" + ledIndex + "' data-tooltip='" + escapeAttr(tooltipText) + "'" + transform + ">" +
            "<rect data-rgb-led-preview x='" + visual.x + "' y='" + visual.y + "' width='" + keyboardGeometry.keyWidth + "' height='" + keyboardGeometry.keyHeight + "' rx='" + keyboardGeometry.radius + "' fill='" + selectedFill + "' stroke='" + (selected ? "#ffffff" : style.stroke) + "'></rect>" +
            renderSvgLabel((position.display || position.keycode) + " " + ledIndex, cx, cy, selectedText) +
            "</g>";
    }

    function renderExtraLed(ledIndex, cx, cy) {
        const selected = rgbSelectedLeds.includes(ledIndex);
        const definedPreview = rgbBuilderDefinedPreviewForLed(ledIndex);
        const allPreview = selected ? rgbBuilderUsesAllFeedbackPreview() : Boolean(definedPreview?.allFeedback);
        const fill = selected ? rgbBuilderPreviewFill() : definedPreview?.fill || "#20262a";
        const textColor = selected ? rgbBuilderPreviewText() : definedPreview?.text || "#e7ecef";
        const tooltipText = "Click to add or remove trackball LED " + ledIndex + " from the group builder.";
        return "<g class='extra-led " + (selected ? "rgb-selected" : "") + (definedPreview ? " rgb-defined" : "") + (allPreview ? " rgb-all-preview" : "") + "' data-action='toggleRgbTrackball' data-tooltip='" + escapeAttr(tooltipText) + "'>" +
            "<circle data-rgb-led-preview cx='" + cx + "' cy='" + cy + "' r='13' fill='" + fill + "' stroke='" + (selected ? "#ffffff" : "#31c6a4") + "'></circle>" +
            "<text x='" + cx + "' y='" + (cy + 1) + "' fill='" + textColor + "' font-size='10' text-anchor='middle' dominant-baseline='middle'>" + ledIndex + "</text>" +
            "</g>";
    }

    function renderLayerRgbSection(rgb) {
        return "<div class='card-list'>" +
            (rgb.layerColors || []).map(renderLayerColorCard).join("") +
            renderLedGroupSubsection("Layer LED Groups", rgb.layerLedGroups || [], "Layer") +
            "</div>";
    }

    function renderPdModeRgbSection(rgb) {
        return "<div class='card-list'>" +
            (rgb.pdModeColors || []).map(renderPdColorCard).join("") +
            renderLedGroupSubsection("Pointing-mode LED Groups", rgb.pdModeLedGroups || [], "Pointing mode") +
            "</div>";
    }

    function renderComboFeedbackSection(rgb) {
        return "<div class='card-list'>" +
            renderComboFeedbackCard(rgb.comboFeedback) +
            renderLedGroupSubsection("Combo Feedback LED Groups", rgb.comboFeedbackLedGroups || [], "") +
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
            renderRgbConfigSummary(row.layer, row.color, row.mode, layerRgbSummaryOptions(row)) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(row.color, "", "", { pickerColor: layerPreviewColor(row) || row.color }) +
            layerPassthroughNote(row) +
            "<div class='form-grid four'>" +
            "<label><span>mode</span><select name='mode'>" + options(["ALL_KEYS", "KEYS_MAPPED_ON_THIS_LAYER_ONLY"], row.mode) + "</select></label>" +
            "<button data-action='updateLayerColor' data-dirty-button class='primary'>Apply</button>" +
            "</div></div></details>";
    }

    function renderPdColorCard(row) {
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section data-mode='" + escapeAttr(row.pointingMode) + "'>" +
            renderRgbConfigSummary(row.pointingMode, row.color, row.locality) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(row.color) +
            "<div class='form-grid four'>" +
            "<label><span>locality</span><select name='locality'>" + options(rgbLocalities, row.locality) + "</select></label>" +
            "<button data-action='updatePdModeColor' data-dirty-button class='primary'>Apply</button>" +
            "</div></div></details>";
    }

    function renderAutomouseCard(config) {
        if (!config) {
            return "<p class='muted'>No active automouse fade config parsed.</p>";
        }
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section>" +
            renderRgbConfigSummary("Fade destination", config.end_color, config.mode) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(config.end_color) +
            "<div class='form-grid four'>" +
            "<label><span>mode</span><select name='mode'>" + options(automouseFadeModes, config.mode) + "</select></label>" +
            "<button data-action='updateAutomouseFade' data-dirty-button class='primary'>Apply</button>" +
            "</div>" +
            "</div></details>";
    }

    function renderComboFeedbackCard(config) {
        if (!config) {
            return "<p class='muted'>No active combo feedback config parsed.</p>";
        }
        return "<details class='card rgb-subsection collapsible-card' data-dirty-section>" +
            renderRgbConfigSummary("Active combo color", config.color, config.locality) +
            "<div class='rgb-subsection-body'>" +
            renderHsvColorControl(config.color) +
            "<div class='form-grid four'>" +
            "<label><span>locality</span><select name='locality'>" + options(rgbLocalities, config.locality) + "</select></label>" +
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
        const branchRows = (config.tapBranchColors || []).map((color, index) => ["Tap branch " + index, "tapBranchColor" + index, color]);
        return "<div id='keyBehaviorFeedbackCard' class='card-list' data-dirty-section>" +
            "<details class='card collapsible-card'>" +
            "<summary><h3>Policy</h3></summary>" +
            "<div class='rgb-subsection-body'>" +
            "<div class='form-grid four'>" +
            "<label><span>tap commit mode</span><select name='tapCommitMode'>" + options(keyFeedbackTapCommitModes, config.tapCommitMode) + "</select></label>" +
            "<label><span>locality</span><select name='locality'>" + options(rgbLocalities, config.locality) + "</select></label>" +
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
            renderRgbConfigSummary(label, color) +
            "<div class='rgb-subsection-body'>" + renderHsvColorControl(color, id, extraAttrs) + "</div>" +
            "</details>";
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
        return "<summary><span class='rgb-summary'>" +
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
        const ownerHeader = ownerLabel ? "<th>" + escapeHtml(ownerLabel) + "</th>" : "";
        return "<table><thead><tr>" + ownerHeader + "<th>Color</th><th>LED group</th><th>LEDs</th></tr></thead><tbody>" +
            rows.map((row) => "<tr>" +
                (ownerLabel ? "<td><code>" + escapeHtml(row.owner || "") + "</code></td>" : "") +
                renderLedGroupColorCell(row, tableKind) +
                "<td><code>" + escapeHtml(row.ledGroup || "") + "</code></td>" +
                "<td><code>" + escapeHtml((row.ledIndices || []).join(", ")) + "</code></td>" +
                "</tr>").join("") +
            "</tbody></table>";
    }

    function renderLedGroupColorCell(row, tableKind = "") {
        if (tableKind === "keyBehavior" && row.owner === keyBehaviorAllGroups) {
            return "<td><div class='toolbar'>" + keyBehaviorRgbSemanticColorRows().map((semanticRow) => renderInlineSwatch(semanticRow.color)).join("") + "</div><code class='muted'>runtime feedback color</code></td>";
        }
        return "<td>" + renderInlineSwatch(row.color) + "<code>" + escapeHtml(row.color?.expression || "") + "</code></td>";
    }

    function renderHsvColorControl(color, id, extraAttrs = "", options = {}) {
        const hex = hsvToHex(options.pickerColor || color) || "#000000";
        const idAttr = id ? " data-color-id='" + escapeAttr(id) + "'" : "";
        return "<div class='color-control' data-color-control" + idAttr + extraAttrs + ">" +
            "<div class='color-row'>" +
            "<label><span>picker</span><input type='color' data-color-picker value='" + hex + "'></label>" +
            hsvInputs(color) +
            "</div>" +
            "<code class='muted' data-color-expression>" + escapeHtml(colorExpression(color)) + "</code>" +
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
            expression.textContent = colorExpression(color);
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
        return panel("VIA Macros",
            "<table><thead><tr><th>Slot</th><th>Payload</th><th></th></tr></thead><tbody>" +
            model.viaMacros.map((slot) =>
                "<tr data-dirty-section data-keycode='" + escapeAttr(slot.keycode) + "'><td><code>" + escapeHtml(slot.keycode) + "</code></td><td><input value='" + escapeAttr(slot.payload) + "'></td><td><button data-action='updateViaMacro' data-dirty-button>Apply</button></td></tr>"
            ).join("") +
            "</tbody></table>",
            true
        );
    }

    function renderComboStudio() {
        return panel("Combo Builder",
            "<div class='card' data-dirty-section data-combo-builder><div class='form-grid'>" +
            renderKeyPickerInput("comboOutput", "Output", "", "Tab", "single", "", "data-combo-output") +
            renderKeyPickerInput("comboInputs", "Inputs", "", "D, F", "list", "", "data-combo-inputs") +
            "<button data-action='addCombo' data-dirty-button class='primary'>Append combo row</button>" +
            "</div></div>" +
            "<h3 style='margin-top: 14px'>Existing combos</h3>" +
            "<table><thead><tr><th>Output</th><th>Inputs</th></tr></thead><tbody>" +
            model.combos.map((combo) =>
                "<tr><td>" + escapeHtml(combo.outputDisplay || combo.output) + "<br><code class='muted'>" + escapeHtml(combo.output) + "</code></td><td>" + escapeHtml((combo.inputDisplays || combo.inputs).join(" + ")) + "<br><code class='muted'>" + escapeHtml(combo.inputs.join(" + ")) + "</code></td></tr>"
            ).join("") +
            "</tbody></table>",
            true
        );
    }

    function currentLayer() {
        return layerWithPendingLayoutEdits(model.layers.find((layer) => layer.name === activeLayer) || model.layers[0]);
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
        if (qmkKeyLabels[normalized]) return qmkKeyLabels[normalized];

        let match = normalized.match(/^LT\\(LAYER_([^,]+),\\s*(.+)\\)$/);
        if (match) return displayKeyExpression(match[2]) + " / hold " + titleCase(match[1]);

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
