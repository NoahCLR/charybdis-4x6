"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

// The structure only holds if something checks it. These assertions encode the
// rules in AGENTS.md so a violating import fails here rather than being noticed
// later, which is how the 14,539-line file this app replaced came to exist.

const APP_ROOT = path.resolve(__dirname, "..");

// A layer may import from itself and from the layers listed here. `data` is
// inert vendored content with no dependencies of its own, so anything may read
// it without creating a cycle.
const ALLOWED_IMPORTS = {
    data: [],
    transport: ["data"],
    schema: ["data"],
    protocol: ["transport", "schema", "data"],
    model: ["schema", "data"],
    session: ["transport", "protocol", "model", "schema", "data"],
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
    for (const dir of ["core", "scripts", "webview"]) {
        for (const file of sourceFiles(dir)) {
            const text = fs.readFileSync(file, "utf8");
            assert.ok(
                !/require\("vscode"\)|from "vscode"/.test(text),
                `${path.relative(APP_ROOT, file)} imports vscode; only extension.js may`
            );
        }
    }
});

test("the ported webview stays a renderer", () => {
    // studio-ui.js may read the vendored catalog to bake constants into the
    // page, but it must not reach for device or session logic: the UI renders
    // the model the host posts and nothing else.
    const forbidden = ["core/transport", "core/protocol", "core/session"];
    for (const file of sourceFiles("webview")) {
        for (const target of [...requiresIn(file), ...importsIn(file)]) {
            assert.ok(
                !forbidden.some((prefix) => target.includes(prefix)),
                `${path.relative(APP_ROOT, file)} imports ${target}; the UI must render the posted model instead`
            );
        }
    }
});

test("nothing we author mentions the firmware repository", () => {
    // The one sanctioned QMK reader is the catalog generator, which is a build
    // step producing a checked-in file rather than a runtime dependency.
    const authored = [...sourceFiles("core"), path.join(APP_ROOT, "extension.js")];
    for (const file of authored) {
        const text = fs.readFileSync(file, "utf8");
        for (const marker of ["keymap.c", "rgb_config.c", "config.h", "bastardkb-qmk", "qmk_firmware", ".hjson"]) {
            assert.ok(
                !text.includes(marker),
                `${path.relative(APP_ROOT, file)} references ${marker}; this app must not read the repository`
            );
        }
    }
});

test("the ported UI cannot touch the filesystem, whatever its copy still says", () => {
    // studio-ui.js carries stale mentions of the C source files in tooltips and
    // help text, inherited from the editor it was ported from. Those are display
    // strings and get rewritten tab by tab. What must never come back is the
    // ability to act on them, so assert on capability rather than wording.
    for (const file of sourceFiles("webview")) {
        const text = fs.readFileSync(file, "utf8");
        for (const capability of ['require("node:fs")', 'require("fs")', "readFileSync", "writeFileSync", "child_process"]) {
            assert.ok(
                !text.includes(capability),
                `${path.relative(APP_ROOT, file)} uses ${capability}; the ported UI must stay a renderer`
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
