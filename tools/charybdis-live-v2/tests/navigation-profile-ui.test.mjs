import assert from "node:assert/strict";
import test from "node:test";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (name) => fs.readFileSync(path.join(root, "webview", "ui", name), "utf8");

test("the navigation rail stays label-only", () => {
    const shell = read("shell.mjs");
    assert.doesNotMatch(shell, /nav-count|function counts\(/);
    assert.doesNotMatch(shell, /layers\.length|macroSlots|configDefaults/);
});

test("layer tabs sit on the board they choose, on every layer-aware screen", () => {
    for (const name of ["keys.mjs", "lighting.mjs"]) {
        const screen = read(name);
        assert.match(screen, /const card = el\(`<div class="board-card"><\/div>`\);/, `${name} draws the board as a card`);
        assert.match(screen, /card\.append\((bar|layerBar\(\))\);[\s\S]*card\.appendChild\(board\(/, `${name} puts the layer tabs on it, above the board`);
        assert.doesNotMatch(screen, /workbench-stack/, `${name} keeps no separate layer box over the workbench`);
    }
});

test("Lighting's stage tabs are the paint order, with no second row of stages", () => {
    const lighting = read("lighting.mjs");
    assert.doesNotMatch(lighting, /paintorder|pochip|the board above shows this layer/);
    assert.match(lighting, /stages\.map\(stageTab\)/);
});

test("profile management leads with working import and export cards", () => {
    const profile = read("profile.mjs");
    const actions = profile.indexOf("profile-actions");
    const recovery = profile.indexOf("profile-recovery");
    assert(actions >= 0 && recovery > actions, "backup actions must precede secondary recovery status");
    assert.match(profile, /class="card profile-action"/);
    assert.match(profile, /Export profile…/);
    assert.match(profile, /Choose profile…/);
    assert.match(profile, /exportPortableProfile/);
    assert.match(profile, /choosePortableProfile/);
    assert.doesNotMatch(profile, /Layer priority/);
});
