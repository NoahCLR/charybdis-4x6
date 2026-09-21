"use strict";
const assert = require("node:assert/strict");
const vm = require("node:vm");
const test = require("node:test");
const {renderDeviceProfileDetails} = require("../../webview/device-profile-ui");
const {renderDeviceCombos} = require("../../webview/combo-ui");
const {decodeComboPages} = require("../../core/protocol/combo-readback-v1");
const {fixturePages} = require("../fixtures/device-combos");
const {getStudioHtml} = require("../../webview/studio-ui");
const {buildDeviceModel} = require("../../core/session/device-model");
const {decodedDeviceProfile} = require("../fixtures/device-profile");

// Only the DOM node construction surface used by this standalone renderer.
// Deliberately no HTML parser: device values must remain text.
class Node {
    constructor(tag) {this.tagName = tag; this.children = []; this.text = "";}
    set innerHTML(value) {throw new Error("Device values must not be parsed as HTML");}
    set textContent(value) {this.text = String(value); this.children = [];}
    get textContent() {return this.text + this.children.map(child => child.textContent).join(" ");}
    append(...children) {this.children.push(...children);}
    replaceChildren(...children) {this.text = ""; this.children = children;}
    all(tag) {return this.children.flatMap(child => [...(child.tagName === tag ? [child] : []), ...child.all(tag)]);}
}
function documentForView() {
    const nodes = {deviceCombos: new Node("section"), deviceRgbStages: new Node("section")};
    return {nodes, createElement: tag => new Node(tag), getElementById: id => nodes[id]};
}

test("device readback renders RGB stages", () => {
    const document = documentForView();
    renderDeviceProfileDetails(document, buildDeviceModel({committed: decodedDeviceProfile()}));
    assert.match(document.nodes.deviceRgbStages.textContent, /Keyboard lighting/);
    assert.match(document.nodes.deviceRgbStages.textContent, /Layer colours/);
    assert.equal(document.nodes.deviceRgbStages.all("input").length, 5);
    assert.ok(document.nodes.deviceRgbStages.all("input").every(input => input.type === "checkbox" && input.checked));
    assert.ok(document.nodes.deviceRgbStages.all("label").every(label => label.className.includes("toggle-inline")));
    renderDeviceProfileDetails(document, buildDeviceModel({baseRgb: {state: "read", effectId: 1, hue: 0, saturation: 255, brightness: 255, speed: 32}}));
    assert.match(document.nodes.deviceRgbStages.textContent, /Solid colour · 100% brightness/);
    assert.match(document.nodes.deviceRgbStages.textContent, /Hue 0 · Saturation 255/);
    assert.match(document.nodes.deviceRgbStages.textContent, /last read/);
    renderDeviceProfileDetails(document, buildDeviceModel({}));
    assert.match(document.nodes.deviceRgbStages.textContent, /No RGB stage configuration/);
});

