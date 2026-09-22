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

test("layer selection sits directly above the section selectors on every layer-aware screen", () => {
    const keys = read("keys.mjs");
    assert.match(keys, /const workbench = el\(`<div class="workbench-stack"><\/div>`\);\s*workbench\.append\(bar, bench\(\)\);/);
    assert.doesNotMatch(keys, /main\.appendChild\(bar\)/);

    const lighting = read("lighting.mjs");
    assert.match(lighting, /the board above shows this layer/);
    assert.match(lighting, /const workbench = el\(`<div class="workbench-stack"><\/div>`\);\s*workbench\.append\(bar, bench\);/);
    assert.doesNotMatch(lighting, /main\.appendChild\(layerBar/);
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
