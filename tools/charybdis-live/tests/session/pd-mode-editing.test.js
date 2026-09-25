"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {document} = require("../fixtures/pd-profile");
const {ProfileDraftSession} = require("../../core/session/profile-draft-session");
const {fingerprint, summary, validateSnapshot, reorderLayers} = require("../../core/model/portable-profile");
const {settingsEditorView} = require("../../core/model/settings-editor");
const {candidateMetadataForBlob, buildCandidateBeginRequest} = require("../../core/protocol/profile-candidate-v1");
const {buildDeviceModel} = require("../../core/session/device-model");
function fixture() {
    const value = document(), snapshot = {document: value, fingerprint: fingerprint(value), summary: summary(value)};
    const caps = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: value.actionAbiDigest};
    const draft = new ProfileDraftSession(snapshot, "board", caps);
    const stage = message => draft.stage({...message, draftRevision: draft.revision});
    return {draft, stage, caps};
}
test("all six modes migrate as data, with eight RGB rows and two empty slots", () => {
    const value = validateSnapshot(document());
    assert.equal(value.document.version, 2);
    assert.deepEqual(value.pdModes.map(mode => mode.name), ["Dragscroll", "Volume", "Brightness", "Zoom", "Arrow", "Pinch", "", ""]);
    assert.equal(value.rgb.formatVersion, 2); assert.equal(value.rgb.pdModeColors.length, 8);
    assert.equal(value.pdModes[4].buttons[0].modifiers, 32);
    assert.equal(value.pdModes[5].heldModifiers, 8);
    assert.deepEqual(value.settings.values.slice(10, 15), [0, 0, 0, 0, 0]);
});
test("create, bind, edit RGB and macros, reorder, review and undo share a PD draft", () => {
    const {draft, stage, caps} = fixture();
    const config = {kind: 1, name: "History", axis: 1, thresholdX: 30, directions: {left: Object.freeze({keycode: "LALT(KC_LEFT)"}), right: Object.freeze({keycode: "LALT(KC_RIGHT)"})}};
    stage({type: "savePdMode", slot: 7, config});
    assert.equal(config.directions.left.keycode, "LALT(KC_LEFT)", "shortcut resolution must not mutate the submitted form");
    const created = draft.current.fingerprint;
    assert.equal(validateSnapshot(draft.document).pdModes[7].directions.left.keycode, 0x450);
    stage({type: "updateLayoutKeys", layers: [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "PD_SLOT_7"}]}]});

    stage({type: "updatePdModeColor", pointingMode: "PD_MODE_SLOT_7", h: "64", s: "255", v: "100", locality: "RGB_RIGHT_HALF"});
    stage({type: "updateViaMacro", keycode: "VIA_MACRO_0", payload: "hello", name: "Greeting"});
    const reordered = reorderLayers(draft.document, [0, 2, 1, 3, 4, 5, 6, 7]);
    assert.deepEqual(validateSnapshot(reordered).pdModes, validateSnapshot(draft.document).pdModes);
    const view = draft.view({connected: true, selectedDeviceId: "board"});
    const slot = view.changes.find(change => change.area === "Pointing modes" && change.unit === "pd:7");
    const field = label => slot.fields.find(entry => entry.label === label)?.after;
    assert.equal(slot.status, "added"); assert.equal(slot.title, "Slot 8 · History");
    assert.equal(field("Name"), "History");
    assert.equal(field("Horizontal movement per tap"), 30);
    assert.match(field("Left"), /Inherit modifiers/);
    const model = buildDeviceModel({...draft.editingState({connected: true, selectedDeviceId: "board", capabilities: caps}), capabilities: caps});
    assert.equal(model.pdModes[7].name, "History");
    assert(model.qmkKeycodes.some(key => key.value === "PD_SLOT_7" && key.keycode === 0x7ef2 && key.label === "History · hold"));
    assert(model.qmkKeycodes.some(key => key.value === "PD_SLOT_7_LOCK" && key.keycode === 0x7ef3 && key.label === "History · toggle"));
    assert(!model.qmkKeycodes.some(key => key.value === "SLOT_7_MODE"));
    assert(model.qmkKeycodes.some(key => key.value === "PD_SLOT_6" && key.keycode === 0x7ef0 && key.label === "Slot 7 · hold (empty)"),
        "an unconfigured slot still offers its keycodes, so a button can be placed before the mode exists");
    assert(!settingsEditorView(draft.current).sections.some(section => section.fields.some(field => field.id >= 10 && field.id <= 14)));
    draft.undo(draft.revision); draft.undo(draft.revision); draft.undo(draft.revision);
    assert.equal(draft.current.fingerprint, created);
    draft.undo(draft.revision); assert.equal(draft.dirty, false);
});
test("duplicate and clear are atomic, and unsupported outputs cannot enter a draft", () => {
    const {draft, stage} = fixture();
    stage({type: "duplicatePdMode", slot: 6, source: 4});
    assert.equal(validateSnapshot(draft.document).pdModes[6].buttons[2].tap.keycode, 0x819);
    const before = draft.current.fingerprint;
    assert.throws(() => stage({type: "duplicatePdMode", slot: 6, source: 5}), /empty destination/);
    assert.throws(() => stage({type: "savePdMode", slot: 7, config: {kind: 1, name: "Recursive", thresholdY: 40, directions: {up: {keycode: "PD_SLOT_6"}}}}));
    assert.equal(draft.current.fingerprint, before);
    stage({type: "clearPdMode", slot: 6}); assert.equal(draft.dirty, false);
});
test("keycode-picker modifier expressions are accepted as directional actions", () => {
    const {draft, stage} = fixture();
    stage({type: "savePdMode", slot: 6, config: {kind: 1, name: "History", axis: 0, thresholdY: 40, directions: {up: {keycode: "G(KC_Z)"}, down: {keycode: "G(KC_Y)"}}}});
    const mode = validateSnapshot(draft.document).pdModes[6];
    assert.equal(mode.directions.up.keycode, 0x081d);
    assert.equal(mode.directions.down.keycode, 0x081c);
});
test("logical Apply declares schema two, all five domains and storage format three", () => {
    const value = validateSnapshot(document());
    const metadata = candidateMetadataForBlob(value.profile, {actionAbiDigest: value.document.actionAbiDigest, viaGeneration: 3, viaDigest: 42});
    assert.equal(metadata.schemaMajor, 2); assert.equal(metadata.requestedDomains, 31); assert.equal(metadata.storeFormatVersion, 3);
    const frame = buildCandidateBeginRequest(1, metadata);
    assert.equal(frame[23], 3); assert.equal(frame.readUInt32LE(24), 3); assert.equal(frame.readUInt32LE(28), 42);
});

