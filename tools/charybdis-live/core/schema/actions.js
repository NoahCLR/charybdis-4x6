"use strict";

// Profile actions: their names, their native keycodes, and the limits that
// decode them, in one place.
//
// A profile stores an action as {kind, operand}. Editors, the draft, the
// review and the device model all need to name one, turn it into the native
// keycode VIA stores, and decode a domain with the right action limits; they
// ask here rather than each doing it their own way.

const keycodes = require("../data/keycode-catalog");
const {PD_BINDINGS, pdBindingOfCode} = require("../data/pd-bindings");
const {PROFILE_ACTION_KINDS: ACTION} = require("./profile-blob-v1");
const {resolveNativeQmkExpression} = require("./compiled-profile-v1");

// The native action ABIs whose semantic actions map onto VIA keycodes.
const NATIVE_ACTION_ABI_V1 = 0xdcb00959;
const KNOWN_ACTION_ABIS = Object.freeze([NATIVE_ACTION_ABI_V1, 0xeb80829c, 0x61072732]);
const knownActionAbi = value => KNOWN_ACTION_ABIS.includes(value);

// An action's name, the one every edit message and row lookup uses.
function actionName(action) {
    switch (action.kind) {
        case ACTION.NONE: return "KC_NO";
        case ACTION.QMK_KEYCODE: return keycodes.resolve(action.operand).name;
        case ACTION.LAYER_MOMENTARY: return `MO(${action.operand})`;
        case ACTION.LAYER_LOCK: return `LOCK_LAYER(${action.operand})`;
        case ACTION.VIA_MACRO: return `VIA_MACRO_${action.operand}`;
        case ACTION.HARDCODED_MACRO: return `MACRO_${action.operand}`;
        case ACTION.PD_MODE_MOMENTARY:
        case ACTION.PD_MODE_LOCK: {
            const binding = PD_BINDINGS[action.operand];
            if (!binding) throw new Error(`Unsupported pointing slot ${action.operand}.`);
            return action.kind === ACTION.PD_MODE_LOCK ? binding.lock : binding.hold;
        }
        default: throw new Error(`Unsupported device action kind ${action.kind}.`);
    }
}

// The native keycode an action is stored as on a VIA layer, when it has one.
const nativeCode = action => action.kind === ACTION.QMK_KEYCODE ? action.operand : resolveNativeQmkExpression(actionName(action), {});
// A native keycode as the action a profile stores for it.
const keycodeAction = operand => ({kind: ACTION.QMK_KEYCODE, operand});

// The pointing slot a native keycode binds, if it is a pointing-mode key.
const pdSlotOfCode = code => pdBindingOfCode(code)?.slot;

// The action limits a profile of this schema version decodes with: schema 2
// has eight pointing slots, schema 1 six.
const actionLimitsFor = version => ({actionLimits: {maxPdModes: version === 2 ? 8 : 6}});

// A layer as edit messages and group rows name it, and back.
const layerRef = index => `Layer ${index}`;
const layerOfRef = text => {
    const match = /^Layer ([0-9]+)$/.exec(String(text ?? ""));
    return match ? Number(match[1]) : undefined;
};

// TG(n), TO(n), TT(n) and OSL(n) on a layer the keyboard has: the QMK layer
// keycodes a keyboard reporting Profile Wire feature bit 14 runs through its
// own layer ownership (TG as LOCK_LAYER, TO as "lock only n", TT as a hold
// whose fifth tap locks, OSL as a one-shot). Where each may go is
// placementProblem's to say.
const OWNED_LAYER_BASES = Object.freeze([0x5200, 0x5260, 0x5280, 0x52c0]); // TO, TG, OSL, TT
const isOwnedLayerCode = (code, layerCount) =>
    OWNED_LAYER_BASES.includes(code & ~0x1f) && (code & 0x1f) < layerCount;

