// What the keyboard would light, from what the keyboard reported.
//
// Every surface that shows colour asks this module, so the board, the key
// dots, the swatches and the review all answer the same way — and they answer
// from the draft, because the draft is what the model carries.

import {hsv, isOff} from "../lib/colour.mjs";
import {inLocality} from "./geometry.mjs";

const OFF = {h: "0", s: "0", v: "0"};

// The order the firmware paints in. Names match the stage labels the device
// view produces; the base effect is read separately and has no stage bit.
export const STAGE_ORDER = [
    {id: "base", label: "Base effect"},
    {id: "layers", label: "Layer colours"},
    {id: "auto", label: "Auto-mouse fade"},
    {id: "pd", label: "Pointing modes"},
    {id: "combo", label: "Combo feedback"},
    {id: "key", label: "Key behaviour feedback"},
];

export const PD_MODE_IDS = [
    "PD_MODE_DRAGSCROLL", "PD_MODE_VOLUME", "PD_MODE_BRIGHTNESS", "PD_MODE_ZOOM",
    "PD_MODE_ARROW", "PD_MODE_PINCH", "PD_MODE_SLOT_6", "PD_MODE_SLOT_7",
];

export function stageEnabled(model, id) {
    if (id === "base") return Boolean(model?.rgb?.baseEffect?.enabled);
    const entry = STAGE_ORDER.find((stage) => stage.id === id);
    const stage = (model?.rgb?.stages || []).find((row) => row.label === entry?.label);
    return Boolean(stage?.enabled);
}

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

export const comboColour = (model) => model?.rgb?.comboFeedback?.color || OFF;

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
    Boolean(position) && !["KC_TRANSPARENT", "KC_TRNS", "_______", "KC_NO", "XXXXXXX"].includes(position.keycode);

/**
 * The colour one key would show with nothing held: base effect, then this
 * layer's colour on the keys it owns. A pointing-mode colour is an overlay
 * that exists only while its mode runs, so it is painted only when the caller
 * asks for that preview, and then by locality rather than on the trigger key.
 */
export function keyLight(model, layer, position, options = {}) {
    const base = stageEnabled(model, "base") ? baseColour(model) : null;
    let colour = base || OFF, source = base ? "base" : "off";

    const row = layerColourRow(model, layer?.index);
    if (stageEnabled(model, "layers") && row && !isOff(row.color)) {
        const paintsEveryKey = row.mode === "ALL_KEYS";
        if (paintsEveryKey || isMapped(position)) {
            colour = row.color;
            source = "layer";
        }
    }

    const preview = options.pdActive;
    if (preview && stageEnabled(model, "pd") && !isOff(preview.color)
        && inLocality(position.layoutIndex, preview.locality, preview.triggerIndex)) {
        colour = preview.color;
        source = "pointing";
    }
    return {colour, source};
}

export const hsvTuple = hsv;