test("a cleared slot keeps its bindings, because the keyboard keeps its keycodes", () => {
    // The mode keycodes are a fixed registry in the firmware, and its runtime
    // refuses to activate a slot whose record is empty rather than misbehaving
    // (`noah_effective_pd_for_mask` returns nothing, and both activate and lock
    // bail out on that). So clearing a slot leaves an inert button on the
    // board, not a broken profile — and the validated profile counts what still
    // reaches the empty slot so the interface can say the key does nothing.
    const {draft, stage, caps} = fixture();
    const modelNow = () => buildDeviceModel({...draft.editingState({connected: true, selectedDeviceId: "board", capabilities: caps}), capabilities: caps});
    stage({type: "savePdMode", slot: 7, config: {kind: 1, name: "History", axis: 1, thresholdX: 30,
        directions: {left: {keycode: "LALT(KC_LEFT)"}, right: {keycode: "LALT(KC_RIGHT)"}}}});
    stage({type: "updateLayoutKeys", layers: [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "PD_SLOT_7"}]}]});

    stage({type: "clearPdMode", slot: 7});
    const cleared = validateSnapshot(draft.document);
    assert.equal(cleared.pdModes[7].kind, 0, "the slot is empty");
    assert.equal(cleared.danglingPdBindings[7], 1, "and the key that still reaches it is counted, not refused");
    assert.equal(cleared.document.layers[0][0] !== 0, true, "the button stays on the board");
    assert.equal(modelNow().qmkKeycodes.find(key => key.value === "PD_SLOT_7").label, "Slot 8 · hold (empty)",
        "and the keycode stays in the catalogue, described as empty rather than by a name that is gone");

    stage({type: "savePdMode", slot: 7, config: {kind: 1, name: "History", axis: 1, thresholdX: 30,
        directions: {left: {keycode: "LALT(KC_LEFT)"}, right: {keycode: "LALT(KC_RIGHT)"}}}});
    assert.equal(validateSnapshot(draft.document).danglingPdBindings[7], undefined,
        "configuring the slot again makes the same button live");
    assert.equal(modelNow().qmkKeycodes.find(key => key.value === "PD_SLOT_7").label, "History · hold");
});