test("the generated webview script includes the device renderer and parses as delivered", () => {
    const html = getStudioHtml();
    const script = html.match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    new vm.Script(script);
    assert.match(script, /function renderDeviceProfileDetails/);
    assert.match(script, /renderDeviceProfileDetails\(document, model\)/);
    assert.doesNotMatch(script, /\["behaviors", "Behaviours"\]/);
    assert.match(script, /renderSelectedBehaviorEditor\(behaviorTarget, selectedBehavior\)/);
    assert(script.indexOf('["macros", "Macros"]') < script.indexOf('["pdModes", "Pointing modes"]'));
    assert(script.indexOf('["pdModes", "Pointing modes"]') < script.indexOf('["rgb", "RGB"]'));
    assert.match(html, /id="connectionHealth"/);
    assert.match(html, /id="profileHealth"/);
    assert.match(html, /id="draftHealth"/);
    assert.match(html, /id="recoveryHealth"/);
    assert.doesNotMatch(script, /keymap\.c|rgb_config\.c|["']config\.h["']|compileFirmware|generateProfileDocs/);
});

test("the layer overview orders combos before macros", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const overview = script.slice(script.indexOf("    function renderLayerOverview("), script.indexOf("    function renderTooltipHeader("));
    assert(overview.indexOf("renderLayerBehaviorTable") < overview.indexOf("renderLayerComboTable"));
    assert(overview.indexOf("renderLayerComboTable") < overview.indexOf("renderLayerMacroTable"));
    assert(overview.indexOf("renderLayerMacroTable") < overview.indexOf("renderLayerPdModeTable"));
});

test("the layer overview resolves every configured pointing-mode alias", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const functions = script.slice(script.indexOf("    function collectLayerPdModes("), script.indexOf("    function renderStep("));
    const aliases = {
        "0x7E50": "DRAGSCROLL",
        "0x7E51": "VOLUME_MODE",
        "0x7E52": "BRIGHTNESS_MODE",
        "0x7E53": "ZOOM_MODE",
        "0x7E5A": "ARROW_MODE_LOCK",
        "0x7E55": "PINCH_MODE",
        "0x7EF0": "PD_SLOT_6",
    };
    const behaviors = new Map([["0x7E55", {steps: [{tapCountName: "Double Tap Branch", hold: {action: "0x7E53", helper: "PRESS_AND_HOLD_UNTIL_RELEASE"}}]}]]);
    const context = vm.createContext({
        model: {
            pdModes: [{id: 4, name: "Arrow"}, {id: 6, name: "Window switch"}],
            rgb: {pdModeColors: [
                {pointingMode: "PD_MODE_DRAGSCROLL", color: {h: "1", s: "2", v: "3"}, locality: "RGB_RIGHT_HALF"},
                {pointingMode: "PD_MODE_SLOT_6", color: {h: "4", s: "5", v: "6"}, locality: "RGB_BOTH_HALVES"},
            ]},
        },
        canonicalLayoutKeyExpression: keycode => aliases[keycode] || keycode,
        behaviorForKey: keycode => behaviors.get(keycode),
        layerCombos: () => [],
    });
    vm.runInContext(functions, context);
    const layer = {positions: [
        {keycode: "0x7E52", display: "Brightness"},
        {keycode: "0x7E55", display: "Pinch"},
        {keycode: "0x7E51", display: "Volume"},
        {keycode: "0x7E50", display: "Dragscroll"},
        {keycode: "0x7E5A", display: "Arrow toggle"},
        {keycode: "0x7EF0", display: "Custom mode"},
    ]};
    const rows = context.collectLayerPdModes(layer);
    assert.deepEqual(new Set(rows.map(row => row.mode)), new Set([
        "PD_MODE_BRIGHTNESS",
        "PD_MODE_PINCH",
        "PD_MODE_ZOOM",
        "PD_MODE_VOLUME",
        "PD_MODE_DRAGSCROLL",
        "PD_MODE_ARROW",
        "PD_MODE_SLOT_6",
    ]));
    assert.equal(rows.find(row => row.mode === "PD_MODE_ZOOM").keycode, "ZOOM_MODE");
    assert.equal(rows.find(row => row.mode === "PD_MODE_ARROW").displayName, "Arrow");
    assert.equal(rows.find(row => row.mode === "PD_MODE_SLOT_6").keycode, "PD_SLOT_6");
    assert.equal(rows.find(row => row.mode === "PD_MODE_SLOT_6").displayName, "Window switch");
    assert.deepEqual(rows.find(row => row.mode === "PD_MODE_DRAGSCROLL").color, {h: "1", s: "2", v: "3"});
});

test("the layer overview presents pointing modes by their clean device names", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const render = script.slice(script.indexOf("    function renderPdModeCell("), script.indexOf("    function collectLayerPdModes("));
    const context = vm.createContext({escapeHtml: value => String(value).replace(/</g, "&lt;")});
    vm.runInContext(render, context);
    const html = context.renderPdModeCell({displayName: "Window <switch>", locked: false, mode: "PD_MODE_SLOT_6", keycode: "PD_SLOT_6"});
    assert.match(html, /Window &lt;switch>/);
    assert.match(html, /Hold/);
    assert.doesNotMatch(html, /PD_MODE_SLOT_6|PD_SLOT_6/);
});


test("the Combos tab renders the device table safely and clears stale readout", () => {
    const document = documentForView();
    const pages = fixturePages();
    const combos = {state: "read", ...decodeComboPages(pages[0], pages.slice(1))};
    const model = buildDeviceModel({combos});
    model.combos[0].inputDisplays[0] = "<img src=x>";
    model.combos[0].termMs = 0;
    model.combos[0].mustHold = true;
    renderDeviceCombos(document, model);
    const host = document.nodes.deviceCombos;
    assert.equal(host.all("tbody")[0].children.length, 2);
    assert.match(host.textContent, /Cmd\+C \+ Cmd\+V/);
    assert.match(host.textContent, /0 ms window/);
    assert.match(host.textContent, /Hold/);
    assert.match(host.textContent, /<img src=x>/);
    assert.equal(host.all("img").length, 0);
    renderDeviceCombos(document, {combos: [], comboReadback: {state: "unavailable", error: {message: "Flash updated firmware"}}});
    assert.match(host.textContent, /Flash updated firmware/);
    assert.equal(host.all("tbody").length, 0);
});
