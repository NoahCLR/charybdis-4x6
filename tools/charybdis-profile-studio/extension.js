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

const LAYOUT_SLOT_COUNT = 56;
const TAP_COUNT_NAMES = ["single", "double", "triple", "quadruple", "quintuple"];
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
        case "updateLayoutKey":
            await patchLayoutKey(root, message.layer, Number(message.layoutIndex), message.keycode);
            await postModel(panel, root, "Updated keymap.c layer key.");
            return;
        case "updateLayerColor":
            await patchLayerColor(root, message.layer, message.hue, message.sat, message.val, message.mode);
            await postModel(panel, root, "Updated rgb_config.c layer color.");
            return;
        case "updatePdModeColor":
            await patchPdModeColor(root, message.pointingMode, message.hue, message.sat, message.val, message.locality);
            await postModel(panel, root, "Updated rgb_config.c pointing-mode color.");
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
    const [keymapText, rgbText] = await Promise.all([fs.readFile(keymapPath, "utf8"), fs.readFile(rgbPath, "utf8")]);

    const diagnostics = [];
    const safe = (label, fallback, callback) => {
        try {
            return callback();
        } catch (error) {
            diagnostics.push(`${label}: ${error instanceof Error ? error.message : String(error)}`);
            return fallback;
        }
    };

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
        rgb: safe("rgb", {}, () => parseRgbConfig(rgbText)),
        diagnostics,
    };
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

