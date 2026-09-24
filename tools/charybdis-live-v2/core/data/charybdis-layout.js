"use strict";

// The Charybdis 4x6 key layout: inert data every layer may read.
//
// QMK keyboard.json -> layouts.LAYOUT.layout, in the same order as the
// authored LAYOUT(...) arguments. Keeping this contract beside the wire codec
// makes it impossible to confuse visual order with matrix row/column order.
const CHARYBDIS_4X6_LAYOUT_MATRIX = Object.freeze([
    [0, 0], [0, 1], [0, 2], [0, 3], [0, 4], [0, 5],
    [5, 5], [5, 4], [5, 3], [5, 2], [5, 1], [5, 0],
    [1, 0], [1, 1], [1, 2], [1, 3], [1, 4], [1, 5],
    [6, 5], [6, 4], [6, 3], [6, 2], [6, 1], [6, 0],
    [2, 0], [2, 1], [2, 2], [2, 3], [2, 4], [2, 5],
    [7, 5], [7, 4], [7, 3], [7, 2], [7, 1], [7, 0],
    [3, 0], [3, 1], [3, 2], [3, 3], [3, 4], [3, 5],
    [8, 5], [8, 4], [8, 3], [8, 2], [8, 1], [8, 0],
    [4, 3], [4, 4], [4, 1], [9, 1], [9, 3], [4, 5], [4, 2], [9, 5],
].map((position) => Object.freeze(position)));

module.exports = {CHARYBDIS_4X6_LAYOUT_MATRIX};
