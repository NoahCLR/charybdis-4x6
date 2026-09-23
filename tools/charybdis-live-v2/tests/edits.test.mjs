// The payloads this interface posts have to be the ones the tested core
// accepts. Every screen builds its messages with webview/view/edits.mjs (and
// pointing records with view/pointing-config.mjs); these tests stage exactly
// what those builders return against a real draft session, so a change of
// message shape fails here rather than on a keyboard.

import assert from "node:assert/strict";
import test from "node:test";
import {createRequire} from "node:module";
import path from "node:path";
import {fileURLToPath} from "node:url";
import * as edits from "../webview/view/edits.mjs";
import {KIND, SCROLL_STARTER, readConfig, startingRecord} from "../webview/view/pointing-config.mjs";

const require = createRequire(import.meta.url);
const here = path.dirname(fileURLToPath(import.meta.url));
const {ProfileDraftSession} = require(path.join(here, "..", "core", "session", "profile-draft-session"));
const portable = require(path.join(here, "..", "core", "model", "portable-profile"));
const {settingsEditorView} = require(path.join(here, "..", "core", "model", "settings-editor"));
const {buildDeviceModel} = require(path.join(here, "..", "core", "session", "device-model"));
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require(path.join(here, "..", "core", "protocol", "via-layout-v1"));
const {RGB_LOCALITIES} = require(path.join(here, "..", "core", "schema", "rgb-domain-v1"));
const {document: pdDocument} = require(path.join(here, "fixtures", "pd-profile"));

const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x61072732};

function session() {
    const doc = pdDocument();
    const snapshot = {document: doc, fingerprint: portable.fingerprint(doc), summary: portable.summary(doc), limits: {brightnessMax: 200}};
    return new ProfileDraftSession(snapshot, "test-device", capabilities);
}
// The host adds the draft id and revision to every message the webview posts.
const stage = (draft, message) => draft.stage({draftId: draft.id, draftRevision: draft.revision, ...message});
const decoded = (draft) => portable.validateSnapshot(draft.document);
const slotOf = (layoutIndex) => CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex][0] * 6 + CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex][1];
const reviewAreas = (draft) => draft.view({selectedDeviceId: "test-device", connected: true}).changes.map((change) => change.area);

// ── layout keys ─────────────────────────────────────────────────────────

test("a key picked in the interface lands at its position on the layer the board was showing", () => {
    const draft = session();
    const before = draft.document.layers.map((layer) => layer.slice());
    stage(draft, edits.setKey("Layer 3", 27, "KC_B"));
    assert.equal(draft.document.layers[3][slotOf(27)], 0x05, "KC_B at matrix row/column of layout index 27");
    draft.document.layers.forEach((layer, index) => {
        const changed = layer.filter((code, slot) => code !== before[index][slot]).length;
        assert.equal(changed, index === 3 ? 1 : 0, `layer ${index}`);
    });
    assert.deepEqual(reviewAreas(draft), ["Layout"], "the change is reviewable before it is applied");
});

test("Delete stores the key as transparent, and nothing else moves", () => {
    const draft = session();
    stage(draft, edits.setKey("Layer 2", 27, "KC_B"));
    const before = draft.document.layers[2].slice();
    stage(draft, edits.clearKey("Layer 2", 27));
    const after = draft.document.layers[2];
    assert.deepEqual(after.map((code, slot) => code !== before[slot] ? slot : -1).filter((slot) => slot >= 0), [slotOf(27)]);
    assert.equal(after[slotOf(27)], 1, "KC_TRANSPARENT");
});

test("a modifier around a macro or pointing key is refused, not stored as another keycode", () => {
    const draft = session();
    const before = draft.document.layers[0].slice();
    // the picker builds Cmd + VIA macro 3 as G(VIA_MACRO_3)
    const picked = edits.pickerExpression({keys: ["VIA_MACRO_3"], mods: ["Cmd"]});
    assert.equal(picked, "G(VIA_MACRO_3)");
    for (const keycode of [picked, "C(DRAGSCROLL)", "LOCK_LAYER(8)", "LT(60, 0x00)"]) {
        assert.throws(() => stage(draft, edits.setKey("Layer 0", 3, keycode)), /Cannot represent/, keycode);
    }
    assert.deepEqual(draft.document.layers[0], before);
    assert.equal(draft.dirty, false);
});

