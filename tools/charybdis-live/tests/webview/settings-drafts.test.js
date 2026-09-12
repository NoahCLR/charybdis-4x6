"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const vm = require("node:vm");
const {createSettingsDraftStore} = require("../../webview/settings-drafts");
const {getStudioHtml} = require("../../webview/studio-ui");
const fields = value => [{macro: "timeout", value}];
test("settings drafts survive failures and remain bound to their original device snapshot", () => {
    const store = createSettingsDraftStore();
    store.capture("mouse", fields("800"), fields("1200"), "before");
    const copy = store.get("mouse"); copy.fields[0].value = "broken";
    assert.equal(store.get("mouse").fields[0].value, "800");
    store.capture("mouse", fields("900"), fields("1200"), "external");
    assert.equal(store.get("mouse").base, "before");
    assert.equal(store.stale("mouse", "external"), true);
    store.remove("mouse"); assert.equal(store.get("mouse"), undefined);
});
test("verified section saves retain other drafts and only advance the exact submitted base", () => {
    const store = createSettingsDraftStore();
    store.capture("mouse", fields("800"), fields("1200"), "before");
    store.capture("timing", fields("200"), fields("150"), "before");
    store.capture("old", fields("200"), fields("150"), "older");
    store.accept("mouse", fields("800"), "before", "after");
    assert.equal(store.get("mouse"), undefined);
    assert.equal(store.get("timing").fields[0].value, "200");
    assert.equal(store.stale("timing", "after"), false);
    assert.equal(store.get("old").base, "older");
    store.accept(null, null, "after", "macro save");
    assert.equal(store.stale("timing", "macro save"), false);
    const saved = store.snapshot(); store.clear(); store.restore(saved);
    assert.equal(store.get("timing").fields[0].value, "200");
});
test("the delivered Defaults controls preserve zero, use accessible labels and gate stale saves", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    new vm.Script(script);
    const context = vm.createContext({model: {activeProfile: {id: "board"}, settingsEditing: {identity: "before", writable: true}},
        settingsDrafts: createSettingsDraftStore(), settingsSavePending: false, settingsOpen: {},
        escapeAttr: String, escapeHtml: String, layersForUi: () => [], optionsWithLabels: () => ""});
    vm.runInContext(script.slice(script.indexOf("    function settingsDraftKey("), script.indexOf("    function renderRgbStudio(")), context);
    assert.match(context.renderConfigDefaultControl({macro:"speed", label:"Speed", kind:"number", value:0, validate:"nonnegative-int"}), /aria-label='Speed'.*value='0'/);
    assert.equal(context.settingsWriteAvailable("autoMouse"), true);
    context.layersForUi = () => [{name: "Layer 4", displayName: "Pointer"}];
    vm.runInContext(script.slice(script.indexOf("    function validateIdentifier("), script.indexOf("    function validateSafeConfigExpression(")), context);
    assert.equal(context.validateLayerIdentifier("Layer 4"), "");
    assert.notEqual(context.validateLayerIdentifier("Layer 9"), "");
    context.settingsDrafts.capture(context.settingsDraftKey("autoMouse"), fields("800"), fields("1200"), "before");
    context.model.settingsEditing.identity = "external";
    assert.equal(context.settingsWriteAvailable("autoMouse"), false);
    assert.match(context.renderConfigDefaultSection({id:"autoMouse",fields:[]}), /data-write-unavailable disabled/);
});

test("Read from keyboard retains Defaults drafts across the shared draft reset", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const store = createSettingsDraftStore(), sent = [];
    let refresh;
    const context = vm.createContext({settingsDrafts: store, model: {},
        document: {getElementById: id => {assert.equal(id, "reload"); return {addEventListener: (event, handler) => {assert.equal(event, "click"); refresh = handler;}};}},
        captureSettingsDrafts: () => store.capture("board:timing", fields("175"), fields("150"), "before"),
        post: message => sent.push(message.type), clearLayoutComboOriginal() {}, resetMacroRecorderState() {},
        behaviorDrafts: {clear() {}}, keyPickerHost: {innerHTML: ""}});
    vm.runInContext(script.slice(script.indexOf("    function discardLocalDraftState("), script.indexOf("    function render()")), context);
    vm.runInContext(script.slice(script.indexOf('    document.getElementById("reload")'), script.indexOf('    document.getElementById("profileSelect")')), context);
    refresh();
    assert.equal(store.get("board:timing").fields[0].value, "175");
    assert.equal(store.stale("board:timing", "external"), true);
    assert.deepEqual(sent, ["refresh"]);
    context.discardLocalDraftState();
    assert.equal(store.keys().length, 0, "explicit profile changes still clear old device drafts");
});
