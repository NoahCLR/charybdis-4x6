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
    assert(
        !getClientScript().includes("window.prompt"),
        "Profile Studio must not use browser prompts inside the VS Code webview"
    );
    assert(
        getClientScript().includes('type: "requestCreateProfile"'),
        "Profile Studio new-profile button must request extension-side input"
    );
    assert(
        getClientScript().includes("layoutComboShouldSaveOriginal(original, originalSource, payload.inputs)"),
        "Profile Studio layout combo save path must distinguish auto-matched combos from explicit edits"
    );
    assert(
        getClientScript().includes('activeLayoutComboOriginalSource === "matched"'),
        "Profile Studio layout combo builder must clear stale auto-matched combo identity when inputs diverge"
    );
    assert(
        getClientScript().includes('addSource(combo.output, { kind: "combo", combo });'),
        "Profile Studio layer behavior overview must include behavior rows reached through combo outputs"
    );
    assert(
        getClientScript().includes('addSource(input, { kind: "comboInput", combo, input });'),
        "Profile Studio layer behavior overview must include behavior rows attached to combo inputs"
    );
    assert(
        getClientScript().includes('renderTooltipHeader("Reachable via", "Physical keys, combo inputs, or combo outputs on the active layer'),
        "Profile Studio layer behavior overview must label combo-output behavior sources as reachable entries"
    );
    assert(
        getClientScript().includes("function dualRoleLayoutVisual") &&
        getClientScript().includes('call.helper === "LT"') &&
        getClientScript().includes("dual-role-separator-line") &&
        getClientScript().includes("dual-role-hold-label"),
        "Profile Studio layout keys should render LT()/mod-tap hold legends"
    );
    assert(
        getClientScript().includes("function keyFaceRows") &&
        getClientScript().includes("function keyFaceTopRowCount") &&
        getClientScript().includes("function keyFaceLowerRows") &&
        getClientScript().includes("comboTopY") &&
        getClientScript().includes("badges.map((badge, index)") &&
        getClientScript().includes("const keyFaceState = { hasBehavior: dots.length > 0, hasCombo: badges.length > 0 };"),
        "Profile Studio layout keys should reserve dynamic rows for behavior markers, combo badges, tap labels, and hold legends"
    );
    assert(
        displayKeyExpression("DRAGSCROLL") === "Dragscroll" &&
        displayKeyExpression("DRAGSCROLL_LOCK") === "Dragscroll Lock",
        "Profile Studio should render dragscroll keycodes with friendly labels"
    );

    const model = await buildModel(${JSON.stringify(repoRoot)});
    const aliases = model.qmkKeycodeAliases || {};
    assert(
        model.qmkKeyLabels.KC_BSLS === ${JSON.stringify("\\")},
        "Profile Studio did not decode escaped QMK backslash labels"
    );
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
        await saveCombo(tempRoot, DEFAULT_PROFILE_TARGET, editableCombo.output, editableCombo.inputs.join(", "), "KC_ESC", editableCombo.inputs.join(", "));
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
            await patchConfigDefaults(tempRoot, DEFAULT_PROFILE_TARGET, [
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

    {
        const nativeFs = require("fs");
        const originalShowInputBox = vscode.window.showInputBox;
        const tempRoot = nativeFs.mkdtempSync(path.join(require("os").tmpdir(), "profile-studio-new-profile-"));
        try {
            nativeFs.writeFileSync(
                path.join(tempRoot, "qmk.json"),
                JSON.stringify({userspace_version: "1.0", build_targets: []}, null, 4) + "\\n"
            );
            const created = await createProfile(tempRoot, "fresh_profile");
            assert(created.keymap === "fresh_profile", "Profile Studio created the wrong keymap target");
            const targetPaths = profileTargetPaths(tempRoot, created);
            for (const filePath of [targetPaths.keymap, targetPaths.config, targetPaths.rgb, targetPaths.rules]) {
                assert(nativeFs.existsSync(filePath), "Profile Studio did not create " + path.relative(tempRoot, filePath));
            }
            const starterKeymap = nativeFs.readFileSync(targetPaths.keymap, "utf8");
            assert(
                starterKeymap.includes("Fresh Profile Charybdis 4x6 keymap data"),
                "Profile Studio starter keymap did not render the profile title"
            );
            assert(
                starterKeymap.includes("This translation unit owns the authored keymap data"),
                "Profile Studio starter keymap is missing the authored-data comments"
            );
            assert(
                starterKeymap.includes("// ─── Keymap Layouts") &&
                    starterKeymap.includes("// clang-format off") &&
                    starterKeymap.includes("╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮"),
                "Profile Studio starter keymap is missing the documented physical layout scaffold"
            );
            const freshModel = await buildModel(tempRoot, created, [created]);
            assert((freshModel.diagnostics || []).length === 0, "Profile Studio starter profile should parse without diagnostics");
            assert((freshModel.layers || []).length === 5, "Profile Studio starter profile should expose five default layers");
            assert((freshModel.combos || []).length === 0, "Profile Studio starter profile should begin with no active combos");
            assert((freshModel.keyBehaviors || []).length === 0, "Profile Studio starter profile should begin with no active key behaviors");

            const parsedQmkJson = JSON.parse(nativeFs.readFileSync(path.join(tempRoot, "qmk.json"), "utf8"));
            parsedQmkJson.build_targets.push(["bastardkb/charybdis/4x6", "deleted_profile"]);
            nativeFs.writeFileSync(path.join(tempRoot, "qmk.json"), JSON.stringify(parsedQmkJson, null, 4) + "\\n");
            const staleState = {activeProfileId: "bastardkb/charybdis/4x6:deleted_profile"};
            const active = await activeProfileTarget(tempRoot, staleState);
            assert(
                active.target && active.target.keymap === "fresh_profile",
                "Profile Studio should fall back to an editable profile when qmk.json references a deleted keymap"
            );
            assert(
                active.profiles.some((profile) => profile.keymap === "deleted_profile" && !profile.editable),
                "Profile Studio should keep stale registered keymaps visible as incomplete metadata"
            );
            const refreshPosts = [];
            await handleWebviewMessage(
                {webview: {postMessage(message) { refreshPosts.push(message); }}},
                tempRoot,
                staleState,
                {type: "refresh"}
            );
            const cleanedQmkJson = JSON.parse(nativeFs.readFileSync(path.join(tempRoot, "qmk.json"), "utf8"));
            assert(
                !(cleanedQmkJson.build_targets || []).some((target) => Array.isArray(target) && target[1] === "deleted_profile"),
                "Profile Studio reload should remove qmk.json targets whose keymap folder is missing"
            );
            assert(
                (cleanedQmkJson.build_targets || []).some((target) => Array.isArray(target) && target[1] === "fresh_profile"),
                "Profile Studio reload should keep qmk.json targets whose keymap folder exists"
            );
            assert(
                refreshPosts.some((message) => message.notice && message.notice.includes("deleted_profile")),
                "Profile Studio reload should report removed stale qmk.json targets"
            );
            assert(
                refreshPosts.some((message) => message.model && !(message.model.profiles || []).some((profile) => profile.keymap === "deleted_profile")),
                "Profile Studio reload should stop showing removed stale qmk.json targets"
            );

            await appendCombo(tempRoot, created, "KC_ESC", "KC_Q, KC_W");
            let updatedKeymap = nativeFs.readFileSync(targetPaths.keymap, "utf8");
            assert(!updatedKeymap.includes("NOAH_KEYMAP_EMPTY_COMBOS"), "Profile Studio did not remove the empty combo flag");
            assert(updatedKeymap.includes("COMBO(KC_ESC, (KC_Q, KC_W))"), "Profile Studio did not append a combo to the starter profile");

            await saveKeyBehavior(tempRoot, created, {
                keycode: "KC_Q",
                steps: [
                    {
                        tapCount: 0,
                        tap: {helper: "TAP_SENDS", action: "KC_ESC"},
                    },
                ],
            });
            updatedKeymap = nativeFs.readFileSync(targetPaths.keymap, "utf8");
            assert(!updatedKeymap.includes("NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS"), "Profile Studio did not remove the empty behavior flag");
            assert(!updatedKeymap.includes("{0},"), "Profile Studio did not replace the starter behavior placeholder");
            const editedModel = await buildModel(tempRoot, created, [created]);
            assert((editedModel.combos || []).length === 1, "Profile Studio did not parse the starter combo after append");
            assert((editedModel.keyBehaviors || []).length === 1, "Profile Studio did not parse the starter behavior after save");

            await saveKeyBehavior(tempRoot, created, {
                keycode: "KC_Q",
                steps: [
                    {
                        tapCount: 1,
                        tap: {helper: "TAP_SENDS", action: "VIA_MACRO_10"},
                    },
                ],
            });
            updatedKeymap = nativeFs.readFileSync(targetPaths.keymap, "utf8");
            assert(!updatedKeymap.includes("},,"), "Profile Studio left a duplicate comma after replacing a behavior row");
            assert(
                updatedKeymap.includes("                        [1] = {.tap = TAP_SENDS(VIA_MACRO_10)},"),
                "Profile Studio did not replace the existing behavior row"
            );

            const messageRoot = nativeFs.mkdtempSync(path.join(require("os").tmpdir(), "profile-studio-message-profile-"));
            try {
                nativeFs.writeFileSync(
                    path.join(messageRoot, "qmk.json"),
                    JSON.stringify({userspace_version: "1.0", build_targets: []}, null, 4) + "\\n"
                );
                let inputValidated = false;
                vscode.window.showInputBox = async (options) => {
                    assert(options && typeof options.validateInput === "function", "Profile name prompt should validate input");
                    assert(options.validateInput("Bad Name"), "Profile name prompt should reject invalid keymap names");
                    assert(!options.validateInput("message_profile"), "Profile name prompt should accept valid keymap names");
                    inputValidated = true;
                    return " message_profile ";
                };
                const posted = [];
                await handleWebviewMessage(
                    {webview: {postMessage(message) { posted.push(message); }}},
                    messageRoot,
                    {activeProfileId: ""},
                    {type: "requestCreateProfile"}
                );
                assert(inputValidated, "Profile Studio did not prompt for a new profile name");
                assert(nativeFs.existsSync(path.join(messageRoot, "keyboards/bastardkb/charybdis/4x6/keymaps/message_profile/keymap.c")), "requestCreateProfile did not create keymap.c");
                assert(
                    posted.some((message) => message.type === "model" && message.model?.activeProfile?.keymap === "message_profile"),
                    "requestCreateProfile did not activate and post the created profile model"
                );
            } finally {
                vscode.window.showInputBox = originalShowInputBox;
                nativeFs.rmSync(messageRoot, {recursive: true, force: true});
            }
        } finally {
            vscode.window.showInputBox = originalShowInputBox;
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
