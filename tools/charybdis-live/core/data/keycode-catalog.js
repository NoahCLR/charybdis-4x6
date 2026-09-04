"use strict";

// Resolves the numeric keycodes a device reports into something a person
// recognises, using the vendored catalog rather than a QMK checkout.
//
// A keycode arriving from the keyboard is a bare uint16. Most resolve to a
// catalog entry directly. The rest are parameterised quantum keycodes — layer
// taps, momentary layers, modified keys — where the high bits select a
// behaviour and the low bits carry its arguments. Those are decoded
// structurally so an unknown-but-valid keycode still renders as something
// truthful rather than as a hex number.

const catalog = require("./keycode-catalog.json");

const BY_VALUE = new Map(catalog.entries.map((entry) => [entry.value, entry]));
const BY_NAME = new Map();
for (const entry of catalog.entries) {
    BY_NAME.set(entry.name, entry);
    for (const alias of entry.aliases) {
        if (!BY_NAME.has(alias)) {
            BY_NAME.set(alias, entry);
        }
    }
}

// QMK's quantum keycode ranges. Kept here rather than derived from the catalog
// because the catalog lists the range markers only as boundaries, which the
// generator deliberately drops.
const QUANTUM = Object.freeze([
    {mask: 0xf000, base: 0x4000, kind: "layer-tap", layerBits: 0x0f00, layerShift: 8, keyBits: 0x00ff},
    {mask: 0xf000, base: 0x5000, kind: "layer", layerBits: 0x0fff, layerShift: 0, keyBits: 0},
    {mask: 0xf000, base: 0x6000, kind: "mod-tap", layerBits: 0x1f00, layerShift: 8, keyBits: 0x00ff},
]);

const MOMENTARY_BASE = 0x5220;
const TOGGLE_BASE = 0x5260;
const ONESHOT_BASE = 0x5280;
const LAYER_MOVE_BASE = 0x5240;

function resolve(value) {
    if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
        return unknown(value);
    }

    const direct = BY_VALUE.get(value);
    if (direct) {
        return {
            value,
            name: direct.name,
            label: direct.label,
            group: direct.group,
            kind: "basic",
            known: true,
        };
    }

    const layered = resolveLayerKeycode(value);
    if (layered) {
        return layered;
    }

    const tap = resolveTapHold(value);
    if (tap) {
        return tap;
    }

    return unknown(value);
}

// MO/TG/OSL/TO carry the layer in the low bits of a fixed base.
function resolveLayerKeycode(value) {
    const forms = [
        {base: MOMENTARY_BASE, name: "MO", label: "Layer hold"},
        {base: TOGGLE_BASE, name: "TG", label: "Layer toggle"},
        {base: ONESHOT_BASE, name: "OSL", label: "One-shot layer"},
        {base: LAYER_MOVE_BASE, name: "TO", label: "Layer move"},
    ];
    for (const form of forms) {
        const layer = value - form.base;
        if (layer >= 0 && layer < 32) {
            return {
                value,
                name: `${form.name}(${layer})`,
                label: `${form.label} ${layer}`,
                group: "layer",
                kind: "layer",
                layer,
                known: true,
            };
        }
    }
    return undefined;
}

// LT(layer, kc) and mod-tap share a shape: a behaviour in the high nibble, a
// basic keycode in the low byte.
function resolveTapHold(value) {
    for (const range of QUANTUM) {
        if ((value & range.mask) !== range.base || range.kind === "layer") {
            continue;
        }
        const tapped = BY_VALUE.get(value & range.keyBits);
        const argument = (value & range.layerBits) >> range.layerShift;
        if (!tapped) {
            continue;
        }
        if (range.kind === "layer-tap") {
            return {
                value,
                name: `LT(${argument},${tapped.name})`,
                label: `${tapped.label} / layer ${argument}`,
                group: "layer",
                kind: "layer-tap",
                layer: argument,
                tap: tapped.name,
                known: true,
            };
        }
        return {
            value,
            name: `MT(${argument},${tapped.name})`,
            label: `${tapped.label} / mod`,
            group: "modifiers",
            kind: "mod-tap",
            tap: tapped.name,
            known: true,
        };
    }
    return undefined;
}

function unknown(value) {
    const hex = Number.isInteger(value) ? `0x${(value >>> 0).toString(16).toUpperCase().padStart(4, "0")}` : String(value);
    return {value, name: hex, label: hex, group: "unknown", kind: "unknown", known: false};
}

function lookup(name) {
    return BY_NAME.get(name);
}

function metadata() {
    return {
        format: catalog.format,
        qmkVersion: catalog.qmkVersion,
        keycodeCount: catalog.entries.length,
    };
}

module.exports = {lookup, metadata, resolve};
