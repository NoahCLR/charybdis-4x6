"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {renderPortableProfile} = require("../../webview/portable-profile-ui");

class Node {
    constructor(tag) {this.tag = tag; this.children = []; this.style = {}; this.dataset = {}; this.attributes = {}; this.textContent = "";}
    set innerHTML(value) {throw Error("Profile values must remain text");}
    append(...children) {this.children.push(...children);}
    replaceChildren(...children) {this.children = children;}
    setAttribute(key, value) {this.attributes[key] = value;}
    all(tag) {return this.children.flatMap(child => [...(child.tag === tag ? [child] : []), ...child.all(tag)]);}
    querySelectorAll(selector) {assert.equal(selector, "input[data-portable-layer]"); return this.all("input").filter(input => input.dataset.portableLayer !== undefined);}
}
function fixture() {
    const host = new Node("section"), messages = [];
    const document = {getElementById: () => host, createElement: tag => new Node(tag)};
    const model = {portable: {available: true, eightLayers: true, layers: {key: "reviewed", order: [0, 1, 2, 3, 4, 5, 6, 7], names: ["Base", "Numbers", "Symbols", "Navigation", "Pointer", "Extra 1", "Extra 2", "Extra 3"]}}};
    const render = () => renderPortableProfile(document, model, message => messages.push(message));
    const button = label => host.all("button").find(node => node.textContent === label);
    render(); return {host, model, messages, render, button};
}
test("moving or saving a layer includes its current name even before blur", () => {
    const f = fixture(), name = f.host.all("input").find(input => input.dataset.portableLayer === 4);
    name.value = "Trackball";
    f.host.all("li").find(row => row.all("input")[0] === name).all("button")[0].onclick();
    assert.equal(f.messages[0].names[4], "Trackball"); assert.equal(f.messages[0].direction, 1);
    f.model.portable.layers.order = [0, 1, 2, 3, 5, 4, 6, 7]; f.render();
    assert.equal(f.host.all("input")[2].value, "Trackball");
    f.button("Save layer changes").onclick(); assert.equal(f.messages[1].names[4], "Trackball");
});
test("base cannot move and busy controls cannot start overlapping operations", () => {
    const f = fixture(), base = f.host.all("li").at(-1);
    assert.equal(base.all("button").length, 0);
    assert.equal(f.host.all("li")[0].all("button")[0].disabled, true);
    f.model.portable.busy = true; f.render();
    assert.ok(f.host.all("button").every(button => button.disabled));
    assert.ok(f.host.all("input").every(input => input.disabled));
});
test("cancelling discards names; a new reviewed profile gets its own draft", () => {
    const f = fixture(); f.host.all("input")[0].value = "Discard this";
    const layers = f.model.portable.layers; f.model.portable.layers = null; f.render();
    f.model.portable.layers = layers; f.render(); assert.equal(f.host.all("input")[0].value, "Extra 3");
    f.model.portable.layers = {...layers, key: "different", names: Array(8).fill("<img src=x>")}; f.render();
    assert.equal(f.host.all("input")[0].value, "<img src=x>"); assert.equal(f.host.all("img").length, 0);
});
test("an interrupted import offers a complete restore without mislabelling partial data as a backup", () => {
    const f = fixture(); f.model.portable.layers = null;
    f.model.portable.review = {current: null, incoming: {layers: 8, behaviors: 37, combos: 7, macros: 12}}; f.render();
    assert.ok(f.host.all("p").some(p => p.textContent.includes("diagnostic copy")));
    f.button("Restore profile").onclick(); assert.equal(f.messages.at(-1).type, "restorePortableProfile");
});
test("the five-layer upgrade always asks for a backup before the storage change", () => {
    const f = fixture(); Object.assign(f.model.portable, {legacy: true, eightLayers: false, available: false, layers: null}); f.render();
    assert.equal(f.button("Export profile").disabled, true);
    assert.ok(f.host.all("span").some(node => node.textContent.includes("backup bridge first")));
    f.model.portable.available = true; f.render();
    assert.equal(f.button("Export profile").disabled, false); assert.equal(f.button("Import profile").disabled, true);
    assert.ok(f.host.all("span").some(node => node.textContent.includes("Export your profile before")));
});
