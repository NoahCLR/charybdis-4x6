"use strict";

// The name a key reads by, in one profile (D-L33). The vendored catalogue names
// what QMK ships; the profile names the rest: a macro or custom key by its
// name, a pointing-mode key by its slot, a bare user slot by the behaviour
// target it stands for.
// The screens (session/device-model.js) and the review (model/profile-review.js)
// both ask here, so one key never reads "Dragscroll · hold" on one and
// "User 16" on the other.
//
// A layer is named by its name wherever a key is named: MO(3) reads "Hold
// Navigation" and LT(3,KC_SLASH) "/ / Navigation". Only the raw keycode, where
// a screen shows one, keeps the number.

const {PD_BINDINGS} = require("../data/pd-bindings");
const {CUSTOM_KEY_SLOTS, customKeyCode} = require("../data/user-keycodes");
const keycodes = require("../data/keycode-catalog");
const {actionName} = require("../schema/actions");
const {PROFILE_ACTION_KINDS: ACTION} = require("../schema/profile-blob-v1");
const {resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {layerName, slotName} = require("./vocabulary");

// What a layer key does to its layer, in the picker's words. TG is a lock on
// this firmware (D-L34).
const LAYER_VERBS = {MO: "Hold", TG: "Lock", LOCK_LAYER: "Lock", TT: "Tap-toggle", OSL: "One-shot", TO: "Move to", DF: "Default", PDF: "Default"};
const LAYER_KEY = /^(MO|TG|TT|OSL|TO|DF|PDF|LOCK_LAYER)\((\d+)\)$/;

// LOCK_LAYER(2) → "Lock layer 2".
function semanticLabel(semantic) {
    const [, name, args] = /^([A-Z][A-Z0-9_]*)(?:\((.*)\))?$/.exec(semantic) || [, semantic, undefined];
    const words = name.replace(/_/g, " ").toLowerCase();
    return words.charAt(0).toUpperCase() + words.slice(1) + (args === undefined ? "" : ` ${args.replace(/\s+/g, "")}`);
}

/**
 * What one profile renames, over the catalogue.
 *
 *   labels    keycode name → label, under the stored name and the profile's own
 *   aliases   stored name → the profile's name for it (QK_USER_16 → PD_SLOT_0)
 *   pointing  every pointing-mode keycode, {name, native, code, label}
 *   custom    every custom key, {name, native, code, label}
 *
 * `layers` holds the layer names, which every layer key reads by. Only a
 * keyboard whose action ABI is known gets the rest from its profile: the
 * user-slot layout they depend on is the ABI's.
 */
function profileKeyNames({layers, macros = [], customKeys = [], behaviors = [], pdModes = [], actionsKnown = false} = {}) {
    const labels = {}, aliases = {}, pointing = [], custom = [];
    const count = Math.max(8, layers?.length || 0);
    for (let layer = 0; layer < count; layer++) for (const form of Object.keys(LAYER_VERBS)) {
        const name = `${form}(${layer})`;
        if (form !== "LOCK_LAYER") labels[name] = layerKeyLabel(name, layers);
        else if (actionsKnown) {
            const value = resolveNativeQmkExpression(name, {});
            if (value === undefined) continue;
            const native = keycodes.resolve(value).name;
            aliases[native] = name;
            labels[native] = labels[name] = layerKeyLabel(name, layers);
        }
    }
    const layered = {labels, aliases, pointing, custom, layers: layers || []};
    if (!actionsKnown) return layered;
    for (const slot of macros) {
        const native = keycodes.resolve(resolveNativeQmkExpression(slot.keycode, {})).name;
        const label = slot.name || `Macro ${slot.keycode.split("_").at(-1)}`;
        aliases[native] = slot.keycode;
        labels[native] = label;
        labels[slot.keycode] = label;
    }
    // Every custom key, named or not: all 64 exist, and one without a name
    // reads by its number.
    for (let slot = 0; slot < CUSTOM_KEY_SLOTS; slot++) {
        const name = `CUSTOM_KEY_${slot}`, code = customKeyCode(slot);
        const label = customKeys.find(key => key.slot === slot)?.name || `Custom key ${slot}`;
        const native = keycodes.resolve(code).name;
        aliases[native] = name;
        labels[native] = labels[name] = label;
        custom.push({name, native, code, label});
    }
    // A behaviour row keyed by a semantic target names its keycode only where
    // the catalogue has nothing better than a bare user slot: LOCK_LAYER(2)
    // reads "Lock Numbers" instead of "User 30". A key already named — a layer
    // key, or a macro named above — keeps that name, so a key reads the same
    // whether or not a behaviour sits on it.
    for (const row of behaviors) {
        if (row.target.kind === ACTION.QMK_KEYCODE) continue;
        const semantic = actionName(row.target);
        const value = resolveNativeQmkExpression(semantic, {});
        if (value === undefined) continue;
        const catalogued = keycodes.resolve(value);
        aliases[catalogued.name] = semantic;
        const generic = !catalogued.known || catalogued.group === "user";
        if (!generic || labels[catalogued.name] !== undefined) continue;
        labels[catalogued.name] = labels[semantic] = semanticLabel(semantic);
    }
    // Every slot gets its keycodes, configured or not: the firmware's mode
    // keycodes are a fixed registry, so a key may be placed for a slot that is
    // still empty, and stays put when a slot is cleared. It does nothing until
    // the slot is configured, which the label says.
    for (const slot of pdModes) for (const locked of [false, true]) {
        const binding = PD_BINDINGS[slot.id];
        const name = locked ? binding.lock : binding.hold, code = locked ? binding.lockCode : binding.holdCode;
        const label = `${slotName(slot)} · ${locked ? "toggle" : "hold"}${slot.kind ? "" : " (empty)"}`;
        const native = keycodes.resolve(code).name;
        aliases[native] = name;
        labels[native] = label;
        labels[name] = label;
        pointing.push({name, native, code, label});
    }
    return layered;
}

// A layer key by what it does and the name of its layer.
function layerKeyLabel(name, layers) {
    const [, form, layer] = LAYER_KEY.exec(name) || [];
    return form ? `${LAYER_VERBS[form]} ${layerName(layers, Number(layer))}` : undefined;
}

// A stored keycode value as it reads in this profile, and as it reads with the
// name it is stored under too, for the one place two keys can read alike: a
// review row whose sides would otherwise look the same (KC_1 and KC_KP_1 are
// both "1").
function keyLabel(names, value) {
    const resolved = keycodes.resolve(value);
    const named = names?.labels?.[resolved.name];
    if (named !== undefined) return named;
    // A layer-tap is a tap key and a layer, both named: "/ / Navigation".
    if (resolved.kind === "layer-tap") {
        const tap = keycodes.lookup(resolved.tap);
        return `${tap ? keyLabel(names, tap.value) : resolved.tap} / ${layerName(names?.layers, resolved.layer)}`;
    }
    return resolved.label;
}
function keyLabelWithName(names, value) {
    const {name} = keycodes.resolve(value);
    const label = keyLabel(names, value);
    return label === name ? label : `${label} (${name})`;
}

module.exports = {keyLabel, keyLabelWithName, layerKeyLabel, profileKeyNames, semanticLabel};
