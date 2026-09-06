"use strict";

// A selected layer over Layer 0 is a preview, not an observed active stack.
function combosForLayerPreview(model, layer, canonical = value => value) {
    if (!layer || model.comboReadback?.state !== "read" || !model.comboReadback.enabled) return [];
    const layers = model.layers || [];
    const reference = model.comboReadback.layerReferences[layer.index];
    const base = layers.find(candidate => candidate.index === 0);
    const referenced = layers.find(candidate => candidate.index === reference);
    if (!referenced) return [];
    const keys = layer.positions.map(position => {
        let key = position.keycode;
        if (model.comboReadback.fixedReference || reference !== layer.index) {
            key = referenced.positions.find(candidate => candidate.layoutIndex === position.layoutIndex)?.keycode;
        } else if (["KC_TRANSPARENT", "KC_TRNS", "_______"].includes(key)) {
            key = base?.positions.find(candidate => candidate.layoutIndex === position.layoutIndex)?.keycode;
        }
        return {layoutIndex: position.layoutIndex, key: canonical(key || "KC_NO")};
    });
    return (model.combos || []).flatMap(combo => {
        const matches = combo.inputs.map(input => keys.filter(position => position.key === canonical(input)));
        if (matches.some(positions => !positions.length)) return [];
        return [{...combo, inputPositions: [...new Set(matches.flat().map(position => position.layoutIndex))]}];
    });
}

module.exports = {combosForLayerPreview};
