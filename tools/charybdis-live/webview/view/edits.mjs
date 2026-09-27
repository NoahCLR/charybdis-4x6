// Every edit the interface posts, built from plain values.
//
// The screens gather what the person chose and call these; the host stages
// what they return against the draft. Keeping the builders pure is what lets
// tests/edits.test.mjs stage the exact messages the screens send, so a change
// of shape fails there rather than on a keyboard.

import {hsv} from "../lib/colour.mjs";

// ── layout keys ─────────────────────────────────────────────────────────

export const layoutKeys = (layer, changes) => ({type: "updateLayoutKeys", layer, changes});
export const setKey = (layer, layoutIndex, keycode) => layoutKeys(layer, [{layoutIndex, keycode}]);
// Delete and Backspace leave a key transparent, so the layer below answers.
export const clearKey = (layer, layoutIndex) => setKey(layer, layoutIndex, "KC_TRANSPARENT");
// A swap is one message, so one undo takes both keys back.
export const swapKeys = (layer, from, to) => layoutKeys(layer, [
    {layoutIndex: from.layoutIndex, keycode: to.keycode},
    {layoutIndex: to.layoutIndex, keycode: from.keycode},
]);

// ── key behaviours ──────────────────────────────────────────────────────

export const TIER_FIELDS = {tap: "tap", hold: "hold", long: "longHold"};
export const DEFAULT_REPEAT_HZ = "20";

export const addBehaviour = (keycode, identity) => ({
    type: "addBehavior", expectedBase: identity,
    behavior: {keycode, steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: keycode}}]},
});
export const deleteBehaviour = (keycode, identity) => ({type: "deleteBehavior", keycode, expectedBase: identity});
export const retargetBehaviour = (keycode, target, identity, conflict) =>
    ({type: "retargetBehavior", keycode, target, expectedBase: identity, ...(conflict ? {conflict} : {})});

// One cell of the grid as the editor holds it. A tap tier has no helper; the
// repeat rate only means something for "repeat while held", where it must be
// 1–100 Hz, so an empty field posts the default rather than 0.
export function cellBranch(kind, {action, helper, repeatHz}) {
    const sends = String(action ?? "").trim();
    if (!sends) return null;
    if (kind === "tap") return {helper: "TAP_SENDS", action: sends};
    return {helper, action: sends, repeatHz: helper === "REPEAT_WHILE_HELD" ? String(repeatHz ?? "").trim() || DEFAULT_REPEAT_HZ : "0"};
}

// What one edit in the cell editor does. A cell that sends nothing has nothing
// the keyboard can store, so choosing how it runs before what it sends is held
// by the editor rather than posted; the choice then goes out with the first
// action. A cell that already holds a branch posts every edit, and emptying it
// removes the tier.
export function cellEdit(kind, {stored, action, helper, repeatHz}) {
    const branch = cellBranch(kind, {action, helper, repeatHz});
    if (branch || stored) return {branch};
    return {pending: kind === "tap" ? null : {helper, repeatHz}};
}

// One behaviour row, posted whole: the form is the row. `terms` are the
// timing fields as typed (empty means the keyboard default, stored as 0),
// `change` replaces one cell, and every other cell is carried as read.
export function saveBehaviour(behaviour, {terms = {}, anchored, change} = {}, identity) {
    const term = (name) => terms[name] === undefined ? String(behaviour[name] ?? "0") : String(terms[name]).trim() || "0";
    const sourceSteps = new Map((behaviour.steps || []).map((step) => [step.tapCount, step]));
    if (change && !sourceSteps.has(change.tapCount)) sourceSteps.set(change.tapCount, {tapCount: change.tapCount});
    const steps = [...sourceSteps.values()].sort((left, right) => left.tapCount - right.tapCount).map((step) => {
        const next = {tapCount: step.tapCount};
        for (const [kind, field] of Object.entries(TIER_FIELDS)) {
            const branch = change && change.tapCount === step.tapCount && change.kind === kind ? change.branch : step[field];
            if (!branch) continue;
            next[field] = kind === "tap"
                ? {helper: "TAP_SENDS", action: branch.action}
                : {helper: branch.helper, action: branch.action, repeatHz: branch.repeatHz || "0"};
        }
        return next;
    }).filter((step) => step.tap || step.hold || step.longHold);
    return {
        type: "saveBehavior",
        expectedBase: identity,
        behavior: {
            keycode: behaviour.keycode,
            tapHoldTerm: term("tapHoldTerm"),
            longerHoldTerm: term("longerHoldTerm"),
            multiTapTerm: term("multiTapTerm"),
            keepsAutoMouseAnchored: anchored ?? behaviour.keepsAutoMouseAnchored,
            steps,
        },
    };
}

