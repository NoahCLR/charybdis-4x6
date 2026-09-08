"use strict";

const keycodes = require("../data/keycode-catalog");
const {PROFILE_ACTION_KINDS: ACTION} = require("../schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain, KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");
const {semanticActionForExpression, resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {actionName, knownActionAbi} = require("./device-profile-view");

const BEHAVIOR_EDITS = new Set(["saveBehavior", "addBehavior", "deleteBehavior"]);
const invalid = message => Object.assign(new Error(message), {code: "INVALID_BEHAVIOR_EDIT"});
const normalized = value => String(value ?? "").replace(/\s+/g, "");

function integer(value, max, label, optional = false) {
    if (optional && (value === undefined || value === null || String(value).trim() === "")) return 0;
    if (!["string", "number"].includes(typeof value) || !/^\d+$/.test(String(value).trim())) throw invalid(`${label} must be a whole number.`);
    const result = Number(value);
    if (!Number.isInteger(result) || result < 0 || result > max) throw invalid(`${label} must be between 0 and ${max}.`);
    return result;
}

function editKeyBehaviors(payload, message, capabilities = {}) {
    if (!(capabilities.supportedDomainMask & 2)) throw invalid("This firmware does not support saving key behaviours.");
    const {rows} = decodeKeyBehaviorDomain(payload);
    const knownAbi = knownActionAbi(capabilities.actionAbiDigest);
    const native = action => action.kind === ACTION.QMK_KEYCODE ? action.operand
        : knownAbi ? resolveNativeQmkExpression(actionName(action), {}) : undefined;
    const equivalent = (left, right) => (left.kind === right.kind && left.operand === right.operand)
        || (native(left) !== undefined && native(left) === native(right));
    const action = (value, previous) => {
        const name = String(value ?? "").trim();
        if (!name) throw invalid("Choose an action for each enabled behaviour branch.");
        if (previous && normalized(name) === normalized(actionName(previous))) return previous;
        let result;
        const encoded = keycodes.encode(name);
        if (encoded !== undefined && !/^MO\s*\(/.test(name)) {
            if (keycodes.resolve(encoded).kind === "layer") throw invalid("Use MO(layer) or LOCK_LAYER(layer) so the keyboard can track layer ownership.");
            result = {kind: ACTION.QMK_KEYCODE, operand: encoded};
        } else {
            result = semanticActionForExpression(name, {});
            if (result.kind === ACTION.QMK_KEYCODE) {
                // Native expressions must pass the catalog's structural
                // checks; the legacy compiler accepts invalid modifier wraps.
                if (!/^[A-Z][A-Z0-9_]*$/.test(name)) throw invalid(`Unsupported key expression: ${name}.`);
                if (!knownAbi) throw invalid("Named custom actions require a matching keyboard action vocabulary.");
            }
        }
        return previous && equivalent(previous, result) ? previous : result;
    };
    const form = message.type === "deleteBehavior" ? {keycode: message.keycode} : message.behavior;
    if (!form || typeof form !== "object") throw invalid("Choose a behaviour row to save.");
    const named = rows.find(row => normalized(actionName(row.target)) === normalized(form.keycode));
    const target = action(form.keycode, named?.target);
    const index = rows.findIndex(row => equivalent(row.target, target));
    const previous = rows[index];

    if (message.type === "deleteBehavior") {
        if (!previous) throw invalid("This behaviour row is no longer present. Read from keyboard again.");
        rows.splice(index, 1);
    } else {
        if (message.type === "addBehavior" && previous) throw invalid("This key already has a behaviour row. Edit the existing row instead.");
        const suppliedSteps = message.type === "addBehavior" && form.steps === undefined
            ? [{tapCount: 0, tap: form.tap, hold: form.hold, longHold: form.longHold}] : form.steps;
        if (!Array.isArray(suppliedSteps)) throw invalid("A behaviour must include its tap branches.");
        const steps = [];
        const seen = new Set();
        for (const step of suppliedSteps) {
            const tapIndex = integer(step?.tapCount, 4, "Tap branch index");
            if (seen.has(tapIndex)) throw invalid("A behaviour cannot repeat a tap branch index.");
            seen.add(tapIndex);
            const oldStep = previous?.steps.find(row => row.tapIndex === tapIndex);
            const next = {tapIndex};
            for (const field of ["tap", "hold", "longHold"]) {
                const branch = step[field];
                if (!branch) continue;
                const helper = String(branch.helper ?? "").trim();
                if (!helper) {
                    if (String(branch.action ?? "").trim() || String(branch.repeatHz ?? "").trim()) throw invalid("Choose a helper for each enabled behaviour branch.");
                    continue;
                }
                if (field === "tap") {
                    if (helper !== "TAP_SENDS") throw invalid("Tap branches require the Tap sends helper.");
                    next.tap = action(branch.action, oldStep?.tap);
                } else {
                    if (!Object.hasOwn(KEY_BEHAVIOR_HOLD_MODES, helper)) throw invalid(`Unknown hold helper: ${helper}.`);
                    next[field] = {
                        mode: KEY_BEHAVIOR_HOLD_MODES[helper],
                        repeatHz: helper === "REPEAT_WHILE_HELD" ? integer(branch.repeatHz, 100, "Repeat frequency") : 0,
                        action: action(branch.action, oldStep?.[field]?.action),
                    };
                }
            }
            if (next.tap || next.hold || next.longHold) steps.push(next);
        }
        const anchored = form.keepsAutoMouseAnchored ?? previous?.keepsAutoMouseAnchored ?? false;
        if (typeof anchored !== "boolean") throw invalid("The auto-mouse anchor flag must be on or off.");
        const row = {
            target: previous?.target ?? target,
            tapHoldTerm: integer(form.tapHoldTerm, 65535, "Tap/hold time", true),
            longerHoldTerm: integer(form.longerHoldTerm, 65535, "Long-hold time", true),
            multiTapTerm: integer(form.multiTapTerm, 65535, "Multi-tap time", true),
            keepsAutoMouseAnchored: anchored,
            steps,
        };
        if (previous) rows[index] = row;
        else rows.push(row);
    }
    return encodeKeyBehaviorDomain({rows}, {
        limits: {maxRows: capabilities.maxBehaviorRows, maxPopulatedSteps: capabilities.maxPopulatedBehaviorSteps, maxTapStepsPerBehavior: capabilities.maxTapStepsPerBehavior},
        actionLimits: {maxLogicalLayers: capabilities.compiledLayerCount, maxViaMacroSlots: capabilities.viaMacroSlots, maxHardcodedMacroSlots: capabilities.hardcodedMacroSlots},
    });
}

module.exports = {BEHAVIOR_EDITS, editKeyBehaviors};
