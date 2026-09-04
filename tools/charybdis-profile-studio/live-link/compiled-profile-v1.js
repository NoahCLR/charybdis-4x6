"use strict";

const {
    PROFILE_ACTION_KINDS,
    PROFILE_DOMAIN_IDS,
    decodeProfileBlob,
    encodeProfileBlob,
} = require("./profile-blob-v1");
const {
    KEY_BEHAVIOR_HOLD_MODES,
    encodeKeyBehaviorDomain,
} = require("./key-behavior-domain-v1");
const {
    RGB_PD_MODE_IDS,
    createStudioRgbDomainV1,
} = require("./rgb-domain-v1");

const PROFILE_MILESTONE_DOMAIN_MASK = 0x03;
const QMK_USER_BASE = 0x7e40;
const QMK_MACRO_BASE = 0x7700;
const HARDCODED_MACRO_SLOTS = 16;
const CHARYBDIS_KEYCODE_VALUES = Object.freeze({
    DPI_MOD: 0x7e00,
    DPI_RMOD: 0x7e01,
    S_D_MOD: 0x7e02,
    S_D_RMOD: 0x7e03,
    SNIPING: 0x7e04,
    SNP_TOG: 0x7e05,
    DRGSCRL: 0x7e06,
    DRG_TOG: 0x7e07,
});

const HOLD_MODES = Object.freeze({
    PRESS_AND_HOLD_UNTIL_RELEASE: KEY_BEHAVIOR_HOLD_MODES.PRESS_AND_HOLD_UNTIL_RELEASE,
    TAP_AT_HOLD_THRESHOLD: KEY_BEHAVIOR_HOLD_MODES.TAP_AT_HOLD_THRESHOLD,
    REPEAT_WHILE_HELD: KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD,
    TAP_ON_RELEASE_AFTER_HOLD: KEY_BEHAVIOR_HOLD_MODES.TAP_ON_RELEASE_AFTER_HOLD,
});

const PD_ACTIONS = Object.freeze({
    DRAGSCROLL: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_DRAGSCROLL},
    DRAGSCROLL_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_DRAGSCROLL},
    VOLUME_MODE: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_VOLUME},
    VOLUME_MODE_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_VOLUME},
    BRIGHTNESS_MODE: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_BRIGHTNESS},
    BRIGHTNESS_MODE_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_BRIGHTNESS},
    ZOOM_MODE: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_ZOOM},
    ZOOM_MODE_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_ZOOM},
    ARROW_MODE: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_ARROW},
    ARROW_MODE_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_ARROW},
    PINCH_MODE: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: RGB_PD_MODE_IDS.PD_MODE_PINCH},
    PINCH_MODE_LOCK: {kind: PROFILE_ACTION_KINDS.PD_MODE_LOCK, operand: RGB_PD_MODE_IDS.PD_MODE_PINCH},
});

const MODIFIER_WRAPPERS = Object.freeze({
    C: 0x0100, LCTL: 0x0100,
    S: 0x0200, LSFT: 0x0200,
    A: 0x0400, LALT: 0x0400,
    G: 0x0800, LGUI: 0x0800,
    LCAG: 0x0d00, LCA: 0x0500, LCG: 0x0900, LCS: 0x0300,
    LAG: 0x0c00, LSG: 0x0a00, LAS: 0x0600,
    MEH: 0x0700, HYPR: 0x0f00,
    RCTL: 0x1100, RSFT: 0x1200, RALT: 0x1400, RGUI: 0x1800,
});

const MODIFIER_BITS = Object.freeze({
    MOD_LCTL: 0x01, MOD_LSFT: 0x02, MOD_LALT: 0x04, MOD_LGUI: 0x08,
    MOD_RCTL: 0x11, MOD_RSFT: 0x12, MOD_RALT: 0x14, MOD_RGUI: 0x18,
    MOD_HYPR: 0x0f, MOD_MEH: 0x07,
});

class StudioProfileCompileError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "StudioProfileCompileError";
        this.code = code;
        Object.assign(this, details);
    }
}