// ── combos ──────────────────────────────────────────────────────────────

// A combo follows the default window unless it has its own: followsDefault,
// or an empty window, says it follows.
export function comboMessage(id, {output, inputs, termMs, followsDefault, holdTermMs, mustHold, mustTap, ordered}) {
    const payload = {output: String(output ?? "").trim(), inputs, termMs, followsDefault: Boolean(followsDefault), holdTermMs, mustHold: Boolean(mustHold), mustTap: Boolean(mustTap), ordered: Boolean(ordered)};
    return id === null || id === undefined ? {type: "addCombo", ...payload} : {type: "saveCombo", id, ...payload};
}
export const deleteCombo = (id) => ({type: "deleteCombo", id});
export const comboHoldTerm = (holdTermMs, identity) => ({type: "updateComboHoldTerm", holdTermMs, expectedBase: identity});
export const comboDefaultTerm = (defaultTermMs, identity) => ({type: "updateComboDefaultTerm", defaultTermMs, expectedBase: identity});
// QMK keeps one hold threshold for every combo. A keyboard that stores it
// once always has one; an older one stores it on each combo, so a first combo
// there starts from the keyboard's tapping term, which is QMK's own default
// for COMBO_HOLD_TERM and the value that firmware runs while it has none.
export const comboHoldTermValue = (model, written) => String(String(written ?? "").trim() || (model?.comboReadback?.holdTermMs
    ?? model?.behaviorTimingDefaults?.tappingTerm ?? "")).trim();
// The window a new combo, or one that follows the default, runs with; empty
// on a keyboard that has no default.
export const comboDefaultTermValue = (model) => String(model?.comboReadback?.defaultTermMs ?? "");

// ── lighting ────────────────────────────────────────────────────────────

export const hsvPayload = (colour) => {
    const [h, s, v] = hsv(colour);
    return {h, s, v};
};
export const rgbStages = (mask, bit, on) => ({type: "updateRgbStages", stageEnableMask: on ? mask | bit : mask & ~bit});
export const layerColour = (layer, mode, colour) => ({type: "updateLayerColor", layer, mode, ...hsvPayload(colour)});
export const automouseFade = (mode, colour) => ({type: "updateAutomouseFade", mode, ...hsvPayload(colour)});
export const pdModeColour = (pointingMode, locality, colour) => ({type: "updatePdModeColor", pointingMode, locality, ...hsvPayload(colour)});
export const comboFeedback = (locality, colour) => ({type: "updateComboFeedback", locality, ...hsvPayload(colour)});

// Key feedback is stored as one record, so any change posts the whole thing
// with the one field replaced: `rowId` is a colour field or `branch:<n>`.
export function keyFeedback(feedback = {}, rowId, next, overrides = {}) {
    const config = {
        tapBranchColors: (feedback.tapBranchColors || []).map(hsvPayload),
        tapCommittedColor: hsvPayload(feedback.tapCommittedColor),
        holdActiveColor: hsvPayload(feedback.holdActiveColor),
        longHoldActiveColor: hsvPayload(feedback.longHoldActiveColor),
        tapCommitMode: feedback.tapCommitMode,
        locality: feedback.locality,
        ...overrides,
    };
    if (rowId && next) {
        const branch = /^branch:(\d+)$/.exec(rowId);
        if (branch) config.tapBranchColors[Number(branch[1])] = hsvPayload(next);
        else config[rowId] = hsvPayload(next);
    }
    return {type: "updateKeyBehaviorFeedback", config};
}

// A new LED group row: its table, whose row it is (combo feedback has no
// owner — every combo shares one colour), and either a saved group or the
// LEDs picked on the board.
export const ledRow = ({target, owner, source}, ledIndices, colour) => ({
    type: "addRgbLedGroup",
    group: {
        target,
        ...(target === "combo" ? {} : {owner}),
        ...(source ? {ledGroupName: source} : {ledIndices}),
        ...hsvPayload(colour),
    },
});
export const deleteLedRow = (target, index) => ({type: "deleteRgbLedGroup", target, index});
export const saveLedGroup = (ledIndices) => ({type: "saveRgbReusableLedGroup", group: {ledIndices}});
export const deleteLedGroup = (name) => ({type: "deleteRgbReusableLedGroup", name});

