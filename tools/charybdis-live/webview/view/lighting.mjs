// What the keyboard would light, from what the keyboard reported.
//
// Every surface that shows colour asks this module, so the board, the key
// dots, the swatches and the review all answer the same way — and they answer
// from the draft, because the draft is what the model carries.

import {isOff} from "../lib/colour.mjs";
import {LED_INDEX, TRACKBALL_LED, inLocality, trackballInLocality} from "./geometry.mjs";

const OFF = {h: "0", s: "0", v: "0"};


export const PD_MODE_IDS = [
    "PD_MODE_DRAGSCROLL", "PD_MODE_VOLUME", "PD_MODE_BRIGHTNESS", "PD_MODE_ZOOM",
    "PD_MODE_ARROW", "PD_MODE_PINCH", "PD_MODE_SLOT_6", "PD_MODE_SLOT_7",
];

export function stageEnabled(model, id) {
    if (id === "base") return Boolean(model?.rgb?.baseEffect?.enabled);
    // A stage is found by its id, never by what it is called.
    const stage = (model?.rgb?.stages || []).find((row) => row.id === id);
    return Boolean(stage?.enabled);
}

// A stage switched on can still paint nothing. The auto-mouse fade runs only
// while auto-mouse raises its layer: with auto-mouse off, a pointer layer
// turned on by hand shows its own lighting. This answers why, or null; the
// stored switch is left as it is, so turning auto-mouse back on restores it.
// Without settings readback nothing is claimed.
export function stageIdle(model, id) {
    if (id !== "auto" || !stageEnabled(model, id)) return null;
    const field = (model?.configDefaults || []).find((section) => section.id === "autoMouse")
        ?.fields.find((entry) => entry.macro === "autoMouse");
    return field && !field.enabled ? "Auto-mouse is off, so this stage paints nothing." : null;
}

export const stageInEffect = (model, id) => stageEnabled(model, id) && !stageIdle(model, id);

// The base effect is a VIA read, not part of the profile. Only a solid colour
// can be drawn honestly; any other effect has no single colour to show.
export function baseColour(model) {
    const base = model?.rgb?.baseEffect;
    if (!base || base.state !== "read" || !base.enabled || !base.previewColor) return null;
    return base.previewColor;
}

export const layerColourRow = (model, layerId) =>
    (model?.rgb?.layerColors || []).find((row) => row.layerId === layerId);

export const pdColourRow = (model, slotId) =>
    (model?.rgb?.pdModeColors || []).find((row) => row.pointingMode === PD_MODE_IDS[slotId]);

export function feedbackColours(model) {
    const feedback = model?.rgb?.keyBehaviorFeedback || {};
    return {
        tap: feedback.tapCommittedColor || OFF,
        hold: feedback.holdActiveColor || OFF,
        long: feedback.longHoldActiveColor || OFF,
        branches: feedback.tapBranchColors || [],
    };
}

export const tierColour = (model, kind) => feedbackColours(model)[kind] || OFF;

// A key counts as mapped on a layer when it stores something of its own;
// transparent and disabled positions keep the layer underneath.
export const isMapped = (position) =>
    Boolean(position?.keycode) && !["KC_TRANSPARENT", "KC_TRNS", "_______", "KC_NO", "XXXXXXX"].includes(position.keycode);

export const mappedKeyCount = (layer) => (layer?.positions || []).filter(isMapped).length;

// Two firmware rules for "no colour": a layer colour is skipped only when both
// saturation and value are 0 (rgb_layer_stage.c), and a group row stored as
// HSV(0, 0, 0) inherits its stage colour instead of painting black
// (rgb_helpers.h rgb_hsv_is_inherit_color).
const solid = (colour) => Boolean(colour) && !(Number(colour.s) === 0 && Number(colour.v) === 0);
const inherits = (colour) => !colour || (Number(colour.h) === 0 && Number(colour.s) === 0 && Number(colour.v) === 0);

// The indicator stages only run under a live effect: with the matrix off the
// firmware reports effect 0 and paints nothing at all (rgb_matrix.c).
const matrixOff = (model) => model?.rgb?.baseEffect?.state === "read" && !model.rgb.baseEffect.enabled;