function buildCanonicalStudioProfileV1(model, options = {}) {
    if (!model || typeof model !== "object") {
        throw new TypeError("Profile Studio model must be an object.");
    }
    const capabilities = options.capabilities || {};
    const actionLimits = {
        maxLogicalLayers: capabilities.maxLogicalLayers,
        maxPdModes: Object.keys(RGB_PD_MODE_IDS).length,
        maxViaMacroSlots: capabilities.viaMacroSlots,
        maxHardcodedMacroSlots: capabilities.hardcodedMacroSlots,
    };
    for (const key of Object.keys(actionLimits)) {
        if (!Number.isInteger(actionLimits[key])) delete actionLimits[key];
    }
    const rgbOptions = {
        maxLogicalLayers: capabilities.maxLogicalLayers,
        // The firmware's canonical compiled-default materializer stores only
        // LED groups referenced by renderer rows. Source-only declarations
        // have no runtime meaning and would make source/live identity drift.
        includeUnusedGroups: false,
    };
    if (Number.isInteger(capabilities.physicalLedCount) && capabilities.physicalLedCount !== 58) {
        throw compileError("INCOMPATIBLE_RGB_GEOMETRY", `Firmware reports ${capabilities.physicalLedCount} RGB LEDs; this profile schema requires 58.`);
    }
    const rgbDomain = createStudioRgbDomainV1(model, rgbOptions);
    const behaviorRows = (model.keyBehaviors || []).map((row, rowIndex) => compileBehaviorRow(row, model, rowIndex));
    const behaviorPayload = encodeKeyBehaviorDomain({rows: behaviorRows}, {
        limits: {
            maxRows: capabilities.maxBehaviorRows,
            maxTapStepsPerBehavior: capabilities.maxTapStepsPerBehavior,
            maxPopulatedSteps: capabilities.maxPopulatedBehaviorSteps,
        },
        actionLimits,
    });
    const blob = encodeProfileBlob({domains: [
        rgbDomain,
        {id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS, version: 1, payload: behaviorPayload},
    ]});
    if (Number.isInteger(capabilities.maxProfilePayload) && blob.length > capabilities.maxProfilePayload) {
        throw compileError("CAPACITY_EXCEEDED", `Compiled live profile is ${blob.length} bytes; firmware accepts at most ${capabilities.maxProfilePayload}.`);
    }
    const decoded = decodeProfileBlob(blob);
    return {
        blob,
        byteLength: blob.length,
        crc32: decoded.crc32,
        digest: decoded.digest,
        domainMask: PROFILE_MILESTONE_DOMAIN_MASK,
        behaviorRows: behaviorRows.length,
    };
}

function compileBehaviorRow(row, model, rowIndex) {
    const label = `Key behavior ${rowIndex + 1} (${String(row?.keycode || "missing target")})`;
    if (!row || typeof row !== "object") throw compileError("INVALID_BEHAVIOR", `${label} is not an object.`, {row: rowIndex});
    const compiled = {
        target: semanticActionForExpression(row.keycode, model, `${label} target`),
        tapHoldTerm: optionalU16(row.tapHoldTerm, `${label} tap-hold term`),
        longerHoldTerm: optionalU16(row.longerHoldTerm, `${label} longer-hold term`),
        multiTapTerm: optionalU16(row.multiTapTerm, `${label} multi-tap term`),
        keepsAutoMouseAnchored: Boolean(row.keepsAutoMouseAnchored),
        steps: [],
    };
    for (const [stepIndex, step] of (row.steps || []).entries()) {
        const stepLabel = `${label}, tap branch ${Number(step.tapCount) + 1}`;
        const value = {tapIndex: requiredInteger(step.tapCount, 0, 0xff, `${stepLabel} index`)};
        if (step.tap) value.tap = compileTap(step.tap, model, `${stepLabel} tap`);
        if (step.hold) value.hold = compileHold(step.hold, model, `${stepLabel} hold`);
        if (step.longHold) value.longHold = compileHold(step.longHold, model, `${stepLabel} long hold`);
        if (!value.tap && !value.hold && !value.longHold) {
            throw compileError("EMPTY_BEHAVIOR_STEP", `${stepLabel} has no action.`, {row: rowIndex, step: stepIndex});
        }
        compiled.steps.push(value);
    }
    return compiled;
}

function compileTap(branch, model, label) {
    if (String(branch.helper || "").trim() !== "TAP_SENDS") {
        throw compileError("UNSUPPORTED_HELPER", `${label} uses ${branch.helper || "no helper"}; live profiles support TAP_SENDS for tap branches.`, {expression: branch.helper});
    }
    return semanticActionForExpression(branch.action, model, label);
}

