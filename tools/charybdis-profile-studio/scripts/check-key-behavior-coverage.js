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
