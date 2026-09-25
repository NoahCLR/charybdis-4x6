"use strict";

// Which keyboard layer each slot of a draft holds.
//
// A layer is neither its name, which a rename changes, nor its slot, which a
// reorder changes. A draft keeps an order beside each history entry —
// `order[slot]` is the keyboard layer that slot now holds — set from what
// Edit layers saved, never inferred from names or contents. The keyboard's
// profile rearranged into that order, every layer reference following, is the
// *reference* the review compares the draft with: a reorder is then one item,
// and everything else is compared layer with layer.

const {reorderLayers, validateSnapshot} = require("./portable-profile");

const IDENTITY = Object.freeze([0, 1, 2, 3, 4, 5, 6, 7]);
const isIdentity = order => order.every((layer, slot) => layer === slot);
// `step` rearranges a document already in `order`, as reorderLayers reads it:
// slot n takes what slot step[n] held.
const compose = (order, step) => Object.freeze(step.map(slot => order[slot]));
const inverse = order => {
    const result = [];
    order.forEach((layer, slot) => {result[layer] = slot;});
    return Object.freeze(result);
};

// A document rearranged by `step`: every layer moves with its keys, name,
// colour and settings, and every reference to it follows.
function rearranged(document, step) {
    const names = validateSnapshot(document).settings.names;
    return reorderLayers(document, [...step], step.map(slot => names[slot]), {keysFollow: true});
}

// A review unit named by the layer it belongs to rather than the slot it sits
// in, so edits made before and after a reorder link by what they touched.
function layerUnit(unit, order) {
    const layout = /^layout:(\d+):(\d+)$/.exec(unit);
    if (layout) return `layout@${order[layout[1]]}:${layout[2]}`;
    const name = /^layerName:(\d+)$/.exec(unit);
    if (name) return `layerName@${order[name[1]]}`;
    const colour = /^rgb:layer:(\d+)$/.exec(unit);
    if (colour) return `rgb:layer@${order[colour[1]]}`;
    return unit;
}

module.exports = {IDENTITY, compose, inverse, isIdentity, layerUnit, rearranged};
