"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../../core/data/charybdis-layout");
const protocol = require("../../core/protocol/via-layout-v1");

test("the layout names 56 distinct matrix positions, and the wire codec uses the same table", () => {
    assert.equal(CHARYBDIS_4X6_LAYOUT_MATRIX.length, 56);
    assert.equal(new Set(CHARYBDIS_4X6_LAYOUT_MATRIX.map(([row, column]) => row * 6 + column)).size, 56);
    assert.ok(CHARYBDIS_4X6_LAYOUT_MATRIX.every(([row, column]) => row >= 0 && row < 10 && column >= 0 && column < 6));
    assert.equal(protocol.CHARYBDIS_4X6_LAYOUT_MATRIX, CHARYBDIS_4X6_LAYOUT_MATRIX);
});
