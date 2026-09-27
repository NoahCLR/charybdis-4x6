import assert from "node:assert/strict";
import test from "node:test";
import {GROUP_ORDER, GROUP_TITLES, REACH_FIELDS, inGroupOrder, initialReachGroups, reachEntries, setReachGroupOpen} from "../webview/view/reach-groups.mjs";

test("every reach group has a title and a place in the one order", () => {
    assert.deepEqual(Object.keys(GROUP_TITLES).sort(), [...GROUP_ORDER].sort());
    assert.deepEqual(inGroupOrder([{id: "elsewhere"}, {id: "here"}, {id: "through"}]).map((group) => group.id), ["here", "through", "elsewhere"],
        "a tab's groups come out in the shared order whatever order it lists them in");
    assert.equal(GROUP_ORDER[0], "key", "the selected key's combos lead, above what the layer reaches");
    assert.deepEqual(GROUP_ORDER.slice(1, 3), ["view", "here"], "the composed view precedes the strict layer routes");
});

test("a group reads its own reach list, and a missing one reads as empty", () => {
    const reach = {inView: ["v"], onKeys: ["a"], throughKeys: ["b"], fromBranches: [], fromCombos: ["c"]};
    assert.deepEqual(reachEntries(reach, "view"), ["v"]);
    assert.deepEqual(reachEntries(reach, "here"), ["a"]);
    assert.deepEqual(reachEntries(reach, "combos"), ["c"]);
    assert.deepEqual(reachEntries(reach, "belowCombos"), []);
    assert.deepEqual(reachEntries(reach, "elsewhere"), [], "elsewhere is a tab's own remainder");
    assert.ok(Object.keys(REACH_FIELDS).every((id) => GROUP_ORDER.includes(id)));
});

test("reach sections share one state across Keys tabs and open independently", () => {
    const initial = initialReachGroups();
    assert.deepEqual(Object.keys(initial), GROUP_ORDER);
    assert.equal(initial.view, true);
    assert.equal(initial.here, false);
    const withLayer = setReachGroupOpen(initial, "here", true);
    const withCombos = setReachGroupOpen(withLayer, "combos", true);
    assert.equal(withCombos.view, true, "opening another route leaves the view open");
    assert.equal(withCombos.here, true, "opening a third route leaves this layer open");
    assert.equal(withCombos.combos, true);
    assert.equal(setReachGroupOpen(withCombos, "here", false).here, false,
        "collapsing a shared route is visible to every tab that has it");
    assert.equal(initial.here, false, "the initial state is not changed");
});
