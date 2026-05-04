#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

const extensionRoot = path.resolve(__dirname, "..");
const repoRoot = path.resolve(extensionRoot, "..", "..");
const sourcePath = path.join(extensionRoot, "extension.js");
const source = fs.readFileSync(sourcePath, "utf8");

const context = {
    console,
    process,
    Buffer,
    setTimeout,
    clearTimeout,
    __filename: sourcePath,
    __dirname: extensionRoot,
    module: {exports: {}},
    exports: {},
    require(name) {
        if (name === "vscode") {
            return {
                workspace: {workspaceFolders: []},
                window: {},
                commands: {registerCommand() { return {dispose() {}}; }},
                StatusBarAlignment: {Left: 1},
                ViewColumn: {Beside: 2},
                Uri: {file: (fsPath) => ({fsPath})},
            };
        }
        return require(name);
    },
};

const appendedCheck = `
(async () => {
    function assert(condition, message) {
        if (!condition) {
            throw new Error(message);
        }
    }

    const syntheticAliases = {
        FOO_CANON: "FOO_CANON",
        FOO_ALIAS: "FOO_CANON",
    };
    assert(
        canonicalKeyExpression("FOO_ALIAS", syntheticAliases) === "FOO_CANON",
        "canonicalKeyExpression should normalize direct aliases"
    );
    assert(
        canonicalKeyExpression("WRAP(FOO_ALIAS,BAR)", syntheticAliases) === "WRAP(FOO_CANON, BAR)",
        "canonicalKeyExpression should normalize nested aliases and expression spacing"
    );

    const model = await buildModel(${JSON.stringify(repoRoot)});
    const aliases = model.qmkKeycodeAliases || {};
    assert(
        (model.configDefaults || []).some((section) => (section.fields || []).some((field) => field.macro === "AUTO_MOUSE_TIME" && field.value === "1200")),
        "Profile Studio missed config.h defaults"
    );
    const reachable = new Map();
    const rawReachable = new Set();

    function addReachable(keycode, source) {
        rawReachable.add(keycode);
        const canonical = canonicalKeyExpression(keycode, aliases);
        if (!reachable.has(canonical)) {
            reachable.set(canonical, []);
        }
        reachable.get(canonical).push(source);
    }

    for (const layer of model.layers || []) {
        for (const position of layer.positions || []) {
            addReachable(position.keycode, layer.name + " index " + position.layoutIndex);
        }
    }
    for (const combo of model.combos || []) {
        addReachable(combo.output, "combo output " + combo.output);
    }

    const missing = [];
    const canonicalOnly = [];
    for (const behavior of model.keyBehaviors || []) {
        const canonical = canonicalKeyExpression(behavior.keycode, aliases);
        if (!reachable.has(canonical)) {
            missing.push(behavior.keycode);
        } else if (!rawReachable.has(behavior.keycode)) {
            canonicalOnly.push({keycode: behavior.keycode, matchedBy: reachable.get(canonical)});
        }
    }

    if (missing.length) {
        throw new Error("Profile Studio missed key_behaviors[] rows: " + missing.join(", "));
    }

    const comboLayerMisses = [];
    for (const combo of model.combos || []) {
        const canonicalInputs = combo.inputs.map((input) => canonicalKeyExpression(input, aliases));
        const found = (model.layers || []).some((layer) => {
            const keys = new Set((layer.positions || []).map((position) => canonicalKeyExpression(position.keycode, aliases)));
            return canonicalInputs.every((input) => keys.has(input));
        });
        if (!found) {
            comboLayerMisses.push(combo.output + " from " + combo.inputs.join(" + "));
        }
    }
    if (comboLayerMisses.length) {
        throw new Error("Profile Studio missed combo layer membership: " + comboLayerMisses.join(", "));
    }

    const editableCombo = (model.combos || []).find((combo) => combo.output !== "KC_ESC");
    assert(editableCombo, "Profile Studio combo edit check needs at least one non-KC_ESC combo");
    const nativeFs = require("fs");
    const tempRoot = nativeFs.mkdtempSync(path.join(require("os").tmpdir(), "profile-studio-combo-edit-"));
    try {
        const sourceKeymap = path.join(${JSON.stringify(repoRoot)}, KEYMAP_RELATIVE_PATH);
        const targetKeymap = path.join(tempRoot, KEYMAP_RELATIVE_PATH);
        nativeFs.mkdirSync(path.dirname(targetKeymap), {recursive: true});
        nativeFs.copyFileSync(sourceKeymap, targetKeymap);
        await saveCombo(tempRoot, editableCombo.output, editableCombo.inputs.join(", "), "KC_ESC", editableCombo.inputs.join(", "));
        const updated = nativeFs.readFileSync(targetKeymap, "utf8");
        assert(
            updated.includes("COMBO(KC_ESC, (" + editableCombo.inputs.join(", ") + "))"),
            "Profile Studio did not replace the selected combo output"
        );
    } finally {
        nativeFs.rmSync(tempRoot, {recursive: true, force: true});
    }

    {
        const nativeFs = require("fs");
        const tempRoot = nativeFs.mkdtempSync(path.join(require("os").tmpdir(), "profile-studio-config-defaults-"));
        try {
            const sourceConfig = path.join(${JSON.stringify(repoRoot)}, KEYMAP_CONFIG_RELATIVE_PATH);
            const targetConfig = path.join(tempRoot, KEYMAP_CONFIG_RELATIVE_PATH);
            nativeFs.mkdirSync(path.dirname(targetConfig), {recursive: true});
            nativeFs.copyFileSync(sourceConfig, targetConfig);
            await patchConfigDefaults(tempRoot, [
                {macro: "TAPPING_TERM", value: "201"},
                {macro: "RGB_AUTOMOUSE_GRADIENT_ENABLE", enabled: false},
            ]);
            const updated = nativeFs.readFileSync(targetConfig, "utf8");
            assert(
                updated.includes("#define TAPPING_TERM 201"),
                "Profile Studio did not patch config.h value defaults"
            );
            assert(
                updated.includes("// #        define RGB_AUTOMOUSE_GRADIENT_ENABLE"),
                "Profile Studio did not disable config.h toggle defaults"
            );
        } finally {
            nativeFs.rmSync(tempRoot, {recursive: true, force: true});
        }
    }

    const reusableGroups = model.rgb?.ledGroups || [];
    const thumbsGroup = reusableGroups.find((group) => group.name === "RGB_LED_GROUP_THUMBS");
    assert(thumbsGroup, "Profile Studio missed RGB_LED_GROUP_THUMBS");
    assert(
        JSON.stringify((thumbsGroup.ledIndices || []).map(Number)) === JSON.stringify([26, 27, 28, 25, 24, 53, 54, 55]),
        "Profile Studio parsed RGB_LED_GROUP_THUMBS with unexpected LEDs"
    );
    assert(thumbsGroup.usageCount > 0, "Profile Studio did not record RGB_LED_GROUP_THUMBS usages");

    const reusableRows = []
        .concat(model.rgb?.layerLedGroups || [])
        .concat(model.rgb?.pdModeLedGroups || [])
        .concat(model.rgb?.comboFeedbackLedGroups || [])
        .concat(model.rgb?.keyBehaviorFeedbackLedGroups || [])
        .filter((row) => row.ledGroup === "RGB_LED_GROUP_THUMBS");
    assert(reusableRows.length === thumbsGroup.usageCount, "Profile Studio reusable LED group usage count is inconsistent");
    assert(
        reusableRows.every((row) => row.ledGroupKind === "reusable"),
        "Profile Studio did not mark named RGB_LED_GROUP_THUMBS rows as reusable"
    );
    assert(
        (model.rgb?.keyBehaviorFeedbackLedGroups || []).some((row) => row.owner === "KEY_FEEDBACK_GROUP_ALL" && row.ledGroup === "RGB_LED_GROUP_THUMBS"),
        "Profile Studio missed the all-feedback reusable thumb LED group row"
    );

    console.log(
        "Profile Studio key behavior coverage OK: " +
        (model.keyBehaviors || []).length +
        " behavior rows, " +
        canonicalOnly.length +
        " canonical-only matches"
    );
})().catch((error) => {
    console.error(error && error.stack || error);
    process.exitCode = 1;
});
`;

vm.runInNewContext(source + appendedCheck, context, {filename: sourcePath});