test("swapping two keys is one message and one draft step", () => {
    const draft = session();
    stage(draft, edits.setKey("Layer 1", 0, "KC_A"));
    stage(draft, edits.setKey("Layer 1", 1, "KC_B"));
    const before = draft.document.layers[1].slice();
    stage(draft, edits.swapKeys("Layer 1", {layoutIndex: 0, keycode: "KC_A"}, {layoutIndex: 1, keycode: "KC_B"}));
    assert.equal(draft.document.layers[1][slotOf(0)], 0x05);
    assert.equal(draft.document.layers[1][slotOf(1)], 0x04);
    draft.undo(draft.revision);
    assert.deepEqual(draft.document.layers[1], before, "one undo takes both keys back");
});

test("an edit from a stale form is refused rather than silently rebased", () => {
    const draft = session();
    assert.throws(() => draft.stage({draftId: draft.id, draftRevision: draft.revision + 5, ...edits.setKey("Layer 0", 0, "KC_B")}), /draft changed/i);
});

test("the picker posts layer keys by index, and wraps the key in modifiers and LT", () => {
    const draft = session();
    assert.equal(edits.pickerExpression({keys: ["MO(1)"]}), "MO(1)");
    assert.equal(edits.pickerExpression({keys: ["KC_A"], mods: ["Ctrl", "Shift"]}), "S(C(KC_A))");
    assert.equal(edits.pickerExpression({keys: ["KC_A"], layerTap: "2"}), "LT(2, KC_A)");
    assert.equal(edits.pickerExpression({keys: ["KC_A", "KC_B"], mode: "list"}), "KC_A, KC_B");
    assert.equal(edits.pickerExpression({keys: []}), "");
    for (const keycode of ["MO(1)", "LOCK_LAYER(1)", edits.pickerExpression({keys: ["KC_A"], layerTap: "2"}),
        edits.pickerExpression({keys: ["KC_A"], mods: ["Ctrl", "Shift"]})]) {
        stage(draft, edits.setKey("Layer 0", 5, keycode));
    }
    assert.equal(draft.document.layers[0][slotOf(5)], 0x0304, "C(S(KC_A))");
});

// ── keyboard shortcuts ──────────────────────────────────────────────────

test("undo and redo belong to the draft except inside text, a recording or a prompt", () => {
    const key = (name, mods = {}) => ({key: name, metaKey: false, ctrlKey: false, altKey: false, shiftKey: false, ...mods});
    assert.equal(edits.historyAction(key("z", {metaKey: true})), "undo");
    assert.equal(edits.historyAction(key("Z", {metaKey: true, shiftKey: true})), "redo");
    assert.equal(edits.historyAction(key("y", {ctrlKey: true})), "redo");
    assert.equal(edits.historyAction(key("z")), null, "no modifier");
    assert.equal(edits.historyAction(key("z", {metaKey: true, altKey: true})), null);
    assert.equal(edits.historyAction(key("z", {metaKey: true}), {editingText: true}), null, "a text field keeps its own undo");
    assert.equal(edits.historyAction(key("z", {metaKey: true}), {busy: true}), null);
});

test("copy, paste and delete on the board are told apart from everything else", () => {
    const key = (name, mods = {}) => ({key: name, metaKey: false, ctrlKey: false, altKey: false, shiftKey: false, ...mods});
    assert.equal(edits.keyAction(key("c", {metaKey: true})), "copy");
    assert.equal(edits.keyAction(key("v", {ctrlKey: true})), "paste");
    assert.equal(edits.keyAction(key("Delete")), "clear");
    assert.equal(edits.keyAction(key("Backspace")), "clear");
    assert.equal(edits.keyAction(key("Backspace", {metaKey: true})), null);
    assert.equal(edits.keyAction(key("v", {metaKey: true, shiftKey: true})), null);
    assert.equal(edits.keyAction(key("c")), null);
});

// ── key behaviours ──────────────────────────────────────────────────────

test("a behaviour row posted as the editor builds it is accepted whole", () => {
    const draft = session();
    // the fixture already has an Escape row; the editor saves it whole
    const behaviour = {keycode: "KC_ESCAPE", tapHoldTerm: 0, longerHoldTerm: 0, multiTapTerm: 0, keepsAutoMouseAnchored: false,
        steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: "KC_ESCAPE"}}]};
    const change = {tapCount: 0, kind: "hold", branch: edits.cellBranch("hold", {action: "KC_LSFT", helper: "PRESS_AND_HOLD_UNTIL_RELEASE"})};
    stage(draft, edits.saveBehaviour(behaviour, {terms: {tapHoldTerm: "175", longerHoldTerm: ""}, change}, draft.identity()));
    const row = decoded(draft).behaviors.rows.find((entry) => entry.target.kind === 1 && entry.target.operand === 0x29);
    assert.equal(row.tapHoldTerm, 175);
    assert.equal(row.longerHoldTerm, 0, "an empty timing field means the keyboard default");
    assert.equal(row.steps[0].hold.mode, 1);
    assert.ok(reviewAreas(draft).includes("Behaviours"));
});

