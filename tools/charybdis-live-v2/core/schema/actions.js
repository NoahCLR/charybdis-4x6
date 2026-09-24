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

module.exports = {KNOWN_ACTION_ABIS, NATIVE_ACTION_ABI_V1, knownActionAbi, actionName, nativeCode, keycodeAction, pdSlotOfCode, actionLimitsFor, layerRef, layerOfRef};
