"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {encodeComboDomainV1, decodeComboDomainV1} = require("../../core/schema/combo-domain-v1");
const fs = require("node:fs"), path = require("node:path");
const golden = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/combo_domain_v1.fixture"), "utf8").trim();
const key = operand => ({kind: 1, operand, flags: 0});
const row = {inputs: [key(4), key(5)], output: key(41), termMs: 45, holdTermMs: 200, mustHold: false, mustTap: false, ordered: false};
test("combo domain preserves native and semantic actions, flags, timing and row order", () => {
    const rows = [row, {...row, inputs: [{kind: 2, operand: 1, flags: 0}, key(6)], ordered: true, mustTap: true}];
    const bytes = encodeComboDomainV1(rows);
    assert.equal(bytes.toString("hex"), golden);
    assert.deepEqual(decodeComboDomainV1(bytes), rows.map((value, id) => ({id, ...value})));
    assert.deepEqual(decodeComboDomainV1(encodeComboDomainV1([])), []);
    assert.equal(decodeComboDomainV1(encodeComboDomainV1(Array(32).fill(row))).length, 32);
});
test("combo domain rejects malformed, ambiguous and noncanonical records", () => {
    for (const change of [{inputs: [key(4)]}, {inputs: [key(4), key(4)]}, {inputs: [key(0), key(1)]}, {output: key(0)}, {termMs: -1}, {holdTermMs: 65536}]) assert.throws(() => encodeComboDomainV1([{...row, ...change}]));
    assert.throws(() => encodeComboDomainV1(Array(33).fill(row)));
    const bytes = encodeComboDomainV1([row]);
    for (const index of [1, 2, 3, 10, 11, 28]) {
        const bad = Buffer.from(bytes); bad[index] = 1;
        assert.throws(() => decodeComboDomainV1(bad));
    }
    assert.throws(() => decodeComboDomainV1(Buffer.concat([bytes, Buffer.from([0])])));
    assert.throws(() => decodeComboDomainV1(bytes.subarray(0, -1)));
});
