"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {validateSnapshot} = require("../../core/model/portable-profile");
const {profileFindings, draftProfileChecks} = require("../../core/model/profile-checks");
const {document} = require("../fixtures/portable-profile");

test("whole-profile checks report inert actions and destination blockers", () => {
    const decoded = validateSnapshot(document());
    decoded.danglingPdBindings = {7: 2};
    decoded.document.macros[0] = Buffer.from("A".repeat(520)).toString("base64");
    const checks = profileFindings(decoded, {brightnessMax: 100, effects: [{id: 2}]});
    assert.equal(checks.find(row => row.kind === "inertPointing")?.count, 2);
    assert.equal(checks.find(row => row.kind === "unplayableMacro")?.place.index, 0);
    assert.deepEqual(checks.filter(row => row.level === "blocker").map(row => row.kind), ["brightnessBlocker", "effectBlocker"]);
});

test("whole-profile findings retain new and fixed status beside the keyboard", () => {
    const keyboard = validateSnapshot(document());
    const draft = validateSnapshot(document());
    draft.danglingPdBindings = {7: 1};
    const checks = draftProfileChecks(keyboard, draft, [0, 1, 2, 3, 4, 5, 6, 7], {});
    assert.equal(checks.find(row => row.kind === "inertPointing")?.status, "new");
});
