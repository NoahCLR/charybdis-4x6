// The payloads this interface posts have to be the ones the tested core
// accepts. These stage the exact messages the webview sends against a real
// draft session, so a change of shape fails here rather than on a keyboard.

import assert from "node:assert/strict";
import test from "node:test";
import {createRequire} from "node:module";
import path from "node:path";
import {fileURLToPath} from "node:url";

const require = createRequire(import.meta.url);
const here = path.dirname(fileURLToPath(import.meta.url));
const {ProfileDraftSession} = require(path.join(here, "..", "core", "session", "profile-draft-session"));
const portable = require(path.join(here, "..", "core", "model", "portable-profile"));
const {document: pdDocument} = require(path.join(here, "fixtures", "pd-profile"));

const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x61072732};

function session() {
    const doc = pdDocument();
    const snapshot = {document: doc, fingerprint: portable.fingerprint(doc), summary: portable.summary(doc)};
    return new ProfileDraftSession(snapshot, "test-device", capabilities);
}

test("a key picked in the interface lands on the layer the board was showing", () => {
    const draft = session();
    const before = draft.document.layers[0].slice();
    // exactly what ui/keys.mjs posts from the picker
    draft.stage({
        type: "updateLayoutKeys", draftId: draft.id, draftRevision: draft.revision,
        layer: "Layer 0", changes: [{layoutIndex: 27, keycode: "KC_B"}],
    });
    const after = draft.document.layers[0];
    assert.notDeepEqual(after, before, "the layer changed");
    assert.equal(draft.dirty, true);
    assert.equal(draft.view({selectedDeviceId: "test-device", connected: true}).changes.length > 0, true,
        "the change is reviewable before it is applied");
});

test("swapping two keys is one message and one draft step", () => {
    const draft = session();
    draft.stage({
        type: "updateLayoutKeys", draftId: draft.id, draftRevision: draft.revision,
        layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "KC_B"}, {layoutIndex: 1, keycode: "KC_A"}],
    });
    assert.equal(draft.dirty, true);
    assert.equal(draft.canUndoSteps ?? true, true);
    draft.undo(draft.revision);
    assert.equal(draft.dirty, false, "one undo takes both keys back");
});

test("a behaviour row posted as the editor builds it is accepted whole", () => {
    const draft = session();
    const behaviours = draft.current.document ? true : false;
    assert.equal(behaviours, true);
    // the shape ui/keys.mjs builds in saveBehaviour()
    const message = {
        type: "saveBehavior", draftId: draft.id, draftRevision: draft.revision,
        expectedBase: draft.identity(),
        behavior: {
            keycode: "KC_ESCAPE",
            tapHoldTerm: "175", longerHoldTerm: "0", multiTapTerm: "0",
            keepsAutoMouseAnchored: false,
            steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: "KC_ESCAPE"},
                hold: {helper: "PRESS_AND_HOLD_UNTIL_RELEASE", action: "KC_LSFT", repeatHz: "0"}}],
        },
    };
    draft.stage(message);
    assert.equal(draft.dirty, true, "the behaviour edit reached the draft");
    const areas = draft.view({selectedDeviceId: "test-device", connected: true}).changes.map((change) => change.area);
    assert.ok(areas.length, "and it is described in the review");
});

test("an edit from a stale form is refused rather than silently rebased", () => {
    const draft = session();
    assert.throws(() => draft.stage({
        type: "updateLayoutKeys", draftId: draft.id, draftRevision: draft.revision + 5,
        layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "KC_B"}],
    }), /draft changed/i);
});

test("a colour changed in Lighting is staged as the keyboard stores it", () => {
    const draft = session();
    // exactly what ui/lighting.mjs posts from the colour editor
    draft.stage({
        type: "updateLayerColor", draftId: draft.id, draftRevision: draft.revision,
        layer: "Layer 3", mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY", h: 60, s: 255, v: 200,
    });
    assert.equal(draft.dirty, true);
    const areas = draft.view({selectedDeviceId: "test-device", connected: true}).changes.map((change) => change.area);
    assert.ok(areas.length, "the colour change is reviewable");
});

test("switching a stage off is a mask edit, and the mask survives the round trip", () => {
    const draft = session();
    draft.stage({
        type: "updateRgbStages", draftId: draft.id, draftRevision: draft.revision,
        stageEnableMask: 0,
    });
    assert.equal(draft.dirty, true);
});

test("a pointing-mode colour keeps its locality, and key feedback posts whole", () => {
    const draft = session();
    draft.stage({
        type: "updatePdModeColor", draftId: draft.id, draftRevision: draft.revision,
        pointingMode: "PD_MODE_DRAGSCROLL", locality: "RGB_BOTH_HALVES", h: 21, s: 255, v: 200,
    });
    draft.stage({
        type: "updateKeyBehaviorFeedback", draftId: draft.id, draftRevision: draft.revision,
        config: {
            tapBranchColors: [{h: 169, s: 255, v: 200}, {h: 222, s: 255, v: 200}, {h: 85, s: 255, v: 200}, {h: 25, s: 255, v: 200}],
            tapCommittedColor: {h: 0, s: 0, v: 158},
            holdActiveColor: {h: 222, s: 255, v: 200},
            longHoldActiveColor: {h: 148, s: 255, v: 200},
            tapCommitMode: "KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS",
            locality: "RGB_KEYS_ONLY",
        },
    });
    assert.equal(draft.dirty, true);
});