function compileHold(branch, model, label) {
    const helper = String(branch.helper || "").trim();
    const mode = HOLD_MODES[helper];
    if (!mode) {
        throw compileError("UNSUPPORTED_HELPER", `${label} uses ${helper || "no helper"}; it has no stable live-profile hold mode.`, {expression: helper});
    }
    const repeatHz = helper === "REPEAT_WHILE_HELD"
        ? requiredInteger(branch.repeatHz, 1, 100, `${label} repeat rate`)
        : 0;
    return {mode, repeatHz, action: semanticActionForExpression(branch.action, model, label)};
}

function semanticActionForExpression(value, model, label = "Action") {
    const expression = normalizeExpression(value);
    if (!expression) throw compileError("UNSUPPORTED_ACTION", `${label} is empty.`, {expression});
    if (PD_ACTIONS[expression]) return {...PD_ACTIONS[expression]};

    let match = expression.match(/^VIA_MACRO_(\d+)$/);
    if (match) return {kind: PROFILE_ACTION_KINDS.VIA_MACRO, operand: Number(match[1])};
    match = expression.match(/^MACRO_(\d+)$/);
    if (match) return {kind: PROFILE_ACTION_KINDS.HARDCODED_MACRO, operand: Number(match[1])};

    const call = parseCall(expression);
    if (call?.name === "MO" || call?.name === "LOCK_LAYER") {
        if (call.args.length !== 1) throw unsupportedAction(label, expression);
        const layer = layerId(call.args[0], model, label);
        return {kind: call.name === "MO" ? PROFILE_ACTION_KINDS.LAYER_MOMENTARY : PROFILE_ACTION_KINDS.LAYER_LOCK, operand: layer};
    }

    const numeric = resolveNativeQmkExpression(expression, model);
    if (numeric === undefined) throw unsupportedAction(label, expression);
    return {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: numeric};
}

function resolveNativeQmkExpression(value, model) {
    const expression = normalizeExpression(value);
    if (/^(?:0x[0-9a-f]+|\d+)$/i.test(expression)) {
        const number = Number(expression);
        return Number.isInteger(number) && number >= 0 && number <= 0xffff ? number : undefined;
    }
    const catalog = model?.qmkKeycodeValues || {};
    if (Number.isInteger(catalog[expression])) return catalog[expression];
    if (Number.isInteger(CHARYBDIS_KEYCODE_VALUES[expression])) return CHARYBDIS_KEYCODE_VALUES[expression];
    if (expression === "_______") return catalog.KC_TRNS ?? catalog.KC_TRANSPARENT ?? 1;
    if (expression === "XXXXXXX") return catalog.KC_NO ?? 0;

    let match = expression.match(/^VIA_MACRO_(\d+)$/);
    if (match && Number(match[1]) < 64) return QMK_MACRO_BASE + Number(match[1]);
    match = expression.match(/^MACRO_(\d+)$/);
    if (match && Number(match[1]) < HARDCODED_MACRO_SLOTS) return QMK_USER_BASE + Number(match[1]);

    const pdModeNames = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
    const pdModeIndex = pdModeNames.indexOf(expression.replace(/_LOCK$/, ""));
    if (pdModeIndex >= 0) {
        return QMK_USER_BASE + HARDCODED_MACRO_SLOTS + pdModeIndex + (expression.endsWith("_LOCK") ? pdModeNames.length : 0);
    }

    const custom = localCustomKeycodeValues(model);
    if (Number.isInteger(custom[expression])) return custom[expression];

    const call = parseCall(expression);
    if (!call) return undefined;
    if (["TO", "MO", "DF", "TG", "OSL"].includes(call.name) && call.args.length === 1) {
        const layer = layerIdOrUndefined(call.args[0], model);
        const bases = {TO: 0x5200, MO: 0x5220, DF: 0x5240, TG: 0x5260, OSL: 0x5280};
        return layer === undefined || layer > 0x1f ? undefined : bases[call.name] | layer;
    }
    if (call.name === "LOCK_LAYER" && call.args.length === 1) {
        const layer = layerIdOrUndefined(call.args[0], model);
        return layer === undefined
            ? undefined
            : QMK_USER_BASE + HARDCODED_MACRO_SLOTS + (pdModeNames.length * 2) + layer;
    }
    if (MODIFIER_WRAPPERS[call.name] !== undefined && call.args.length === 1) {
        const keycode = resolveNativeQmkExpression(call.args[0], model);
        return keycode === undefined ? undefined : (MODIFIER_WRAPPERS[call.name] | keycode) & 0xffff;
    }
    if (call.name === "OSM" && call.args.length === 1) {
        const mods = resolveModifierBits(call.args[0]);
        return mods === undefined ? undefined : 0x52a0 | mods;
    }
    if (call.name === "LT" && call.args.length === 2) {
        const layer = layerIdOrUndefined(call.args[0], model);
        const keycode = resolveNativeQmkExpression(call.args[1], model);
        return layer === undefined || keycode === undefined || keycode > 0xff ? undefined : 0x4000 | (layer << 8) | keycode;
    }
    if (call.name === "MT" && call.args.length === 2) {
        const mods = resolveModifierBits(call.args[0]);
        const keycode = resolveNativeQmkExpression(call.args[1], model);
        return mods === undefined || keycode === undefined || keycode > 0xff ? undefined : 0x2000 | (mods << 8) | keycode;
    }
    return undefined;
}

