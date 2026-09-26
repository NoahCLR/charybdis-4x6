"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {revertUnits} = require("../../core/model/profile-revert");
const {profileReview} = require("../../core/model/profile-review");
const {fingerprint, summary, validateSnapshot} = require("../../core/model/portable-profile");
const {settingsEditorView} = require("../../core/model/settings-editor");
const {ProfileDraftSession} = require("../../core/session/profile-draft-session");
const {document: pdDocument} = require("../fixtures/pd-profile");

const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x61072732};
const snapshotOf = (document) => ({document, fingerprint: fingerprint(document), summary: summary(document), limits: {brightnessMax: 200}});

// A draft with one edit of every kind the review describes.
function edited() {
    const draft = new ProfileDraftSession(snapshotOf(pdDocument()), "board", capabilities);
    const stage = (message) => draft.stage({draftId: draft.id, draftRevision: draft.revision, ...message});
    const settings = (id, updates) => {
        const section = settingsEditorView(draft.current).sections.find((row) => row.id === id);
        return {type: "updateConfigDefaults", sectionId: id, expectedFingerprint: draft.current.fingerprint,
            fields: section.fields.map((field) => field.kind === "toggle" ? {macro: field.macro, enabled: updates[field.macro] ?? field.enabled} : {macro: field.macro, value: updates[field.macro] ?? field.value})};
    };
    stage({type: "updateLayoutKeys", layers: [{layer: "Layer 1", changes: [{layoutIndex: 3, keycode: "KC_B"}, {layoutIndex: 4, keycode: "KC_C"}]}]});
    stage({type: "updateViaMacro", keycode: "VIA_MACRO_2", payload: "hello"});
    stage({type: "updateViaMacro", keycode: "VIA_MACRO_5", name: "Greeting", expectedFingerprint: draft.current.fingerprint});
    stage(settings("keyTiming", {tapHoldTerm: "175", multiTapTerm: "190"}));
    stage({type: "retargetBehavior", keycode: "KC_ESCAPE", target: "KC_Q", expectedBase: draft.identity()});
    stage({type: "addCombo", inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: "50", holdTermMs: "200"});
    stage({type: "updateLayerColor", layer: "Layer 2", mode: "ALL_KEYS", h: "20", s: "200", v: "100"});
    stage({type: "updateRgbStages", stageEnableMask: validateSnapshot(draft.document).rgb.stageEnableMask ^ 1});
    stage({type: "addRgbLedGroup", group: {target: "layer", owner: "Layer 3", ledIndices: [1, 2, 3], h: "0", s: "0", v: "0"}});
    const slot = validateSnapshot(draft.document).pdModes[1];
    stage({type: "savePdMode", slot: 1, expectedBase: draft.identity(), config: {...slot, name: "Loudness", thresholdY: 90,
        directions: Object.fromEntries(Object.entries(slot.directions).map(([name, tap]) => [name, {...tap, keycode: String(tap.keycode)}])),
        diagonals: Object.fromEntries(Object.entries(slot.diagonals || {}).map(([name, tap]) => [name, {...tap, keycode: String(tap.keycode)}])),
        buttons: slot.buttons.map((button) => ({...button, tap: {...button.tap, keycode: String(button.tap.keycode)}}))}});
    return draft;
}
const describe = (items) => items.map((item) => JSON.stringify(item)).sort();

test("discarding one middle combo deletion restores the full packed table", () => {
    const seeded = new ProfileDraftSession(snapshotOf(pdDocument()), "board", capabilities);
    for (let id = 0; id < 7; id++) seeded.stage({draftId: seeded.id, draftRevision: seeded.revision, type: "addCombo",
        inputs: ["KC_A", `KC_${id + 1}`], output: `KC_${id + 1}`, termMs: "50", holdTermMs: "200"});
    const draft = new ProfileDraftSession(snapshotOf(seeded.document), "board", capabilities);
    draft.stage({draftId: draft.id, draftRevision: draft.revision, type: "saveCombo", id: 2,
        inputs: ["KC_A", "KC_3"], output: "KC_Z", termMs: "50"});
    draft.stage({draftId: draft.id, draftRevision: draft.revision, type: "deleteCombo", id: 2});
    const rows = profileReview(draft.base, draft.current);
    assert.deepEqual(rows.map(row => row.unit), ["combo:2"]);
    assert.deepEqual(draft.changes().map(row => row.unit), ["combo:2"]);
    assert.deepEqual(draft.steps().slice(-2).map(step => step.changes.map(row => row.unit)), [["combo:2"], ["combo:2"]]);
    const restored = revertUnits(draft.base, draft.current, new Set([rows[0].unit]), capabilities);
    assert.deepEqual(validateSnapshot(restored).combos, validateSnapshot(draft.base.document).combos);
    assert.deepEqual(profileReview(draft.base, snapshotOf(restored)), []);
    draft.discard(draft.revision, draft.changes()[0].group);
    assert.deepEqual(draft.changes(), []);
});

test("each unit goes back to the keyboard's value, and nothing else moves", () => {
    const draft = edited(), base = draft.base, current = draft.current;
    const rows = profileReview(base, current);
    const units = [...new Set(rows.map((row) => row.unit))];
    for (const kind of ["layout", "macro", "settings", "behavior", "combo", "rgb", "pd"]) {
        assert.ok(units.some((unit) => unit.startsWith(`${kind}:`)), `the fixture edits a ${kind} unit`);
    }
    for (const unit of units) {
        const document = revertUnits(base, current, new Set([unit]), capabilities);
        const left = profileReview(base, {...current, document, fingerprint: fingerprint(document)});
        assert.deepEqual(describe(left), describe(rows.filter((row) => row.unit !== unit)), unit);
    }
});

test("several units revert together, and every one of them back is the keyboard's profile", () => {
    const draft = edited(), base = draft.base, current = draft.current;
    const units = new Set(profileReview(base, current).map((row) => row.unit));
    const document = revertUnits(base, current, units, capabilities);
    const left = profileReview(base, {...current, document, fingerprint: fingerprint(document)});
    assert.ok(left.every((row) => row.unit === "profile"), "nothing described is left");
    assert.deepEqual(document.layers, validateSnapshot(base.document, capabilities).document.layers);
    assert.deepEqual(document.macros, validateSnapshot(base.document, capabilities).document.macros);
});

test("a unit this module does not know is refused rather than skipped", () => {
    const draft = edited();
    assert.throws(() => revertUnits(draft.base, draft.current, new Set(["mystery:1"]), capabilities), {code: "PROFILE_REVERT_UNSUPPORTED"});
});
