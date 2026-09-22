"use strict";

// Developer preview: render the interface against a profile from the test
// fixtures, with no keyboard attached.
//
// This is a build step, not part of the app: it runs by hand, writes into
// `dev/`, and nothing under `webview/` ever reads a file.

const fs = require("node:fs");
const path = require("node:path");
const {ProfileDraftSession} = require("../core/session/profile-draft-session");
const {buildDeviceModel} = require("../core/session/device-model");
const portable = require("../core/model/portable-profile");
const keycodes = require("../core/data/keycode-catalog");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../core/protocol/via-layout-v1");
const {document: pdDocument} = require("../tests/fixtures/pd-profile");

// A readable keymap for the preview only. The fixture profile ships an empty
// VIA layout, which renders honestly but tells you nothing about the layout
// surface, so this script writes one in. It is preview data owned by this
// script: nothing here is read by the app or taken from a firmware checkout.
const PREVIEW_BASE = [
    "KC_ESC", "KC_1", "KC_2", "KC_3", "KC_4", "KC_5", "KC_6", "KC_7", "KC_8", "KC_9", "KC_0", "KC_MINS",
    "KC_TAB", "KC_Q", "KC_W", "KC_E", "KC_R", "KC_T", "KC_Y", "KC_U", "KC_I", "KC_O", "KC_P", "KC_BSLS",
    "KC_LSFT", "KC_A", "KC_S", "KC_D", "KC_F", "KC_G", "KC_H", "KC_J", "KC_K", "KC_L", "KC_SCLN", "KC_QUOT",
    "KC_LCTL", "KC_Z", "KC_X", "KC_C", "KC_V", "KC_B", "KC_N", "KC_M", "KC_COMM", "KC_DOT", "KC_SLSH", "KC_RALT",
    "KC_LGUI", "KC_SPC", "KC_ESC", "KC_ENT", "KC_ENT", "KC_DEL", "KC_BSPC", "KC_BSPC",
];

// Keys the keyboard stores as bare user keycodes: a configured pointing mode, an
// empty slot, and a VIA macro. The preview carries all three because they are
// the values whose stored name and semantic name differ.
const PREVIEW_PD_BINDINGS = {50: 0x7e50, 52: 0x7ef0, 48: 0x7700};

function fillPreviewLayer(document) {
    for (const [layoutIndex, code] of Object.entries(PREVIEW_PD_BINDINGS)) {
        const position = CHARYBDIS_4X6_LAYOUT_MATRIX[Number(layoutIndex)];
        if (position) document.layers[0][position[0] * 6 + position[1]] = code;
    }
    PREVIEW_BASE.forEach((name, layoutIndex) => {
        if (PREVIEW_PD_BINDINGS[layoutIndex] !== undefined) return;
        const position = CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex];
        const code = keycodes.encode(name);
        if (position && Number.isInteger(code)) document.layers[0][position[0] * 6 + position[1]] = code;
    });
    return document;
}

const capabilities = {
    compiledLayerCount: 8,
    supportedDomainMask: 31,
    actionAbiDigest: 0x61072732,
    featureFlags: 0xffff,
    responseVersion: 1,
    reportSize: 32,
    brightnessMax: 255,
};

function buildModel() {
    const doc = fillPreviewLayer(pdDocument());
    const snapshot = {
        document: doc,
        fingerprint: portable.fingerprint(doc),
        summary: portable.summary(doc),
        identity: {generation: 42},
    };
    const deviceId = "preview-charybdis";
    const session = new ProfileDraftSession(snapshot, deviceId, capabilities);
    const state = {
        selectedDeviceId: deviceId,
        connected: true,
        busy: false,
        capabilities,
        status: {committedGeneration: 42, committedDigest: 0x9ac31b70, activeGeneration: 42, peerGeneration: 42},
        devices: [{id: deviceId, manufacturer: "Bastard Keyboards", product: "Charybdis 4x6"}],
        baseRgb: {state: "read", effectId: 1, brightness: 180, hue: 140, saturation: 210, speed: 60},
    };
    const editing = session.editingState(state);
    const model = buildDeviceModel({...editing, device: state.devices[0]});
    model.draft = {...session.view(state), busy: false};
    model.profileIdentity = session.identity();
    // The host adds this block from the live connection; the preview supplies
    // the shape a complete-profile keyboard reports so the screen can be seen.
    const names = session.current.summary.names;
    model.portable = {
        available: true, eightLayers: true, legacy: false, pdUpgradeAvailable: false,
        busy: false, progress: "", review: null,
        // The host loads this when Keys → Layers asks for it; the preview has
        // no host, so it ships the shape that screen renders.
        layers: {key: snapshot.fingerprint, order: Array.from({length: 8}, (_, id) => id), names: [...names]},
    };
    model.layers.forEach((layer, index) => { layer.displayName = names[index] || layer.name; });
    return model;
}