test("choosing repeat while held posts a rate the keyboard accepts", () => {
    assert.deepEqual(edits.cellBranch("hold", {action: "KC_RIGHT", helper: "REPEAT_WHILE_HELD", repeatHz: ""}),
        {helper: "REPEAT_WHILE_HELD", action: "KC_RIGHT", repeatHz: edits.DEFAULT_REPEAT_HZ});
    assert.equal(edits.cellBranch("hold", {action: "KC_RIGHT", helper: "TAP_AT_HOLD_THRESHOLD", repeatHz: "30"}).repeatHz, "0");
    assert.equal(edits.cellBranch("tap", {action: " "}), null, "an empty cell removes the tier");
    const draft = session();
    stage(draft, edits.addBehaviour("KC_Q", draft.identity()));
    const behaviour = {keycode: "KC_Q", steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: "KC_Q"}}]};
    stage(draft, edits.saveBehaviour(behaviour, {change: {tapCount: 0, kind: "hold",
        branch: edits.cellBranch("hold", {action: "KC_RIGHT", helper: "REPEAT_WHILE_HELD", repeatHz: ""})}}, draft.identity()));
    const row = decoded(draft).behaviors.rows.find((entry) => entry.target.operand === 0x14);
    assert.equal(row.steps[0].hold.repeatHz, Number(edits.DEFAULT_REPEAT_HZ));
});

test("a behaviour row is added for a key without one, and deleted again", () => {
    const draft = session();
    const rows = () => decoded(draft).behaviors.rows.length;
    const before = rows();
    stage(draft, edits.addBehaviour("KC_Q", draft.identity()));
    assert.equal(rows(), before + 1);
    stage(draft, edits.deleteBehaviour("KC_Q", draft.identity()));
    assert.equal(rows(), before);
});

test("a behaviour moves to another key, and overwrites or swaps with one already there", () => {
    const draft = session();
    const rows = () => decoded(draft).behaviors.rows.length;
    stage(draft, edits.addBehaviour("KC_Q", draft.identity()));
    stage(draft, edits.addBehaviour("KC_W", draft.identity()));
    const before = rows();
    stage(draft, edits.retargetBehaviour("KC_Q", "KC_E", draft.identity()));
    assert.equal(rows(), before, "moving to a free key keeps the row count");
    assert.throws(() => stage(draft, edits.retargetBehaviour("KC_E", "KC_W", draft.identity())), /overwrite it or swap/);
    stage(draft, edits.retargetBehaviour("KC_E", "KC_W", draft.identity(), "swap"));
    assert.equal(rows(), before);
    stage(draft, edits.retargetBehaviour("KC_W", "KC_E", draft.identity(), "overwrite"));
    assert.equal(rows(), before - 1, "overwriting drops the row that was there");
    draft.undo(draft.revision);
    assert.equal(rows(), before, "one undo brings the overwritten row back");
});

// ── combos ──────────────────────────────────────────────────────────────

test("the first combo on a keyboard with none carries a hold threshold the device reported", () => {
    const draft = session();
    stage(draft, edits.comboMessage(null, {inputs: ["KC_D", "KC_F"], output: "KC_ESCAPE", termMs: "50", holdTermMs: "200"}));
    const combos = decoded(draft).combos;
    assert.equal(combos.length, 1);
    assert.equal(combos[0].holdTermMs, 200);
    const fresh = session();
    assert.throws(() => stage(fresh, edits.comboMessage(null, {inputs: ["KC_J", "KC_K"], output: "KC_TAB", termMs: "50"})),
        /Hold threshold/, "an absent threshold is refused, so the interface must send one");
});

