"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {encodeComboDomain, decodeComboDomain, upgradeComboTable, effectiveComboTerm} = require("../../core/schema/combo-domain-v1");
const fs = require("node:fs"), path = require("node:path");
const golden = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/combo_domain_v1.fixture"), "utf8").trim();
const key = operand => ({kind: 1, operand, flags: 0});
const row = {inputs: [key(4), key(5)], output: key(41), termMs: 45, mustHold: false, mustTap: false, ordered: false};
const v1 = rows => ({version: 1, defaultTermMs: null, holdTermMs: rows.length ? 200 : null, rows});
const v2 = rows => ({version: 2, defaultTermMs: 60, holdTermMs: 150, rows});
const withIds = table => ({...table, rows: table.rows.map((value, id) => ({id, ...value}))});

test("combo domain v1 preserves native and semantic actions, flags, timing and row order", () => {
    const table = v1([row, {...row, inputs: [{kind: 2, operand: 1, flags: 0}, key(6)], ordered: true, mustTap: true}]);
    const bytes = encodeComboDomain(table);
    assert.equal(bytes.toString("hex"), golden);
    assert.deepEqual(decodeComboDomain(bytes, 1), withIds(table));
    assert.deepEqual(decodeComboDomain(encodeComboDomain(v1([])), 1), v1([]));
    assert.equal(decodeComboDomain(encodeComboDomain(v1(Array(32).fill(row))), 1).rows.length, 32);
});

test("combo domain v2 stores the default window and hold threshold once, and a row may follow the default", () => {
    const table = v2([{...row, termMs: null}, {...row, inputs: [key(4), key(6)]}]);
    const bytes = encodeComboDomain(table);
    assert.equal(bytes.length, 8 + 2 * 28);
    assert.deepEqual([...bytes.subarray(0, 8)], [2, 0, 0, 0, 60, 0, 150, 0]);
    // Window zero follows the default; the per-row hold field is gone.
    assert.deepEqual([...bytes.subarray(8 + 2, 8 + 8)], [0, 0, 0, 0, 0, 0]);
    assert.deepEqual([...bytes.subarray(36 + 2, 36 + 4)], [45, 0]);
    const decoded = decodeComboDomain(bytes, 2);
    assert.deepEqual(decoded, withIds(table));
    assert.equal(effectiveComboTerm(decoded, decoded.rows[0]), 60);
    assert.equal(effectiveComboTerm(decoded, decoded.rows[1]), 45);
    // An empty table keeps both values.
    assert.deepEqual(decodeComboDomain(encodeComboDomain(v2([])), 2), v2([]));
});

test("a v1 table upgrades to v2 with every window its own", () => {
    const table = v1([row]);
    const upgraded = upgradeComboTable(table, {defaultTermMs: 50, holdTermMs: 300});
    assert.deepEqual(upgraded, {version: 2, defaultTermMs: 50, holdTermMs: 200, rows: [row]});
    assert.equal(upgradeComboTable(v1([]), {defaultTermMs: 50, holdTermMs: 300}).holdTermMs, 300);
    assert.equal(upgradeComboTable(upgraded, {defaultTermMs: 70, holdTermMs: 0}), upgraded);
});

test("combo domain rejects malformed, ambiguous and noncanonical records", () => {
    for (const change of [{inputs: [key(4)]}, {inputs: [key(4), key(4)]}, {inputs: [key(0), key(1)]}, {output: key(0)}, {termMs: -1}, {termMs: null}]) assert.throws(() => encodeComboDomain(v1([{...row, ...change}])));
    assert.throws(() => encodeComboDomain({...v1([row]), holdTermMs: 65536}));
    assert.throws(() => encodeComboDomain({...v1([row]), defaultTermMs: 50}), /no default/);
    assert.throws(() => encodeComboDomain(v1(Array(33).fill(row))));
    // Version 2 has no zero window of its own, and no zero default.
    assert.throws(() => encodeComboDomain(v2([{...row, termMs: 0}])));
    assert.throws(() => encodeComboDomain({...v2([row]), defaultTermMs: 0}));
    assert.throws(() => encodeComboDomain({...v2([row]), holdTermMs: null}));
    assert.throws(() => encodeComboDomain({...v2([row]), version: 3}));
    const bytes = encodeComboDomain(v1([row]));
    for (const index of [1, 2, 3, 10, 11, 28]) {
        const bad = Buffer.from(bytes); bad[index] = 1;
        assert.throws(() => decodeComboDomain(bad, 1));
    }
    assert.throws(() => decodeComboDomain(Buffer.concat([bytes, Buffer.from([0])]), 1));
    assert.throws(() => decodeComboDomain(bytes.subarray(0, -1), 1));
    assert.throws(() => decodeComboDomain(bytes, 2));
    const two = encodeComboDomain(v1([row, {...row, inputs: [key(4), key(6)]}]));
    two[32 + 4] = 199; // a second hold threshold
    assert.throws(() => decodeComboDomain(two, 1), /one hold threshold/);
    const current = encodeComboDomain(v2([row]));
    for (const [index, value] of [[1, 1], [4, 0], [12, 1], [13, 1]]) {
        const bad = Buffer.from(current); bad[index] = value; if (index === 4) bad[5] = 0;
        assert.throws(() => decodeComboDomain(bad, 2));
    }
});
