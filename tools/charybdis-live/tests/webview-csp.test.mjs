import assert from "node:assert/strict";
import test from "node:test";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), "..");

test("the panel policy permits the authored inline layout and colour styles", () => {
    const shell = fs.readFileSync(path.join(root, "panel-html.js"), "utf8");
    const ui = fs.readdirSync(path.join(root, "webview", "ui"))
        .filter((name) => name.endsWith(".mjs"))
        .map((name) => fs.readFileSync(path.join(root, "webview", "ui", name), "utf8"))
        .join("\n");

    assert.match(ui, /style=/, "the interface still authors dynamic inline styles");
    assert.match(shell, /style-src \$\{webview\.cspSource\} 'unsafe-inline'/,
        "VS Code must not discard the layout and device-colour styles");
    assert.match(shell, /script-src 'nonce-\$\{id\}'/,
        "allowing authored styles must not weaken the script policy");
    assert.doesNotMatch(shell, /script-src[^;]*unsafe-inline/,
        "inline scripts remain forbidden");
});
