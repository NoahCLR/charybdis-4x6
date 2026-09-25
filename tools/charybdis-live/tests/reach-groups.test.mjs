import assert from "node:assert/strict";
import test from "node:test";
import {GROUP_ORDER, GROUP_TITLES, REACH_FIELDS, inGroupOrder, reachEntries} from "../webview/view/reach-groups.mjs";

test("every reach group has a title and a place in the one order", () => {
    assert.deepEqual(Object.keys(GROUP_TITLES).sort(), [...GROUP_ORDER].sort());
    assert.deepEqual(inGroupOrder([{id: "elsewhere"}, {id: "here"}, {id: "through"}]).map((group) => group.id), ["here", "through", "elsewhere"],
        "a tab's groups come out in the shared order whatever order it lists them in");
});

test("a group reads its own reach list, and a missing one reads as empty", () => {
    const reach = {onKeys: ["a"], throughKeys: ["b"], fromBranches: [], fromCombos: ["c"]};
    assert.deepEqual(reachEntries(reach, "here"), ["a"]);
    assert.deepEqual(reachEntries(reach, "combos"), ["c"]);
    assert.deepEqual(reachEntries(reach, "belowCombos"), []);
    assert.deepEqual(reachEntries(reach, "elsewhere"), [], "elsewhere is a tab's own remainder");
    assert.ok(Object.keys(REACH_FIELDS).every((id) => GROUP_ORDER.includes(id)));
});
