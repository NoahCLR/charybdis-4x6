"use strict";

// Classify the expression currently displayed, including unsaved edits. A
// position's numeric value can still describe the previous device read.
function layoutKeyKind(expression) {
    const key = String(expression || "").trim();
    if (["KC_TRANSPARENT", "KC_TRNS", "_______"].includes(key) || /^0x0*1$/i.test(key)) return "transparent";
    if (["KC_NO", "XXXXXXX"].includes(key) || /^0x0+$/i.test(key)) return "disabled";
    return "mapped";
}

function layerPreviewPaint(rgb, layerColor, expression) {
    if (rgb?.layerColorsEnabled === false || !layerColor?.color) return undefined;
    if (layerColor.mode === "KEYS_MAPPED_ON_THIS_LAYER_ONLY" && layoutKeyKind(expression) !== "mapped") return undefined;
    if (!["ALL_KEYS", "KEYS_MAPPED_ON_THIS_LAYER_ONLY"].includes(layerColor.mode)) return undefined;
    // Pass-through leaves the lower layer or base effect untouched.
    const {s, v} = layerColor.color;
    if (Number(s) === 0 && Number(v) === 0) return undefined;
    return layerColor.color;
}

function composedLayerPreviewPaint(rgb, baseLayer, selectedLayer, position, ledIndex) {
    const effect = rgb?.baseEffect;
    let color = effect?.state === "read" ? effect.previewColor : undefined;
    // QMK skips indicators too when its RGB Matrix mode is off.
    if (effect?.state === "read" && effect.enabled === false) return color;
    if (rgb?.layerColorsEnabled === false) return color;

    const layers = baseLayer && baseLayer.name !== selectedLayer?.name ? [baseLayer, selectedLayer] : [selectedLayer];
    const selected = layers.filter(Boolean).map(layer => ({
        name: layer.name,
        row: rgb?.layerColors?.find(row => row.layer === layer.name),
        key: layer.name === selectedLayer?.name ? position : layer.positions?.find(key => key.layoutIndex === position.layoutIndex),
    }));
    // Firmware paints the layer colours first, from base to highest, then
    // processes all LED-group rows in their reported order.
    for (const layer of selected) {
        const paint = layerPreviewPaint(rgb, layer.row, layer.key?.keycode || "KC_NO");
        if (paint) color = paint;
    }
    for (const group of rgb?.layerLedGroups || []) {
        if (!group.ledIndices?.includes(ledIndex)) continue;
        for (const layer of selected) {
            if (group.owner !== "RGB_LAYER_GROUP_ALL" && group.owner !== layer.name) continue;
            const inherited = group.color && ["h", "s", "v"].every(channel => Number(group.color[channel]) === 0);
            const paint = inherited ? layer.row?.color : group.color;
            // An inheriting group on a pass-through layer paints nothing.
            // A literal HSV(h,0,0) with h != 0 is black, not inheritance.
            if (paint && (!inherited || Number(paint.s) !== 0 || Number(paint.v) !== 0)) color = paint;
        }
    }
    return color;
}

function baseEffectPreviewNote(effect) {
    if (effect?.state !== "read") return "Base effect unavailable; unpainted keys use a neutral placeholder. Use Read from keyboard to refresh.";
    if (!effect.enabled) return "RGB was off at the last read.";
    if (!effect.previewColor) return effect.effectName + " is not simulated; unpainted keys use a neutral placeholder. Use Read from keyboard to refresh.";
    return "Using the last-read base effect. Use Read from keyboard to refresh; brightness is approximate.";
}

module.exports = {layoutKeyKind, layerPreviewPaint, composedLayerPreviewPaint, baseEffectPreviewNote};
