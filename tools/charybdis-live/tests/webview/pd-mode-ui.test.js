"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {pdModeDpiChoices, pointingModePickerRows, renderPdModes} = require("../../webview/pd-mode-ui");
const {document: fixture} = require("../fixtures/pd-profile");
const {validateSnapshot} = require("../../core/model/portable-profile");
// Minimal DOM seam; deliberately forbids parsing device strings as markup.
class Node {
    constructor(tag) {this.tagName = tag; this.children = []; this.style = {}; this.dataset = {}; this.events = {}; this.text = "";}
    set innerHTML(value) {throw Error("Unsafe markup");}
    set textContent(value) {this.text = String(value); this.children = [];}
    get textContent() {return this.text + this.children.map(child => child.textContent).join(" ");}
    append(...children) {for (const child of children) {child.parentElement = this; this.children.push(child);}}
    replaceChildren(...children) {this.children = []; this.append(...children);}
    setAttribute(name, value) {this[name] = value;}
    addEventListener(name, handler) {(this.events[name] ||= []).push(handler);}
    dispatch(name) {for (const handler of this.events[name] || []) handler({preventDefault() {}});}
    reportValidity() {return true;}
    all() {return [this, ...this.children.flatMap(child => child.all())];}
}
function setup(writable = true) {
    const host = new Node("section"), messages = [], pickerCalls = [], slots = validateSnapshot(fixture()).pdModes;
    const document = {createElement: tag => new Node(tag), getElementById: id => id === "pdModes" ? host : host.all().find(node => node.id === id)};
    const identity = {source: "draft", generation: 0, digest: 123, originHalf: 255};
    const qmkKeycodes = [{keycode: 0x1d, value: "KC_Z", label: "Z"}, {keycode: 0x2e, value: "KC_EQL", label: "="}, {keycode: 0x50, value: "KC_LEFT", label: "Left"}];
    const configDefaults = [{id: "normalPointerSpeed", fields: [
        {macro: "normalDpi", choices: [400, 600, 800]},
        {macro: "snipingDpi", choices: [100, 200, 300, 400]},
    ]}];
    renderPdModes(document, {pdModes: slots, pdModeEditing: {writable}, draft: {}, profileIdentity: identity, qmkKeycodes, configDefaults}, value => messages.push(value), {
        open: (id, mode) => pickerCalls.push({id, mode}),
        canonicalize: value => value === "Cmd+KC_Z" ? "G(KC_Z)" : value,
    });
    return {host, messages, pickerCalls, document, identity};
}
test("eight forms keep valid engine-specific controls and the original draft identity", () => {
    const {host, document, messages, identity} = setup();
    assert.equal(host.all().filter(node => node.tagName === "form").length, 8);
    const dpi = document.getElementById("pd-0-dpi");
    assert.equal(dpi.tagName, "select");
    assert.deepEqual(dpi.children.map(option => [option.value, option.textContent]), [["0", "Use normal pointer speed"], ["100", "100 DPI"], ["200", "200 DPI"], ["300", "300 DPI"], ["400", "400 DPI"], ["600", "600 DPI"], ["800", "800 DPI"]]);
    assert.equal(dpi.value, "100");
    const form = document.getElementById("pdSlot6"), kind = document.getElementById("pd-6-kind");
    kind.value = "2";
    // Restore sets values without a native change event.
    form.dispatch("restore-pd-controls");
    assert.equal(document.getElementById("pd-6-thresholdH").parentElement.parentElement.hidden, false);
    assert.equal(document.getElementById("pd-6-thresholdX").parentElement.parentElement.hidden, true);
    document.getElementById("pd-6-name").value = "New scroll";
    form.all().find(node => node.tagName === "button" && node.textContent === "Keep mode").dispatch("click");
    assert.equal(messages[0].config.kind, 2); assert.equal(messages[0].config.name, "New scroll");
    assert.equal(messages[0].config.scroll.divisorV, 8);
    assert.equal(messages[0].config.directions, undefined);
    assert.deepEqual(messages[0].expectedBase, identity);
});
test("read-only PD slots expose no enabled editing controls", () => {
    const {host} = setup(false);
    assert(host.all().filter(node => ["input", "select", "button"].includes(node.tagName)).every(node => node.disabled));
});
test("normal flow uses the shared keycode picker while uncommon controls stay advanced", () => {
    const {document, messages, pickerCalls} = setup();
    const form = document.getElementById("pdSlot6"), left = document.getElementById("pd-6-left-key");
    const advanced = form.all().find(node => node.dataset.pdAdvanced === "6");
    assert(advanced); assert.equal(advanced.open, false);
    assert(advanced.all().includes(document.getElementById("pd-6-pointerLayer")));
    assert(advanced.all().includes(document.getElementById("pd-6-thresholdX")));
    assert(advanced.all().includes(document.getElementById("pd-6-button0-kind")));
    assert(!advanced.all().includes(document.getElementById("pd-6-axis")));
    assert.equal(document.getElementById("pd-6-axis").parentElement.parentElement.children[0].textContent, "Directional actions");
    assert.equal(document.getElementById("pd-3-up-key").value, "G(KC_EQL)");
    assert.equal(document.getElementById("pd-1-left-key").parentElement.parentElement.hidden, true);
    assert.equal(document.getElementById("pd-1-up-key").parentElement.parentElement.hidden, false);
    left.parentElement.children.find(node => node.tagName === "button").dispatch("click");
    const buttonShortcut = document.getElementById("pd-4-button1-tap-key");
    buttonShortcut.parentElement.children.find(node => node.tagName === "button").dispatch("click");
    assert.deepEqual(pickerCalls, [{id: "pd-6-left-key", mode: "single"}, {id: "pd-4-button1-tap-key", mode: "single"}]);
    left.value = "Cmd+KC_Z";
    form.all().find(node => node.tagName === "button" && node.textContent === "Keep mode").dispatch("click");
    assert.equal(messages[0].config.directions.left.keycode, "G(KC_Z)");
});
test("layout picker uses current mode labels and canonical firmware actions", () => {
    const model = {qmkKeycodes: [
        {value: "KC_A", label: "A", group: "Letters", keycode: 4},
        {value: "PD_SLOT_7", label: "History · hold", group: "Pointing modes", keycode: 0x7ef2},
        {value: "PD_SLOT_7_LOCK", label: "History · toggle", group: "Pointing modes", keycode: 0x7ef3},
    ]};
    assert.deepEqual(pointingModePickerRows(model), [[model.qmkKeycodes[1], model.qmkKeycodes[2]]]);
    assert(!pointingModePickerRows(model).flat().some(entry => entry.value === "SLOT_7_MODE"));
});
test("DPI choices combine device ladders with values already stored in modes", () => {
    assert.deepEqual(pdModeDpiChoices({
        configDefaults: [{fields: [{macro: "normalDpi", choices: [400, 600]}, {macro: "effectMode", choices: [1, 2]}]}],
        pdModes: [{dpi: 0}, {dpi: 350}],
    }), [[0, "Use normal pointer speed"], [350, "350 DPI"], [400, "400 DPI"], [600, "600 DPI"]]);
});