// Where a stored action may be placed, as the keyboard checks a profile it is
// asked to save (noah_action_supported_at in users/noah/lib/action). Only
// layer keycodes are restricted: a layer hold needs a key held down, so it is a
// key, a press-and-hold branch or a combo (a combo holds its output); OSL() is
// a key, a tap or a combo; LM() is a key or a combo; LT() is only a key; TG(),
// TO() and LOCK_LAYER() go anywhere; DF() and PDF(), which the keyboard does not
// own, go nowhere. Returns what is wrong, or undefined.
const PLACEMENT = Object.freeze({KEY: "key", TAP: "tap", HOLD_PRESS: "holdPress", HOLD_OTHER: "holdOther", COMBO_OUTPUT: "comboOutput"});

function layerRuleOf(code, layerCount) {
    const range = (base, span = 0x20) => code >= base && code < base + span;
    const layer = code & 0x1f;
    if (range(0x4000, 0x1000)) return ((code >> 8) & 0x0f) < layerCount ? {kind: "layerTap", places: [PLACEMENT.KEY]} : {kind: "unowned", places: []};
    if (range(0x5000, 0x200)) return ((code >> 5) & 0x0f) < layerCount ? {kind: "layerMod", places: [PLACEMENT.KEY, PLACEMENT.COMBO_OUTPUT]} : {kind: "unowned", places: []};
    if (range(0x5240) || range(0x52e0)) return {kind: "unowned", places: []};
    if (!(range(0x5200) || range(0x5220) || range(0x5260) || range(0x5280) || range(0x52c0))) return undefined;
    if (layer >= layerCount) return {kind: "unowned", places: []};
    if (range(0x5220) || range(0x52c0)) return {kind: "hold", places: [PLACEMENT.KEY, PLACEMENT.HOLD_PRESS, PLACEMENT.COMBO_OUTPUT]};
    if (range(0x5280)) return {kind: "oneShot", places: [PLACEMENT.KEY, PLACEMENT.TAP, PLACEMENT.COMBO_OUTPUT]};
    return undefined;
}

function placementProblem(action, placement, {layerCount = 8} = {}) {
    const code = action.kind === ACTION.LAYER_MOMENTARY ? 0x5220 | action.operand : nativeCode(action);
    const rule = code === undefined ? undefined : layerRuleOf(code, layerCount);
    if (!rule || rule.places.includes(placement)) return undefined;
    const name = actionName(action);
    switch (rule.kind) {
        case "unowned": return `${name} is a layer keycode this keyboard does not run through its layer tracking, so it cannot be saved here.`;
        case "hold": return `${name} holds a layer, so it only works as a key, a combo or a "Press and hold until release" branch.`;
        case "layerTap": return `${name} makes its own tap/hold decision, so it only works as a key, not in a behaviour or a combo.`;
        case "oneShot": return `${name} works as a key, a tap or a combo, not as a hold.`;
        case "layerMod": return `${name} holds a layer and its modifiers, so it only works as a key or a combo.`;
        default: return undefined;
    }
}

// Whether a behaviour can send an action. QMK and keyboard functions past the
// layer keycodes and below the user range (the Charybdis DPI and sniping keys,
// RGB Matrix, Magic, QK_BOOT…) run only through QMK's key processing. A
// keyboard reporting Profile Wire feature bit 15 sends them from a behaviour as
// a synthetic QMK record, so they work there as on a key. An older one sends
// them through tap_code16(), which keeps only the low byte (DRGSCRL, 0x7E06,
// types C), so there they belong on a key or a combo only.
const isQmkFunction = code => code > 0x52ff && code < 0x7e40 && !(code >= 0x7700 && code < 0x7780);
function behaviorEmitProblem(action, {behaviorQmkFunctions = false} = {}) {
    const code = nativeCode(action);
    if (behaviorQmkFunctions || code === undefined || !isQmkFunction(code)) return undefined;
    return `${actionName(action)} runs in QMK's own key handling, which this keyboard's behaviours do not reach yet, so it only works as a key or a combo here. A firmware update lets behaviours send it.`;
}

module.exports = {PLACEMENT, placementProblem, behaviorEmitProblem, isOwnedLayerCode, KNOWN_ACTION_ABIS, NATIVE_ACTION_ABI_V1, knownActionAbi, actionName, nativeCode, keycodeAction, pdSlotOfCode, actionLimitsFor, layerRef, layerOfRef};