// ── macros, pointing, settings ──────────────────────────────────────────

export const macroMessage = (keycode, payload, identity) => ({type: "updateViaMacro", keycode, payload, expectedFingerprint: identity});
// A VIA macro's name; its steps stay as they are.
export const macroNameMessage = (keycode, name, identity) => ({type: "updateViaMacro", keycode, name: String(name ?? "").trim(), expectedFingerprint: identity});

export const pdMode = (slot, config, identity) => ({type: "savePdMode", slot, expectedBase: identity, config});
export const clearPdMode = (slot, identity) => ({type: "clearPdMode", slot, expectedBase: identity});
export const duplicatePdMode = (slot, source, identity) => ({type: "duplicatePdMode", slot, source, expectedBase: identity});

// A settings section is saved whole: the core validates the complete set, so
// every field travels together. `input(field)` is the value on screen, or
// undefined for a field that is not drawn, which keeps what was read.
export function settingsSection(section, input, identity) {
    return {
        type: "updateConfigDefaults",
        sectionId: section.id,
        expectedFingerprint: identity,
        fields: section.fields.map((field) => {
            const shown = input(field);
            if (field.kind === "toggle") return {macro: field.macro, enabled: shown === undefined ? field.enabled : Boolean(shown)};
            return {macro: field.macro, value: shown === undefined ? field.value : shown};
        }),
    };
}

// ── the keycode picker ──────────────────────────────────────────────────

export const PICKER_MODIFIERS = [
    ["Ctrl", "C"], ["Shift", "S"], ["Alt", "A"], ["Cmd", "G"],
    ["Right Ctrl", "RCTL"], ["Right Shift", "RSFT"], ["Right Alt", "RALT"], ["Right Cmd", "RGUI"],
];

// What the picker posts: a list for list pickers, otherwise one key wrapped
// in the chosen modifiers, and in LT(layer, …) when a tap-hold layer is armed.
// Layers are posted by index, the form the draft encodes.
export function pickerExpression({keys = [], mods = [], layerTap = null, mode = "single"}) {
    if (!keys.length) return "";
    if (mode === "list") return keys.join(", ");
    let value = keys[0];
    for (const name of mods) value = `${PICKER_MODIFIERS.find(([label]) => label === name)[1]}(${value})`;
    if (layerTap !== null && layerTap !== undefined && layerTap !== "") value = `LT(${layerTap}, ${value})`;
    return value;
}

// ── the macro recorder ──────────────────────────────────────────────────

// One captured key event appended to a take. The pause before the first key
// is the time it took to start typing, so only later gaps become delays, and
// only above the threshold, rounded as configured.
export function recordedPayload(current, {keycode, type, gap, captured, delays = true, threshold = 0, round = 1, explicit = false}) {
    const step = Math.max(1, Number(round) || 1);
    const delay = captured && delays && gap > Math.max(0, Number(threshold) || 0) ? `{${Math.round(gap / step) * step}}` : "";
    const command = explicit ? `{${type === "keydown" ? "+" : "-"}${keycode}}` : `{${keycode}}`;
    return `${current}${delay}${command}`;
}

// ── keyboard shortcuts ──────────────────────────────────────────────────

// ⌘Z / Ctrl+Z undoes the draft, ⇧⌘Z and ⌘Y redo it — unless the keys belong
// to something else: a text field keeps its own undo, and a recording or an
// open prompt is not the draft.
export function historyAction({key, metaKey, ctrlKey, altKey, shiftKey}, {editingText = false, busy = false} = {}) {
    if (!(metaKey || ctrlKey) || altKey || editingText || busy) return null;
    const name = String(key).toLowerCase();
    if (name === "z") return shiftKey ? "redo" : "undo";
    if (name === "y" && !shiftKey) return "redo";
    return null;
}

// ⌘C / ⌘V copy a key onto the selected key; Delete and Backspace make it
// transparent. Anything with a modifier the shortcut does not use is left
// alone.
export function keyAction({key, metaKey, ctrlKey, altKey, shiftKey}) {
    const name = String(key).toLowerCase();
    if ((name === "delete" || name === "backspace") && !(metaKey || ctrlKey || altKey || shiftKey)) return "clear";
    if ((name === "c" || name === "v") && (metaKey || ctrlKey) && !altKey && !shiftKey) return name === "c" ? "copy" : "paste";
    return null;
}