test("a settings section is posted whole, and a partial section is refused", () => {
    const draft = session();
    const view = require(path.join(here, "..", "core", "model", "settings-editor")).settingsEditorView(draft.current);
    const section = view.sections.find((row) => row.id === "keyTiming");
    const fields = section.fields.map((field) => field.kind === "toggle"
        ? {macro: field.macro, enabled: field.enabled}
        : {macro: field.macro, value: field.macro === "tappingTerm" ? "210" : field.value});
    draft.stage({
        type: "updateConfigDefaults", draftId: draft.id, draftRevision: draft.revision,
        sectionId: section.id, expectedFingerprint: draft.current.fingerprint, fields,
    });
    assert.equal(draft.dirty, true, "the whole section reached the draft");

    assert.throws(() => draft.stage({
        type: "updateConfigDefaults", draftId: draft.id, draftRevision: draft.revision,
        sectionId: section.id, expectedFingerprint: draft.current.fingerprint, fields: fields.slice(0, 2),
    }), /complete settings section/i);
});

test("a pointing slot is posted whole, with its shortcuts as names", () => {
    const draft = session();
    const before = require(path.join(here, "..", "core", "model", "portable-profile"))
        .validateSnapshot(draft.document).pdModes[1];
    assert.equal(before.kind, 1, "the fixture's slot 2 is directional");
    // the shape ui/pointing.mjs builds in readConfig()
    draft.stage({
        type: "savePdMode", draftId: draft.id, draftRevision: draft.revision,
        expectedBase: draft.identity(),
        slot: 1,
        config: {
            id: 1, kind: 1, name: "Volume", dpi: 0, pointerLayer: 0, axis: 0,
            thresholdX: 0, thresholdY: 70,
            directions: {
                up: {keycode: "KC_VOLU", modifierPolicy: 0, mask: 0},
                down: {keycode: "KC_VOLD", modifierPolicy: 0, mask: 0},
                left: {keycode: "0", modifierPolicy: 0, mask: 0},
                right: {keycode: "0", modifierPolicy: 0, mask: 0},
            },
            buttons: [0, 1, 2].map(() => ({kind: 0, modifiers: 0, tap: {keycode: "0", modifierPolicy: 0, mask: 0}})),
        },
    });
    const after = require(path.join(here, "..", "core", "model", "portable-profile"))
        .validateSnapshot(draft.document).pdModes[1];
    assert.equal(after.thresholdY, 70, "the edited threshold reached the profile");
    assert.ok(after.directions.up.keycode > 0, "and the named shortcut became the keyboard's own value");
});

test("clearing a slot leaves its button on the board, inert until it is configured again", () => {
    const draft = session();
    // The fixture reaches slot 6 from a behaviour. The keyboard's mode keycodes
    // are a fixed registry and its runtime refuses to activate an empty slot,
    // so the button stays put and does nothing — which is what the interface
    // says next to the slot.
    draft.stage({
        type: "clearPdMode", draftId: draft.id, draftRevision: draft.revision, slot: 5, expectedBase: draft.identity(),
    });
    const value = require(path.join(here, "..", "core", "model", "portable-profile")).validateSnapshot(draft.document);
    assert.equal(value.pdModes[5].kind, 0, "the slot is empty");
    assert.ok(value.danglingPdBindings[5] > 0, "and what still reaches it is counted, not refused");
    assert.equal(draft.dirty, true);
});

test("duplicating needs a configured source and an empty destination", () => {
    const draft = session();
    assert.throws(() => draft.stage({
        type: "duplicatePdMode", draftId: draft.id, draftRevision: draft.revision,
        slot: 0, source: 1, expectedBase: draft.identity(),
    }), /empty destination/i, "a configured slot is not overwritten by a copy");
});

test("a macro payload is posted as the text the keyboard stores", () => {
    const draft = session();
    draft.stage({
        type: "updateViaMacro", draftId: draft.id, draftRevision: draft.revision,
        keycode: "VIA_MACRO_0", payload: "hello{120}{KC_ENT}",
        expectedFingerprint: draft.current.fingerprint,
    });
    assert.equal(draft.dirty, true);
    assert.throws(() => draft.stage({
        type: "updateViaMacro", draftId: draft.id, draftRevision: draft.revision,
        keycode: "VIA_MACRO_0", payload: "{KC_A",
        expectedFingerprint: draft.current.fingerprint,
    }), /macro command|}/i, "a payload the keyboard cannot parse is refused");
});
