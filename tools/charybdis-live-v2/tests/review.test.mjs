import assert from "node:assert/strict";
import test from "node:test";
import {discardLabel, draftMarks, placeState, reviewBlocks, statusSummary, stillShown} from "../webview/view/review.mjs";

const item = (area, title, group, extra = {}) => ({area, title, group, status: "changed", fields: [], ...extra});

test("items made together form one block under the area they start in, shown once", () => {
    const changes = [
        item("Behaviours", "Esc", 0, {groupTitle: "Moved a behaviour", status: "removed"}),
        item("Behaviours", "Q", 0, {groupTitle: "Moved a behaviour", status: "added"}),
        item("Layout", "Base · Left · row 1, column 1", 0, {groupTitle: "Moved a behaviour"}),
        item("Layout", "Base · Left thumb 1", 1),
        item("Macros", "Macro 3", 2),
    ];
    const sections = reviewBlocks(changes);
    assert.deepEqual(sections.map((section) => section.area), ["Layout", "Behaviours", "Macros"], "areas in rail order");
    const behaviours = sections.find((section) => section.area === "Behaviours");
    assert.equal(behaviours.blocks.length, 1);
    assert.deepEqual(behaviours.blocks[0].items.map((entry) => entry.title), ["Esc", "Q", "Base · Left · row 1, column 1"],
        "the group keeps its Layout item rather than splitting it off");
    assert.equal(behaviours.blocks[0].title, "Moved a behaviour");
    assert.equal(behaviours.count, 3);
    assert.equal(sections.find((section) => section.area === "Layout").count, 1);
});

test("the header counts items by what happened to them", () => {
    const changes = [item("Layout", "a", 0), item("Layout", "b", 1, {status: "added"}), item("Macros", "c", 2, {status: "added"})];
    assert.equal(statusSummary(changes), "2 added · 1 changed");
});

test("a Discard says how much it takes back", () => {
    assert.equal(discardLabel({items: [1]}), "Discard");
    assert.equal(discardLabel({items: [1, 2]}), "Discard both");
    assert.equal(discardLabel({items: [1, 2, 3]}), "Discard all 3");
});

test("Show goes to where each kind of item is edited", () => {
    const layers = [{index: 0}, {index: 1}, {index: 2}];
    assert.deepEqual(placeState({kind: "key", layer: 2, layoutIndex: 13}, layers), {screen: "keys", tab: "key", layer: 2, selected: 13});
    assert.equal(placeState({kind: "behaviour", keycode: "KC_ESCAPE"}).behaviourRow, "KC_ESCAPE");
    assert.equal(placeState({kind: "macro", index: 4}).macroSlot, "VIA_MACRO_4");
    assert.equal(placeState({kind: "pointing", slot: 6}).pdSlot, 6);
    assert.equal(placeState({kind: "lighting", stage: "pd"}).stage, "pd");
    assert.equal(placeState(null), null, "the profile fallback has nowhere to go");
    const combo = placeState({kind: "combo", index: 3});
    assert.equal(combo.pickCombo, 3, "a combo is asked for by id, since its group depends on the layer");
    assert.equal(combo.reveal, '[data-reach][data-picked="true"]');
    const section = placeState({kind: "settings", section: "keyTiming"});
    assert.deepEqual(section.settingsOpen, ["keyTiming"], "a folded section opens");
    assert.equal(section.reveal, '.settings-group[data-section="keyTiming"]');
    assert.equal(placeState({kind: "settings"}).reveal, undefined, "settings with no section just open the screen");
});

test("a removed thing keeps its mark and its Show only where it is still on screen", () => {
    const removed = (place) => ({status: "removed", place});
    assert.equal(stillShown(removed({kind: "pointing", slot: 4})), true, "a cleared slot is still a card");
    assert.equal(stillShown(removed({kind: "macro", index: 2})), true, "an emptied macro is still a slot");
    assert.equal(stillShown(removed({kind: "behaviour", keycode: "KC_2"})), false, "a removed behaviour has no row left");
    assert.equal(stillShown(removed({kind: "combo", index: 1})), false);
    const marks = draftMarks([removed({kind: "pointing", slot: 4}), removed({kind: "behaviour", keycode: "KC_2"})]);
    assert.deepEqual([...marks.pointing], [4]);
    assert.equal(marks.behaviours.size, 0);
});
