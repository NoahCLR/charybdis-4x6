"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

// The structure only holds if something checks it. These assertions encode the
// rules in AGENTS.md so a violating import fails here rather than being noticed
// later, which is how the 14,539-line file this app replaced came to exist.

const APP_ROOT = path.resolve(__dirname, "..");

// A layer may import from itself and from the layers listed here.
const ALLOWED_IMPORTS = {
    transport: [],
    schema: [],
    protocol: ["transport", "schema"],
    session: ["transport", "protocol", "schema"],
};

function sourceFiles(dir) {
    const root = path.join(APP_ROOT, dir);
    if (!fs.existsSync(root)) {
        return [];
    }
    return fs
        .readdirSync(root, {recursive: true, withFileTypes: true})
        .filter((entry) => entry.isFile() && entry.name.endsWith(".js"))
        .map((entry) => path.join(entry.parentPath || entry.path, entry.name));
}

function requiresIn(file) {
    return Array.from(fs.readFileSync(file, "utf8").matchAll(/require\("([^"]+)"\)/g)).map((m) => m[1]);
}

function importsIn(file) {
    return Array.from(fs.readFileSync(file, "utf8").matchAll(/\bfrom\s+"([^"]+)"/g)).map((m) => m[1]);
}

test("core layers import only downward", () => {
    for (const file of sourceFiles("core")) {
        const layer = path.basename(path.dirname(file));
        const allowed = ALLOWED_IMPORTS[layer];
        assert.ok(allowed, `${file} sits in an undeclared layer: ${layer}`);

        for (const target of requiresIn(file)) {
            if (!target.startsWith(".")) {
                continue;
            }
            const resolved = path.resolve(path.dirname(file), target);
            const targetLayer = path.basename(path.dirname(resolved));
            if (targetLayer === layer) {
                continue;
            }
            assert.ok(
                allowed.includes(targetLayer),
                `${path.relative(APP_ROOT, file)} imports ${targetLayer}/, which ${layer}/ may not depend on`
            );
        }
    }
});

test("only the extension shell knows about VS Code", () => {
    for (const dir of ["core", "media", "scripts"]) {
        for (const file of sourceFiles(dir)) {
            const text = fs.readFileSync(file, "utf8");
            assert.ok(
                !/require\("vscode"\)|from "vscode"/.test(text),
                `${path.relative(APP_ROOT, file)} imports vscode; only extension.js may`
            );
        }
    }
});

test("the webview never reaches into the core", () => {
    for (const file of sourceFiles("media")) {
        for (const target of [...requiresIn(file), ...importsIn(file)]) {
            assert.ok(
                !target.includes("core/") && !target.includes("../core"),
                `${path.relative(APP_ROOT, file)} imports the core; it must render posted snapshots instead`
            );
        }
    }
});

test("nothing at runtime reads the firmware repository", () => {
    // The one sanctioned QMK reader is the catalog generator, which is a build
    // step producing a checked-in file rather than a runtime dependency.
    const runtime = [...sourceFiles("core"), ...sourceFiles("media"), path.join(APP_ROOT, "extension.js")];
    for (const file of runtime) {
        const text = fs.readFileSync(file, "utf8");
        for (const marker of ["keymap.c", "rgb_config.c", "config.h", "bastardkb-qmk", "qmk_firmware", ".hjson"]) {
            assert.ok(
                !text.includes(marker),
                `${path.relative(APP_ROOT, file)} references ${marker}; this app must not read the repository`
            );
        }
    }
});

test("every core module has a test in the matching layer", () => {
    const untested = [];
    for (const file of sourceFiles("core")) {
        const layer = path.basename(path.dirname(file));
        const name = path.basename(file, ".js");
        const expected = path.join(APP_ROOT, "tests", layer, `${name}.test.js`);
        if (!fs.existsSync(expected)) {
            untested.push(`${layer}/${name}`);
        }
    }
    // transport.test.js covers the coordinator and both adapters together.
    const covered = new Set(["transport/request-coordinator", "transport/fake-device-adapter", "transport/device-adapter"]);
    assert.deepEqual(untested.filter((entry) => !covered.has(entry)), []);
});