test("the shared hold threshold is its own message, and reaches every combo", () => {
    const draft = session();
    for (const [inputs, output] of [[["KC_D", "KC_F"], "KC_ESCAPE"], [["KC_J", "KC_K"], "KC_TAB"]]) {
        stage(draft, edits.comboMessage(null, {inputs, output, termMs: "50", holdTermMs: "200"}));
    }
    stage(draft, edits.comboHoldTerm("275"));
    assert.deepEqual(decoded(draft).combos.map((row) => row.holdTermMs), [275, 275],
        "QMK keeps one threshold for all combos, so the edit lands on every row");
});

test("a combo is edited and deleted by the id the interface holds", () => {
    const draft = session();
    stage(draft, edits.comboMessage(null, {inputs: ["KC_D", "KC_F"], output: "KC_ESCAPE", termMs: "50", holdTermMs: "200"}));
    stage(draft, edits.comboMessage(0, {inputs: ["KC_D", "KC_F"], output: " KC_TAB ", termMs: "40", holdTermMs: "200", mustHold: true, ordered: true}));
    const saved = decoded(draft).combos[0];
    assert.equal(saved.termMs, 40);
    assert.equal(saved.output.operand, 0x2b, "the output is trimmed and encoded");
    assert.equal(saved.mustHold, true);
    assert.equal(saved.ordered, true);
    stage(draft, edits.deleteCombo(0));
    assert.deepEqual(decoded(draft).combos, []);
});

// ── lighting ────────────────────────────────────────────────────────────

test("a colour changed in Lighting is staged as the keyboard stores it", () => {
    const draft = session();
    stage(draft, edits.layerColour("Layer 3", "KEYS_MAPPED_ON_THIS_LAYER_ONLY", {h: 60, s: 255, v: 200}));
    const row = decoded(draft).rgb.layerColors.find((entry) => entry.layerId === 3);
    assert.deepEqual(row.color, {h: 60, s: 255, v: 200});
    assert.ok(reviewAreas(draft).includes("RGB"));
    const tooBright = session();
    assert.throws(() => stage(tooBright, edits.layerColour("Layer 3", "KEYS_MAPPED_ON_THIS_LAYER_ONLY", {h: 60, s: 255, v: 201})),
        /Brightness must be between 0 and 200/);
});

test("switching a stage off is a mask edit that lands as that mask", () => {
    const draft = session();
    const mask = decoded(draft).rgb.stageEnableMask;
    const bit = 1;
    stage(draft, edits.rgbStages(mask, bit, false));
    assert.equal(decoded(draft).rgb.stageEnableMask, mask & ~bit);
    stage(draft, edits.rgbStages(mask & ~bit, bit, true));
    assert.equal(decoded(draft).rgb.stageEnableMask, mask | bit);
});

test("a pointing-mode colour keeps the locality posted beside it", () => {
    const draft = session();
    stage(draft, edits.pdModeColour("PD_MODE_DRAGSCROLL", "RGB_BOTH_HALVES", {h: 21, s: 255, v: 180}));
    const row = decoded(draft).rgb.pdModeColors.find((entry) => entry.pdModeId === 0);
    assert.equal(row.locality, RGB_LOCALITIES.RGB_BOTH_HALVES);
    assert.deepEqual(row.color, {h: 21, s: 255, v: 180});
});

test("key feedback posts the whole record with one colour or policy replaced", () => {
    const draft = session();
    // the screen redraws from the new model after each edit, so each message
    // is built from the record as it stands then
    const feedback = () => buildDeviceModel(draft.editingState({selectedDeviceId: "test-device"})).rgb.keyBehaviorFeedback;
    stage(draft, edits.keyFeedback(feedback(), "holdActiveColor", {h: 99, s: 255, v: 150}));
    stage(draft, edits.keyFeedback(feedback(), "branch:0", {h: 12, s: 200, v: 100}));
    const stored = decoded(draft).rgb.keyFeedback;
    assert.deepEqual(stored.holdActiveColor, {h: 99, s: 255, v: 150});
    assert.deepEqual(stored.tapBranchColors[0], {h: 12, s: 200, v: 100});
    stage(draft, edits.keyFeedback(feedback(), null, null, {locality: "RGB_BOTH_HALVES"}));
    assert.deepEqual(decoded(draft).rgb.keyFeedback.holdActiveColor, {h: 99, s: 255, v: 150}, "and earlier edits survive the next one");
    assert.equal(decoded(draft).rgb.keyFeedback.locality, RGB_LOCALITIES.RGB_BOTH_HALVES);
});