// Whether a key stores something of its own on layer l. The viewed layer's
// position is passed in; any other layer is looked up at the same place.
function mappedOn(model, layer, l, position) {
    if (!position) return false;
    if (l === layer?.index) return isMapped(position);
    const other = (model?.layers || []).find((row) => row.index === l) || model?.layers?.[l];
    return isMapped((other?.positions || []).find((row) => row.layoutIndex === position.layoutIndex));
}

/**
 * The colour one LED would show with the viewed layer held on top of the
 * default layer — the firmware's own stage order, per LED:
 *
 *   base effect → every active layer's solid colour, ascending (layer 0 first,
 *   so the default layer shows through a mapped-keys-only layer above it) →
 *   the layer LED group rows → a previewed pointing mode by locality → its
 *   LED group rows.
 *
 * The answer says what painted the LED last: `source` is the stage ("off",
 * "base", "layer" or "pointing") and, for a layer, `layer` is its id.
 *
 * `options.held` names the layers previewed on as well (by layer id); they
 * paint in the same ascending pass, as rgb_layer_stage.c paints every active
 * layer.
 *
 * `position` is the key over this LED, or undefined for the trackball LED,
 * which no key maps to: an all-keys wash reaches it, a mapped-keys layer never
 * does. A pointing-mode colour is an overlay that exists only while its mode
 * runs, so it is painted only when the caller asks for that preview.
 */
function ledLight(model, layer, led, position, options = {}) {
    if (matrixOff(model)) return {colour: OFF, source: "off"};
    const base = stageEnabled(model, "base") ? baseColour(model) : null;
    let colour = base || OFF, source = base ? "base" : "off", painter = null;
    const paint = (next, from, by = null) => { colour = next; source = from; painter = by; };

    if (stageEnabled(model, "layers")) {
        const active = [...new Set([0, layer?.index ?? 0, ...(options.held || [])])].sort((a, b) => a - b);
        for (const l of active) {
            const row = layerColourRow(model, l);
            if (!row || !solid(row.color)) continue;
            if (row.mode === "ALL_KEYS" || mappedOn(model, layer, l, position)) paint(row.color, "layer", l);
        }
        for (const group of model?.rgb?.layerLedGroups || []) {
            if (!(group.ledIndices || []).includes(led)) continue;
            const owners = group.owner === "RGB_LAYER_GROUP_ALL" ? active : active.filter((l) => group.owner === `Layer ${l}`);
            for (const l of owners) {
                const row = layerColourRow(model, l);
                if (!inherits(group.color)) paint(group.color, "layer", l);
                else if (row && solid(row.color)) paint(row.color, "layer", l);
            }
        }
    }

    const preview = options.pdActive;
    if (preview && stageEnabled(model, "pd")) {
        const reaches = position ? inLocality(position.layoutIndex, preview.locality, preview.triggerIndex)
            : trackballInLocality(preview.locality, preview.triggerIndex);
        if (!isOff(preview.color) && reaches) paint(preview.color, "pointing");
        for (const group of model?.rgb?.pdModeLedGroups || []) {
            if (!(group.ledIndices || []).includes(led)) continue;
            if (group.owner !== "RGB_PD_MODE_GROUP_ALL" && group.owner !== preview.mode) continue;
            paint(inherits(group.color) ? preview.color : group.color, "pointing");
        }
    }
    return {colour, source, layer: painter};
}

export function keyLight(model, layer, position, options = {}) {
    return ledLight(model, layer, LED_INDEX[position?.layoutIndex], position, options);
}

// The trackball LED is LED 56, soldered on the right half and wired to no key.
export function trackballLight(model, layer, options = {}) {
    return ledLight(model, layer, TRACKBALL_LED, undefined, options);
}

// Whether an LED's light is the viewed layer's own rather than seen through it
// from beneath: that layer or a previewed pointing mode painted it last, or the
// viewed layer is base, which nothing lies under — the base effect is the floor
// base sits on, always on with it, not a layer below it.
export const ownLight = (light, layerId = 0) =>
    layerId === 0 || light?.source === "pointing" || (light?.source === "layer" && light.layer === layerId);
