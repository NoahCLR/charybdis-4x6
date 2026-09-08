"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const vm = require("node:vm");
const {getStudioHtml} = require("../../webview/studio-ui");
const {macroKeycodes} = require("../../core/schema/macro-payload");
const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
const functions = script.slice(script.indexOf("    function activeMacroSlot("), script.indexOf("    function macroPayloadStats("));
function harness() {
    const slot = keycode => ({keycode, payload: "", empty: true});
    const context = vm.createContext({model: {viaMacros:[slot("VIA_MACRO_0"),slot("VIA_MACRO_1")], hardcodedMacros:[slot("MACRO_0")], macroEditing:{identity:"before", writable:true}},
        macroDrafts:{}, macroDraftBases:{}, macroSavePending:false, activeMacroKeycode:"VIA_MACRO_0",
        splitLayoutArguments:value => value.split(","), displayKeyExpression:value => value,
        macroPayloadKeyListError:keys => keys.some(key => !macroKeycodes().includes(key)) ? "unsupported key" : ""});
    vm.runInContext(functions, context);
    return context;
}
test("existing macro controls use both banks and preserve bases across typing and failed saves", () => {
    const c = harness();
    assert.equal(c.allMacroSlots().length, 3);
    c.setMacroDraft("MACRO_0", "user draft"); c.setMacroDraft("VIA_MACRO_0", "via draft");
    assert.equal(c.macroSlotDirty("MACRO_0"), true);
    assert.equal(c.macroWriteAvailable("MACRO_0"), true);
    c.macroSavePending = true; assert.equal(c.macroWriteAvailable("MACRO_0"), false);
    c.macroSavePending = false; c.acceptMacroSave(undefined);
    assert.equal(c.macroPayloadForSlot(c.macroSlotByKeycode("MACRO_0")), "user draft");
    c.model.macroEditing.identity = "external change";
    c.setMacroDraft("MACRO_0", "further typing");
    assert.equal(c.macroDraftBases.MACRO_0, "before");
    assert.equal(c.macroDraftStale("MACRO_0"), true);
    assert.equal(c.macroWriteAvailable("MACRO_0"), false);
});
test("an acknowledged save clears only its matching draft and advances other drafts from that base", () => {
    const c = harness();
    c.setMacroDraft("VIA_MACRO_0", "first"); c.setMacroDraft("VIA_MACRO_1", "second");
    c.model.macroEditing.identity = "after";
    c.acceptMacroSave({keycode:"VIA_MACRO_0", payload:"first", expectedFingerprint:"before"});
    assert.equal(c.macroDrafts.VIA_MACRO_0, undefined);
    assert.equal(c.macroDrafts.VIA_MACRO_1, "second");
    assert.equal(c.macroWriteAvailable("VIA_MACRO_1"), true);
    c.setMacroDraft("VIA_MACRO_0", "typed while saving");
    c.acceptMacroSave({keycode:"VIA_MACRO_0", payload:"older edit", expectedFingerprint:"before"});
    assert.equal(c.macroDrafts.VIA_MACRO_0, "typed while saving");
});
test("the existing preview renders literal braces as text and still parses chords", () => {
    const c = harness(), preview = c.parseMacroPayloadPreview('Hello {{world}}{KC_LGUI,KC_N}{250}');
    assert.equal(preview.error, "");
    assert.equal(preview.steps[0].detail, "Hello {world}");
    assert.equal(preview.steps[1].kindLabel, "Chord");
    assert.equal(preview.steps[2].kind, "delay");
});

test("layout previews recognize device macro aliases with the delivered regex escaping", () => {
    const context = vm.createContext({canonicalLayoutKeyExpression: key => ({QK_USER_0:"MACRO_0","0x773F":"VIA_MACRO_63"}[key] || key)});
    vm.runInContext(script.slice(script.indexOf("    function macroKeycodesInExpression("), script.indexOf("    function hexToRgba(")), context);
    assert.equal(context.macroKeycodesInExpression("QK_USER_0")[0], "MACRO_0");
    assert.equal(context.macroKeycodesInExpression("0x773F")[0], "VIA_MACRO_63");
});