test("auto-mouse fade and combo feedback post their colour beside their mode", () => {
    const draft = session();
    stage(draft, edits.automouseFade("END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW", {h: 128, s: 255, v: 180}));
    stage(draft, edits.comboFeedback("RGB_KEYS_ONLY", {h: 32, s: 255, v: 200}));
    const rgb = decoded(draft).rgb, original = decoded(session()).rgb;
    assert.equal(rgb.automouseFade.endColor.h, 128);
    assert.notEqual(rgb.automouseFade.mode, original.automouseFade.mode, "the mode posted beside the colour is the one that lands");
    assert.equal(rgb.comboFeedback.color.h, 32);
    assert.equal(rgb.comboFeedback.locality, RGB_LOCALITIES.RGB_KEYS_ONLY);
});

test("LED group rows land in every table, and groups renumber when one is deleted", () => {
    const draft = session();
    const rgb = () => decoded(draft).rgb;
    const ledsOf = (id) => rgb().groups.find((group) => group.id === id).leds;
    const startCount = rgb().groups.length;
    const colour = {h: 10, s: 255, v: 100};
    for (const [target, owner] of [["layer", "Layer 2"], ["pdMode", "PD_MODE_VOLUME"], ["combo", "ignored"], ["keyBehavior", "KEY_FEEDBACK_GROUP_HOLD_ACTIVE"]]) {
        stage(draft, edits.ledRow({target, owner, source: ""}, [1, 2], colour));
    }
    assert.equal(rgb().groups.length, startCount + 1, "rows with the same LEDs share one group");
    assert.equal(edits.ledRow({target: "combo", owner: "x", source: ""}, [1], colour).group.owner, undefined, "a combo row has no owner");

    for (const leds of [[40], [41], [42]]) stage(draft, edits.saveLedGroup(leds));
    const byLeds = (leds) => rgb().groups.find((group) => group.leds.join() === leds.join()).id;
    stage(draft, edits.ledRow({target: "layer", owner: "Layer 3", source: `Group ${byLeds([42])}`}, [], {h: 1, s: 1, v: 1}));
    stage(draft, edits.deleteLedGroup(`Group ${byLeds([40])}`));
    const ids = rgb().groups.map((group) => group.id);
    assert.deepEqual(ids, ids.map((_, index) => index), "ids stay consecutive from zero");
    const layerRow = rgb().layerGroupRows.find((row) => row.selector === 3);
    assert.deepEqual(ledsOf(layerRow.groupId), [42], "a row still paints the LEDs it pointed at");
    const before = rgb().layerGroupRows.length;
    stage(draft, edits.deleteLedRow("layer", before - 1));
    assert.equal(rgb().layerGroupRows.length, before - 1);
});

// ── macros ──────────────────────────────────────────────────────────────

test("a macro payload is posted as the text the keyboard stores", () => {
    const draft = session();
    stage(draft, edits.macroMessage("VIA_MACRO_0", "hello{120}{KC_ENT}", draft.current.fingerprint));
    assert.equal(draft.dirty, true);
    assert.ok(reviewAreas(draft).includes("Macros"));
    assert.throws(() => stage(draft, edits.macroMessage("VIA_MACRO_0", "{KC_A", draft.current.fingerprint)),
        /macro command|}/i, "a payload the keyboard cannot parse is refused");
});

test("a recorded take appends to what the payload held, with no pause before the first key", () => {
    let payload = "{KC_H}";
    payload = edits.recordedPayload(payload, {keycode: "KC_A", type: "keydown", gap: 900, captured: false, threshold: 30, round: 10});
    assert.equal(payload, "{KC_H}{KC_A}", "the time it took to start typing is not part of the macro");
    payload = edits.recordedPayload(payload, {keycode: "KC_B", type: "keydown", gap: 124, captured: true, threshold: 30, round: 10});
    assert.equal(payload, "{KC_H}{KC_A}{120}{KC_B}");
    payload = edits.recordedPayload(payload, {keycode: "KC_C", type: "keydown", gap: 20, captured: true, threshold: 30, round: 10});
    assert.equal(payload, "{KC_H}{KC_A}{120}{KC_B}{KC_C}", "gaps under the threshold are dropped");
    assert.equal(edits.recordedPayload("", {keycode: "KC_LSFT", type: "keyup", gap: 5, captured: true, threshold: 30, explicit: true}), "{-KC_LSFT}");
    const draft = session();
    stage(draft, edits.macroMessage("VIA_MACRO_1", payload, draft.current.fingerprint));
    assert.equal(draft.dirty, true);
});