function localCustomKeycodeValues(model) {
    const layerCount = Array.isArray(model?.layers) ? model.layers.length : 0;
    const pdModeCount = Object.keys(RGB_PD_MODE_IDS).length;
    const first = QMK_USER_BASE + HARDCODED_MACRO_SLOTS + (pdModeCount * 2) + layerCount;
    return Object.fromEntries((model?.customKeycodes || []).map((name, index) => [name, first + index]));
}

function resolveModifierBits(value) {
    const parts = normalizeExpression(value).split("|").map((part) => part.trim()).filter(Boolean);
    if (!parts.length) return undefined;
    let result = 0;
    for (const part of parts) {
        if (MODIFIER_BITS[part] === undefined) return undefined;
        result |= MODIFIER_BITS[part];
    }
    return result & 0x1f;
}

function layerId(value, model, label) {
    const id = layerIdOrUndefined(value, model);
    if (id === undefined) throw compileError("UNKNOWN_LAYER", `${label} references unknown layer ${normalizeExpression(value) || "<empty>"}.`, {expression: value});
    return id;
}

function layerIdOrUndefined(value, model) {
    const expression = normalizeExpression(value);
    if (/^\d+$/.test(expression)) return Number(expression);
    const layers = model?.layers || [];
    const index = layers.findIndex((layer) => layer?.name === expression);
    return index < 0 ? undefined : index;
}

function parseCall(value) {
    const expression = normalizeExpression(value);
    const match = expression.match(/^([A-Z][A-Z0-9_]*)\((.*)\)$/);
    if (!match) return undefined;
    return {name: match[1], args: splitArguments(match[2])};
}

function splitArguments(value) {
    const parts = [];
    let depth = 0;
    let start = 0;
    for (let index = 0; index < value.length; index += 1) {
        if (value[index] === "(") depth += 1;
        if (value[index] === ")") depth -= 1;
        if (depth < 0) return [];
        if (value[index] === "," && depth === 0) {
            parts.push(normalizeExpression(value.slice(start, index)));
            start = index + 1;
        }
    }
    if (depth !== 0) return [];
    parts.push(normalizeExpression(value.slice(start)));
    return parts;
}

function optionalU16(value, label) {
    if (value === undefined || value === null || String(value).trim() === "") return 0;
    return requiredInteger(value, 1, 0xffff, label);
}

function requiredInteger(value, minimum, maximum, label) {
    const text = String(value ?? "").trim();
    if (!/^\d+$/.test(text)) throw compileError("UNRESOLVED_EXPRESSION", `${label} must be a literal integer for live apply; got ${text || "<empty>"}.`, {expression: text});
    const number = Number(text);
    if (!Number.isSafeInteger(number) || number < minimum || number > maximum) {
        throw compileError("VALUE_OUT_OF_RANGE", `${label} must be ${minimum} through ${maximum}; got ${text}.`, {expression: text});
    }
    return number;
}

function normalizeExpression(value) {
    return String(value ?? "").replace(/\s+/g, " ").replace(/\s*,\s*/g, ",").trim();
}

function unsupportedAction(label, expression) {
    return compileError("UNSUPPORTED_ACTION", `${label} expression ${expression} cannot be represented by the stable live-profile action schema.`, {expression});
}

function compileError(code, message, details = {}) {
    return new StudioProfileCompileError(code, message, details);
}

module.exports = {
    PROFILE_MILESTONE_DOMAIN_MASK,
    StudioProfileCompileError,
    buildCanonicalStudioProfileV1,
    resolveNativeQmkExpression,
    semanticActionForExpression,
};
