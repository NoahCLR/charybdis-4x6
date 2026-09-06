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
    const nodes = {deviceCombos: new Node("section"), deviceBehaviors: new Node("section"), deviceRgbStages: new Node("section")};
    return {nodes, createElement: tag => new Node(tag), getElementById: id => nodes[id]};
}

test("device readback renders all behavior rows, timing zeros, branches and RGB stages", () => {
    const document = documentForView();
    renderDeviceProfileDetails(document, buildDeviceModel({committed: decodedDeviceProfile()}));
    const host = document.nodes.deviceBehaviors;
    assert.equal(host.all("tbody")[0].children.length, 37);
    assert.match(host.textContent, /Tap\/hold: 0/);
    assert.match(host.textContent, /Double Tap Branch/);
    assert.match(host.textContent, /TAP_ON_RELEASE_AFTER_HOLD/);
    assert.match(host.textContent, /VIA_MACRO_10/);
    assert.match(host.textContent, /Auto-mouse anchor flag/);
    assert.equal(host.all("tbody")[0].children[0].children[3].textContent, "Not set");
    assert.match(document.nodes.deviceRgbStages.textContent, /Layer colours: enabled/);
    renderDeviceProfileDetails(document, buildDeviceModel({baseRgb: {state: "read", effectId: 1, hue: 0, saturation: 255, brightness: 255, speed: 32}}));
    assert.match(document.nodes.deviceRgbStages.textContent, /Solid colour · Brightness 100%/);
    assert.match(document.nodes.deviceRgbStages.textContent, /Hue 0, saturation 255/);
    assert.match(document.nodes.deviceRgbStages.textContent, /last-read/);
    renderDeviceProfileDetails(document, buildDeviceModel({}));
    assert.equal(host.all("tbody").length, 0, "a new empty read must clear old rows");
    assert.match(document.nodes.deviceRgbStages.textContent, /No RGB configuration/);
});

test("device strings stay text and cannot inject markup into the new view", () => {
    const document = documentForView();
    const model = buildDeviceModel({committed: decodedDeviceProfile()});
    model.keyBehaviors[0].keycode = '<img src=x onerror="throw 1">';
    renderDeviceProfileDetails(document, model);
    assert.match(document.nodes.deviceBehaviors.textContent, /<img src=x/);
    assert.equal(document.nodes.deviceBehaviors.all("img").length, 0);
});

test("the generated webview script includes the device renderer and parses as delivered", () => {
    const html = getStudioHtml();
    const script = html.match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    new vm.Script(script);
    assert.match(script, /function renderDeviceProfileDetails/);
    assert.match(script, /renderDeviceProfileDetails\(document, model, post\)/);
    assert.match(script, /\["behaviors", "Behaviours"\]/);
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