// `--device` renders the interface against the keyboard that is plugged in,
// read-only, the way the host builds its model. Fixture data cannot show what a
// real profile does to a surface — every naming inconsistency found so far came
// from looking at this.
async function deviceModel() {
    const {ProfileDeviceService} = require("../core/session/profile-device-service");
    const service = new ProfileDeviceService({onChange: () => {}});
    try {
        await service.enumerate();
        const devices = service.snapshot().devices;
        if (!devices.length) throw new Error("no Charybdis Raw HID interface found");
        await service.connect(devices[0].id);
        await service.refresh();
        await service.readLayout();
        await service.readCommittedProfile();
        await service.readBaseRgb();
        await service.readCombos();
        if ((service.capabilities?.supportedDomainMask & 15) === 15) await service.readPortableProfile();

        const state = service.snapshot();
        const read = service.portable;
        const draft = read && !read.incomplete && state.capabilities?.compiledLayerCount === 8
            ? new ProfileDraftSession(read, state.selectedDeviceId, state.capabilities) : undefined;
        const device = state.devices.find((entry) => entry.id === state.selectedDeviceId);
        const model = buildDeviceModel({...(draft ? draft.editingState(state) : state), device});
        if (draft) {
            model.draft = {...draft.view(state), busy: false};
            model.profileIdentity = draft.identity();
            const names = draft.current.summary.names;
            model.layers?.forEach((layer, index) => {layer.displayName = names[index] || layer.name;});
        }
        model.portable = {
            available: true, eightLayers: state.capabilities?.compiledLayerCount === 8, legacy: false,
            pdUpgradeAvailable: Boolean(state.capabilities?.featureFlags & (1 << 13)),
            busy: false, progress: "", review: null,
            layers: {key: draft?.current.fingerprint, order: Array.from({length: 8}, (_, id) => id),
                names: [...(draft?.current.summary.names || [])]},
        };
        return model;
    } finally {
        await service.close();
    }
}

const model = process.argv.includes("--device") ? deviceModel() : buildModel();
// `--vscode` also writes the page as the panel actually renders it: the host
// puts its own stylesheet in front of this one (a cascade layer that dresses
// <code> and pads the body) and hangs its theme class on <body>. Both of those
// have broken this interface before, and neither is visible in dev/index.html.
function writeHostPages(page) {
    const root = "/Applications/Visual Studio Code.app/Contents/Resources/app";
    const shell = path.join(root, "out/vs/workbench/contrib/webview/browser/pre/index.html");
    if (!fs.existsSync(shell)) {
        console.log("skipped --vscode: no VS Code install at " + root);
        return;
    }
    const source = fs.readFileSync(shell, "utf8");
    const opening = source.indexOf("defaultStyles.textContent = `") + "defaultStyles.textContent = `".length;
    const hostStyles = source.slice(opening, source.indexOf("`;", opening));
    const theme = (name) => {
        const data = JSON.parse(fs.readFileSync(path.join(root, "extensions/theme-defaults/themes", name), "utf8"));
        const inherited = data.include ? theme(data.include.replace(/^\.\//, "")) : {};
        return {...inherited, ...(data.colors || {})};
    };
    for (const [label, file, klass] of [["dark", "dark_modern.json", "vscode-dark"], ["light", "light_modern.json", "vscode-light"]]) {
        const colours = theme(file);
        const declarations = Object.entries(colours).map(([key, value]) => `  --vscode-${key.replace(/\./g, "-")}: ${value};`).join("\n");
        const injected = `<style>\n${hostStyles}\n</style>\n<style>\n:root {\n`
            + `  --vscode-font-family: -apple-system, sans-serif;\n  --vscode-font-size: 13px;\n`
            + `  --monaco-monospace-font: "SF Mono", Menlo, monospace;\n${declarations}\n}\n`
            + `html, body { background: var(--vscode-editor-background); }\n</style>\n`;
        fs.writeFileSync(path.join(__dirname, "..", "dev", `vscode-${label}.html`),
            page.replace("<body>", `<body class="vscode-body ${klass}">`).replace("<link rel=", injected + "<link rel="));
        console.log(`written dev/vscode-${label}.html`);
    }
}

Promise.resolve(model).then((model) => {
fs.mkdirSync(path.join(__dirname, "..", "dev"), {recursive: true});
fs.writeFileSync(path.join(__dirname, "..", "dev", "model.json"), JSON.stringify(model));
const page = `<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>Charybdis Live v2 — preview</title>
<link rel="stylesheet" href="../webview/styles.css?built=${Date.now()}">
<script>
// Stand in for the extension host: answer the webview's "ready" with one
// fixture model, exactly as the real host answers it after reading a keyboard,
// and log every edit it posts back.
const posted = [];
let model = null, wanted = false;
const publish = () => {
    if (!model || !wanted) return;
    window.dispatchEvent(new MessageEvent("message", {data: {type: "model", model}}));
};
window.acquireVsCodeApi = () => ({
    postMessage: (message) => {
        posted.push(message);
        console.log("posted", JSON.stringify(message));
        if (message.type === "ready" || message.type === "refresh") { wanted = true; publish(); }
    },
    getState: () => undefined, setState: () => {},
});
window.__posted = posted;
fetch("./model.json").then((response) => response.json()).then((loaded) => { model = loaded; publish(); });
// ?screen=lighting opens a screen by clicking its rail button, the way a
// person would, so the preview needs no hook inside the app.
const wantedScreen = new URLSearchParams(location.search).get("screen");
if (wantedScreen) {
    addEventListener("message", () => setTimeout(() => {
        document.querySelector('[data-screen=' + wantedScreen + ']')?.click();
    }, 0), {once: true});
}
</script>
</head><body><div id="root"></div>
<script type="module" src="../webview/app.mjs?built=${Date.now()}"></script>
</body></html>
`;
fs.writeFileSync(path.join(__dirname, "..", "dev", "index.html"), page);
if (process.argv.includes("--vscode")) writeHostPages(page);
console.log("layers", model.layers.length,
    "· behaviours", model.keyBehaviors.length,
    "· combos", model.combos.length,
    "· pd slots", model.pdModes.length,
    "· macros", model.viaMacros.length + model.hardcodedMacros.length,
    "· settings sections", model.configDefaults.length,
    "· rgb layer rows", model.rgb.layerColors?.length);
console.log("written dev/index.html");
}).catch((error) => {console.error(String(error.message || error)); process.exit(1);});