function parseRgbConfig(text) {
    return {
        layerColors: parseLayerColors(text),
        pdModeColors: parsePdModeColors(text),
        comboFeedback: parseSimpleColorStruct(text, /combo_feedback_colors\s*=/, [".color", ".locality"]),
        automouseFade: parseSimpleColorStruct(text, /automouse_fade_end_config\s*=/, [".mode", ".end_color"]),
        keyBehaviorFeedback: parseKeyBehaviorFeedback(text),
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
    const branchMatch = initializer.body.match(/RGB_TAP_BRANCH_COLORS\s*\(([\s\S]*?)\)\s*,/);
    const branchColors = branchMatch ? splitTopLevel(branchMatch[1]).map(parseHsv).filter((color) => color.expression) : [];
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

async function patchLayoutKey(root, layer, layoutIndex, keycode) {
    keycode = normalizeUserKeyExpression(keycode);
    assertSafeExpression(keycode, "keycode");
    if (!Number.isInteger(layoutIndex) || layoutIndex < 0 || layoutIndex >= LAYOUT_SLOT_COUNT) {
        throw new Error(`Invalid layout index: ${layoutIndex}`);
    }

    const filePath = path.join(root, KEYMAP_RELATIVE_PATH);
    const text = await fs.readFile(filePath, "utf8");
    const array = findInitializerBody(text, /keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=/);
    const layerCall = findLayerLayoutCall(array.body, layer);
    const argsBody = array.body.slice(layerCall.argsStart, layerCall.argsEnd);
    const items = splitTopLevelWithRanges(argsBody);
    if (items.length !== LAYOUT_SLOT_COUNT) {
        throw new Error(`${layer} expected ${LAYOUT_SLOT_COUNT} layout entries, got ${items.length}`);
    }

    const token = trimCodeRange(argsBody, items[layoutIndex].start, items[layoutIndex].end);
    const absoluteStart = array.bodyStart + layerCall.argsStart + token.start;
    const absoluteEnd = array.bodyStart + layerCall.argsStart + token.end;
    await writeText(filePath, replaceRange(text, absoluteStart, absoluteEnd, normalizeExpr(keycode)));
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

function parseDesignatedFields(body) {
    const fields = {};
    for (const item of splitTopLevel(body)) {
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

    if (/^[a-zA-Z]$/.test(raw)) {
        return `KC_${raw.toUpperCase()}`;
    }

    if (/^\d$/.test(raw)) {
        return `KC_${raw}`;
    }

    return compact;
}

function displayKeycode(keycode) {
    const normalized = normalizeExpr(keycode);
    return displayKeyExpression(normalized);
}

function editLabelForKeycode(keycode) {
    const normalized = normalizeExpr(keycode);
    const simple = QMK_KEY_LABELS[normalized];
    return simple || normalized;
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

    match = normalized.match(/^([GAS])\((.+)\)$/);
    if (match) {
        const modifier = { G: "Cmd", A: "Alt", S: "Shift" }[match[1]];
        return `${modifier}+${displayKeyExpression(match[2])}`;
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
    for (const [label, value] of [
        ["hue", hue],
        ["saturation", sat],
        ["value", val],
    ]) {
        const text = normalizeExpr(value);
        if (!/^[A-Z0-9_()+\-*/ ]+$/.test(text)) {
            throw new Error(`Invalid HSV ${label}: ${value}`);
        }
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
        button:hover { border-color: var(--accent); }
        input, select {
            min-height: 32px;
            padding: 5px 8px;
            width: 100%;
        }
        main {
            display: block;
            padding: 14px;
        }
        section {
            border: 1px solid var(--line);
            border-radius: 8px;
            background: var(--panel);
            padding: 14px;
            min-width: 0;
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
            padding: 4px 0 8px;
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
        .svg-key {
            cursor: pointer;
        }
        .svg-key rect {
            fill: var(--key);
            stroke: #64717a;
            stroke-width: 1.4;
        }
        .svg-key.selected rect {
            fill: var(--key-active);
            stroke: var(--accent);
            stroke-width: 3;
        }
        .svg-key text {
            fill: var(--text);
            font-family: var(--vscode-font-family, system-ui, sans-serif);
            text-anchor: middle;
            dominant-baseline: middle;
            pointer-events: none;
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
        .muted { color: var(--muted); }
        .notice { color: var(--accent); }
        .error { color: var(--danger); }
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
            gap: 4px;
            min-width: 0;
            color: var(--muted);
        }
        label span { font-size: 11px; }
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
        .swatch {
            width: 100%;
            height: 22px;
            border-radius: 5px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            margin: 4px 0 8px;
        }
        .inline-swatch {
            display: inline-block;
            width: 34px;
            height: 14px;
            border: 1px solid rgba(255, 255, 255, 0.28);
            border-radius: 4px;
            vertical-align: middle;
            margin-right: 6px;
        }
        .toolbar {
            display: flex;
            gap: 8px;
            align-items: center;
            flex-wrap: wrap;
        }
        @media (max-width: 980px) {
            .view-tab { min-width: 0; }
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
    let notice = "";
    const views = [
        ["layout", "Layout"],
        ["macros", "Macros & combos"],
        ["rgb", "RGB"]
    ];
    const keyboardGeometry = {
        width: 1120,
        height: 620,
        keyWidth: 58,
        keyHeight: 58,
        radius: 7,
        yOffset: 54,
        rowStep: 64,
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

    document.getElementById("refresh").addEventListener("click", () => post({ type: "refresh" }));
    document.getElementById("openKeymap").addEventListener("click", () => post({ type: "openSource", file: "keymap" }));
    document.getElementById("openRgb").addEventListener("click", () => post({ type: "openSource", file: "rgb" }));

    window.addEventListener("message", (event) => {
        if (event.data.type === "model") {
            model = event.data.model;
            notice = event.data.notice || "";
            if (!activeLayer && model.layers.length) {
                activeLayer = model.layers[0].name;
            }
            render();
        }
        if (event.data.type === "error") {
            notice = event.data.message || "Unknown error";
            render();
        }
    });

    app.addEventListener("click", (event) => {
        const target = event.target.closest("[data-action]");
        if (!target) return;
        const action = target.dataset.action;
        if (action === "selectLayer") {
            activeLayer = target.dataset.layer;
            selectedKey = 0;
            render();
        } else if (action === "selectView") {
            activeView = target.dataset.view;
            render();
        } else if (action === "selectKey") {
            selectedKey = Number(target.dataset.index);
            render();
        } else if (action === "applyKey") {
            const input = document.getElementById("keycodeInput");
            post({ type: "updateLayoutKey", layer: activeLayer, layoutIndex: selectedKey, keycode: input.value });
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
        } else if (action === "updateViaMacro") {
            const row = target.closest("tr");
            post({
                type: "updateViaMacro",
                keycode: row.dataset.keycode,
                payload: row.querySelector("input").value
            });
        } else if (action === "addCombo") {
            post({
                type: "addCombo",
                output: document.getElementById("comboOutput").value,
                inputs: document.getElementById("comboInputs").value
            });
        } else if (action === "addBehavior") {
            post({ type: "addBehavior", behavior: readBehaviorForm() });
        } else if (action === "saveSelectedBehavior") {
            post({ type: "saveBehavior", behavior: readSelectedBehaviorForm() });
        }
    });

    function value(root, name) {
        return root.querySelector("[name='" + name + "']").value;
    }

    function post(message) {
        notice = "Working...";
        render();
        vscode.postMessage(message);
    }

    function render() {
        if (!model) {
            app.innerHTML = "<section>Loading...</section>";
            return;
        }

        subtitle.textContent = model.root;
        app.innerHTML = renderDiagnostics() + renderViewTabs() + renderActiveView();
    }

    function renderDiagnostics() {
        const items = [];
        if (notice) items.push("<div class='notice'>" + escapeHtml(notice) + "</div>");
        for (const diagnostic of model.diagnostics || []) {
            items.push("<div class='warning'>" + escapeHtml(diagnostic) + "</div>");
        }
        return items.length ? "<section>" + items.join("") + "</section>" : "";
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
        if (!layer) return "<section><h2>Layers</h2><p class='muted'>No LAYOUT blocks found.</p></section>";
        const selected = layer.positions[selectedKey] || layer.positions[0];
        return "<div class='stack'>" +
            "<section>" +
            "<h2>Layout</h2>" +
            renderLayerTabs() +
            renderBoard(layer) +
            "</section>" +
            "<section>" +
            "<h2>Selected Key</h2>" +
            renderSelectedKeyPanel(layer, selected) +
            "</section>" +
            "<section>" +
            "<h2>Layer Behaviors</h2>" +
            renderLayerBehaviorTable(layer) +
            "</section>" +
            "<section>" +
            "<h2>Layer Combos & PD Modes</h2>" +
            renderLayerComboTable(layer) +
            renderLayerPdModeTable(layer) +
            "</section>" +
            "</div>";
    }

    function renderLayerTabs() {
        return "<div class='tabs'>" + model.layers.map((layer) =>
            "<button class='tab " + (layer.name === activeLayer ? "active" : "") + "' data-action='selectLayer' data-layer='" + escapeAttr(layer.name) + "'>" + escapeHtml(layer.name) + "</button>"
        ).join("") + "</div>";
    }

    function renderSelectedKeyPanel(layer, selected) {
        const behavior = behaviorForKey(selected.keycode);
        return "<div class='card'>" +
            "<h3>" + escapeHtml(selected.display || selected.keycode) + "</h3>" +
            "<div class='form-grid'>" +
            "<label><span>Layer</span><input disabled value='" + escapeAttr(layer.name) + "'></label>" +
            "<label><span>Layout index</span><input disabled value='" + selected.layoutIndex + "'></label>" +
            "<label style='grid-column: 1 / -1'><span>Key</span><input id='keycodeInput' value='" + escapeAttr(selected.editLabel || selected.display || selected.keycode) + "' placeholder='A, Enter, Space, _______'></label>" +
            "<div style='grid-column: 1 / -1'><span class='muted'>Source</span><br><code class='source-pill'>" + escapeHtml(selected.keycode) + "</code></div>" +
            "<button data-action='applyKey' class='primary'>Apply key</button>" +
            "</div>" +
            "<div style='height: 14px'></div>" +
            renderSelectedBehaviorEditor(selected, behavior) +
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
            steps.push(row.steps.find((step) => step.tapCount === index) || { tapCount: index, tapCountName: ["single", "double", "triple", "quadruple", "quintuple"][index] });
        }
        return "<h3>Behavior on this key</h3>" +
            "<input type='hidden' id='selectedBehaviorKeycode' value='" + escapeAttr(row.keycode) + "'>" +
            "<div class='form-grid four'>" +
            "<label><span>tap_hold_term</span><input id='selectedTapHoldTerm' value='" + escapeAttr(row.tapHoldTerm || "") + "' placeholder='default'></label>" +
            "<label><span>longer_hold_term</span><input id='selectedLongerHoldTerm' value='" + escapeAttr(row.longerHoldTerm || "") + "' placeholder='default'></label>" +
            "<label><span>multi_tap_term</span><input id='selectedMultiTapTerm' value='" + escapeAttr(row.multiTapTerm || "") + "' placeholder='default'></label>" +
            "<label><span>branch_confirm_term</span><input id='selectedBranchConfirmTerm' value='" + escapeAttr(row.branchConfirmTerm || "") + "' placeholder='default'></label>" +
            "</div>" +
            "<div class='card-list' style='margin-top: 10px'>" +
            steps.map(renderBehaviorStepEditor).join("") +
            "</div>" +
            "<button data-action='saveSelectedBehavior' class='primary' style='margin-top: 10px'>Save behavior row</button>";
    }

    function renderBehaviorStepEditor(step) {
        return "<div class='card'>" +
            "<h3>" + escapeHtml(step.tapCountName || ("tap " + (step.tapCount + 1))) + "</h3>" +
            "<input type='hidden' data-behavior-step='" + step.tapCount + "' value='" + step.tapCount + "'>" +
            "<div class='form-grid three'>" +
            renderActionEditor("step" + step.tapCount + "Tap", "Tap", step.tap, ["", "TAP_SENDS"]) +
            renderActionEditor("step" + step.tapCount + "Hold", "Hold", step.hold, ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            renderActionEditor("step" + step.tapCount + "LongHold", "Long hold", step.longHold, ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            "</div></div>";
    }

    function renderActionEditor(id, label, action, helpers) {
        return "<div class='stack'>" +
            "<label><span>" + label + " helper</span><select id='" + id + "Helper'>" + options(helpers, action?.helper || "") + "</select></label>" +
            "<label><span>" + label + " action</span><input id='" + id + "Action' value='" + escapeAttr(editableActionValue(action)) + "' placeholder='Esc'></label>" +
            "<label><span>" + label + " repeat Hz</span><input id='" + id + "Repeat' value='" + escapeAttr(action?.repeatHz || "") + "' placeholder='100'></label>" +
            "</div>";
    }

    function editableActionValue(action) {
        if (!action) return "";
        const display = displayAction(action.action);
        return display === action.action ? action.action : display;
    }

    function renderBoard(layer) {
        const layerColor = colorForLayer(layer.name);
        return "<div class='board'>" +
            "<svg class='keyboard-svg' viewBox='0 0 " + keyboardGeometry.width + " " + keyboardGeometry.height + "' role='img' aria-label='" + escapeAttr(layer.name + " keyboard layout") + "'>" +
            "<text x='32' y='40' fill='#dbe6e8' font-size='24' font-weight='650'>" + escapeHtml(layer.name) + "</text>" +
            "<text x='32' y='68' fill='#a8b2b8' font-size='13'>" + escapeHtml(layerColorSubtitle(layerColor)) + "</text>" +
            layer.positions.map(renderSvgKey).join("") +
            "</svg>" +
            "</div>";
    }

    function renderSvgKey(position) {
        const visual = keyVisual(position.layoutIndex);
        const label = position.display || position.keycode;
        const selected = selectedKey === position.layoutIndex;
        const style = keyStyle(position);
        const dots = behaviorDotsForKey(position.keycode);
        const badges = comboBadgesForKey(position.keycode);
        const cx = visual.x + keyboardGeometry.keyWidth / 2;
        const cy = visual.y + keyboardGeometry.keyHeight / 2;
        const transform = visual.angle ? " transform='rotate(" + visual.angle + " " + cx + " " + cy + ")'" : "";
        return "<g class='svg-key " + (selected ? "selected" : "") + "' data-action='selectKey' data-index='" + position.layoutIndex + "'" + transform + ">" +
            "<title>" + escapeHtml(position.keycode) + "</title>" +
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

    function layerColorSubtitle(layerColor) {
        if (!layerColor) return "No layer RGB config parsed";
        const color = layerColor.color || {};
        return "RGB matrix " + layerColor.mode + " • authored HSV(" + [color.h, color.s, color.v].join(", ") + ")";
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
        const css = hsvToHex(layerColor.color);
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
        return "<h3>Combos</h3><table><thead><tr><th>Badge</th><th>Inputs</th><th>Output</th></tr></thead><tbody>" +
            combos.map((combo) => "<tr><td><code>" + combo.badge + "</code></td><td>" + escapeHtml((combo.inputDisplays || combo.inputs).join(" + ")) + "</td><td>" + escapeHtml(combo.outputDisplay || combo.output) + "</td></tr>").join("") +
            "</tbody></table>";
    }

    function renderLayerPdModeTable(layer) {
        const rows = collectLayerPdModes(layer);
        if (!rows.length) {
            return "<h3 style='margin-top: 14px'>PD Modes</h3><p class='muted'>No pointing modes are directly placed or reached by visible behavior actions on this layer.</p>";
        }
        return "<h3 style='margin-top: 14px'>PD Modes</h3><table><thead><tr><th>Reachable via</th><th>Mode</th><th>RGB</th></tr></thead><tbody>" +
            rows.map((row) => "<tr><td>" + escapeHtml(row.source) + "</td><td>" + escapeHtml(row.mode) + "</td><td>" + renderInlineSwatch(row.color) + " <code class='muted'>" + escapeHtml(row.locality || "no override") + "</code></td></tr>").join("") +
            "</tbody></table>";
    }

    function collectLayerPdModes(layer) {
        const rows = [];
        const seen = new Set();
        for (const position of layer.positions) {
            addPdModeCandidate(rows, seen, position.keycode, position.display);
            const behavior = behaviorForKey(position.keycode);
            for (const step of behavior?.steps || []) {
                for (const action of [step.tap, step.hold, step.longHold]) {
                    if (action?.action) {
                        addPdModeCandidate(rows, seen, action.action, position.display + " via " + step.tapCountName + " " + action.helper);
                    }
                }
            }
        }
        return rows;
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
            mode,
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

    function colorForPdMode(mode) {
        return (model.rgb?.pdModeColors || []).find((row) => row.pointingMode === mode);
    }

    function renderBehaviorStudio() {
        return "<section>" +
            "<h2>Behavior Builder</h2>" +
            renderBehaviorForm() +
            "<h3 style='margin-top: 14px'>Existing rows</h3>" +
            "<table><thead><tr><th>Keycode</th><th>Steps</th></tr></thead><tbody>" +
            model.keyBehaviors.map((row) =>
                "<tr><td>" + escapeHtml(displayAction(row.keycode)) + "<br><code class='muted'>" + escapeHtml(row.keycode) + "</code></td><td>" + row.steps.map(renderStep).join("<br>") + "</td></tr>"
            ).join("") +
            "</tbody></table>" +
            "</section>";
    }

    function renderBehaviorForm() {
        return "<div class='card'>" +
            "<h3>Append simple single-tap row</h3>" +
            "<div class='form-grid'>" +
            "<label><span>Key</span><input id='behaviorKeycode' placeholder='A'></label>" +
            "<label><span>tap_hold_term</span><input id='behaviorTapHoldTerm' placeholder='150'></label>" +
            renderActionInputs("tap", "Tap", ["", "TAP_SENDS"]) +
            renderActionInputs("hold", "Hold", ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            renderActionInputs("longHold", "Long hold", ["", "PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "TAP_ON_RELEASE_AFTER_HOLD", "REPEAT_WHILE_HELD"]) +
            "<button data-action='addBehavior' class='primary'>Append behavior row</button>" +
            "</div></div>";
    }

    function renderActionInputs(prefix, label, helpers) {
        return "<label><span>" + label + " helper</span><select id='" + prefix + "Helper'>" +
            helpers.map((helper) => "<option value='" + escapeAttr(helper) + "'>" + escapeHtml(helper || "none") + "</option>").join("") +
            "</select></label>" +
            "<label><span>" + label + " action</span><input id='" + prefix + "Action' placeholder='Esc'></label>" +
            "<label><span>" + label + " repeat Hz</span><input id='" + prefix + "Repeat' placeholder='100'></label>";
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
        return {
            helper: document.getElementById(prefix + "Helper").value,
            action: document.getElementById(prefix + "Action").value,
            repeatHz: document.getElementById(prefix + "Repeat").value
        };
    }

    function renderRgbStudio() {
        const rgb = model.rgb || {};
        return "<section>" +
            "<h2>RGB Studio</h2>" +
            "<h3>Layer colors</h3>" +
            "<div class='card-list'>" + (rgb.layerColors || []).map(renderLayerColorCard).join("") + "</div>" +
            "<h3 style='margin-top: 14px'>Pointing-mode colors</h3>" +
            "<div class='card-list'>" + (rgb.pdModeColors || []).map(renderPdColorCard).join("") + "</div>" +
            "</section>";
    }

    function renderLayerColorCard(row) {
        return "<div class='card' data-layer='" + escapeAttr(row.layer) + "'>" +
            "<strong><code>" + escapeHtml(row.layer) + "</code></strong>" +
            renderSwatch(row.color) +
            "<div class='form-grid four'>" +
            hsvInputs(row.color) +
            "<label><span>mode</span><select name='mode'>" + options(["ALL_KEYS", "KEYS_MAPPED_ON_THIS_LAYER_ONLY"], row.mode) + "</select></label>" +
            "<button data-action='updateLayerColor' class='primary'>Apply</button>" +
            "</div></div>";
    }

    function renderPdColorCard(row) {
        return "<div class='card' data-mode='" + escapeAttr(row.pointingMode) + "'>" +
            "<strong><code>" + escapeHtml(row.pointingMode) + "</code></strong>" +
            renderSwatch(row.color) +
            "<div class='form-grid four'>" +
            hsvInputs(row.color) +
            "<label><span>locality</span><select name='locality'>" + options(["RGB_BOTH_HALVES", "RGB_LEFT_HALF", "RGB_RIGHT_HALF", "RGB_KEY_HALF", "RGB_KEYS_ONLY"], row.locality) + "</select></label>" +
            "<button data-action='updatePdModeColor' class='primary'>Apply</button>" +
            "</div></div>";
    }

    function hsvInputs(color) {
        return "<label><span>h</span><input name='h' value='" + escapeAttr(color.h || "") + "'></label>" +
            "<label><span>s</span><input name='s' value='" + escapeAttr(color.s || "") + "'></label>" +
            "<label><span>v</span><input name='v' value='" + escapeAttr(color.v || "") + "'></label>";
    }

    function renderSwatch(color) {
        const css = hsvToCss(color);
        return "<div class='swatch' style='background: " + css + "' title='" + escapeAttr(color.expression || "") + "'></div>";
    }

    function renderInlineSwatch(color) {
        const fill = hsvToHex(color);
        if (!fill) return "";
        return "<span class='inline-swatch' style='background: " + fill + "'></span>";
    }

    function hsvToCss(color) {
        return hsvToHex(color) || "#111";
    }

    function hsvToHex(color) {
        if (!color) return "";
        const h = Number(color.h);
        const s = Number(color.s);
        const v = numericChannel(color.v);
        if (!Number.isFinite(h) || !Number.isFinite(s) || !Number.isFinite(v) || v <= 0) return "";
        const rgb = hsvToRgb(h / 255, s / 255, v / 255);
        return rgbToHex(rgb.r, rgb.g, rgb.b);
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

    function renderMacroStudio() {
        return "<section><h2>VIA Macros</h2>" +
            "<table><thead><tr><th>Slot</th><th>Payload</th><th></th></tr></thead><tbody>" +
            model.viaMacros.map((slot) =>
                "<tr data-keycode='" + escapeAttr(slot.keycode) + "'><td><code>" + escapeHtml(slot.keycode) + "</code></td><td><input value='" + escapeAttr(slot.payload) + "'></td><td><button data-action='updateViaMacro'>Apply</button></td></tr>"
            ).join("") +
            "</tbody></table></section>";
    }

    function renderComboStudio() {
        return "<section><h2>Combo Builder</h2>" +
            "<div class='card'><div class='form-grid'>" +
            "<label><span>Output</span><input id='comboOutput' placeholder='Tab'></label>" +
            "<label><span>Inputs</span><input id='comboInputs' placeholder='D, F'></label>" +
            "<button data-action='addCombo' class='primary'>Append combo row</button>" +
            "</div></div>" +
            "<h3 style='margin-top: 14px'>Existing combos</h3>" +
            "<table><thead><tr><th>Output</th><th>Inputs</th></tr></thead><tbody>" +
            model.combos.map((combo) =>
                "<tr><td>" + escapeHtml(combo.outputDisplay || combo.output) + "<br><code class='muted'>" + escapeHtml(combo.output) + "</code></td><td>" + escapeHtml((combo.inputDisplays || combo.inputs).join(" + ")) + "<br><code class='muted'>" + escapeHtml(combo.inputs.join(" + ")) + "</code></td></tr>"
            ).join("") +
            "</tbody></table></section>";
    }

    function currentLayer() {
        return model.layers.find((layer) => layer.name === activeLayer) || model.layers[0];
    }

    function displayAction(value) {
        const text = String(value || "");
        if (!text) return "";
        if (!text.startsWith("KC_") && !text.includes("_") && !text.includes("(")) return text;
        const simple = {
            "_______": "_______",
            "XXXXXXX": "Disabled",
            "KC_ESC": "Esc",
            "KC_TAB": "Tab",
            "KC_ENT": "Enter",
            "KC_SPC": "Space",
            "KC_BSPC": "Backspace",
            "KC_DEL": "Delete",
            "KC_CAPS": "Caps Lock",
            "KC_LEFT": "Left",
            "KC_RIGHT": "Right",
            "KC_RGHT": "Right",
            "KC_UP": "Up",
            "KC_DOWN": "Down",
            "KC_LEFT_SHIFT": "Left Shift",
            "KC_RIGHT_ALT": "Right Alt",
            "KC_LEFT_GUI": "Left Cmd",
            "KC_MPLY": "Play",
            "KC_MNXT": "Next",
            "KC_MPRV": "Previous",
            "KC_MUTE": "Mute",
            "MS_BTN1": "Mouse 1",
            "MS_BTN2": "Mouse 2",
            "MS_BTN3": "Mouse 3"
        };
        for (let index = 0; index <= 9; index += 1) simple["KC_" + index] = String(index);
        for (let code = 65; code <= 90; code += 1) {
            const letter = String.fromCharCode(code);
            simple["KC_" + letter] = letter;
        }
        if (simple[text]) return simple[text];
        return text.replace(/^KC_/, "").replace(/_/g, " ").toLowerCase().replace(/\\b\\w/g, (char) => char.toUpperCase());
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