// ── pointing modes ──────────────────────────────────────────────────────

// A form as ui/pointing.mjs registers it: one reader per drawn field.
const formOf = (values) => Object.fromEntries(Object.entries(values).map(([key, value]) => [key, () => value]));

test("a pointing slot is posted whole, with its shortcuts as names", () => {
    const draft = session();
    const slot = decoded(draft).pdModes[1];
    assert.equal(slot.kind, 1, "the fixture's slot 2 is directional");
    const config = readConfig({...slot, directions: {}}, formOf({kind: KIND.DIRECTIONAL, name: "Volume", dpi: "0", thresholdX: "0", thresholdY: "70",
        "dir:up": "KC_VOLU", "dir:down": "KC_VOLD"}));
    stage(draft, edits.pdMode(1, config, draft.identity()));
    const after = decoded(draft).pdModes[1];
    assert.equal(after.thresholdY, 70, "the edited threshold reached the profile");
    assert.ok(after.directions.up.keycode > 0, "and the named shortcut became the keyboard's own value");
});

test("a directional slot switched to scrolling posts the starter tuning, which the draft accepts", () => {
    const draft = session();
    const slot = startingRecord(decoded(draft).pdModes[1], KIND.SCROLLING);
    assert.deepEqual(slot.scroll, SCROLL_STARTER);
    stage(draft, edits.pdMode(1, readConfig(slot, formOf({kind: KIND.SCROLLING, name: "Volume", dpi: "0"})), draft.identity()));
    const after = decoded(draft).pdModes[1];
    assert.equal(after.kind, 2);
    assert.equal(after.scroll.divisorV, SCROLL_STARTER.divisorV);
});

test("clearing a slot leaves its button on the board, inert until it is configured again", () => {
    const draft = session();
    // The fixture reaches slot 6 from a behaviour. The keyboard's mode keycodes
    // are a fixed registry and its runtime refuses to activate an empty slot,
    // so the button stays put and does nothing — which is what the interface
    // says next to the slot.
    stage(draft, edits.clearPdMode(5, draft.identity()));
    const value = decoded(draft);
    assert.equal(value.pdModes[5].kind, 0, "the slot is empty");
    assert.ok(value.danglingPdBindings[5] > 0, "and what still reaches it is counted, not refused");
});

test("duplicating needs a configured source and an empty destination", () => {
    const draft = session();
    assert.throws(() => stage(draft, edits.duplicatePdMode(0, 1, draft.identity())), /empty destination/i,
        "a configured slot is not overwritten by a copy");
    stage(draft, edits.clearPdMode(5, draft.identity()));
    stage(draft, edits.duplicatePdMode(5, 1, draft.identity()));
    assert.equal(decoded(draft).pdModes[5].kind, decoded(draft).pdModes[1].kind);
});

// ── settings ────────────────────────────────────────────────────────────

test("a settings section is posted whole, and a partial section is refused", () => {
    const draft = session();
    const section = settingsEditorView(draft.current).sections.find((row) => row.id === "keyTiming");
    const message = edits.settingsSection(section, (field) => field.macro === "tappingTerm" ? "210" : undefined, draft.current.fingerprint);
    assert.equal(message.fields.length, section.fields.length, "undrawn fields travel with the value that was read");
    stage(draft, message);
    assert.equal(settingsEditorView(draft.current).timing.tappingTerm, "210");
    assert.throws(() => stage(draft, {...message, expectedFingerprint: draft.current.fingerprint, fields: message.fields.slice(0, 2)}),
        /complete settings section/i);
});

test("behaviour timing defaults follow the draft's Key Timing, and its undo", () => {
    const draft = session();
    // what extension.js builds the webview's model from while a draft is open
    const timing = () => buildDeviceModel(draft.editingState({selectedDeviceId: "test-device"})).behaviorTimingDefaults;
    const before = timing().tapHoldTerm;
    assert.match(before, /^\d+$/, "the keyboard reports its default");
    const section = settingsEditorView(draft.current).sections.find((row) => row.id === "keyTiming");
    const next = String(Number(before) + 35);
    stage(draft, edits.settingsSection(section, (field) => field.macro === "tapHoldTerm" ? next : undefined, draft.current.fingerprint));
    assert.equal(timing().tapHoldTerm, next, "the behaviour editor's note reads the drafted default");
    draft.undo(draft.revision);
    assert.equal(timing().tapHoldTerm, before, "and undo puts it back");
});
