import assert from "node:assert/strict";
import test from "node:test";
import {categorySummary, discardLabel, draftMarks, groupNote, placeState, reviewBlocks, statusSummary, stillShown} from "../webview/view/review.mjs";

const item = (area, title, group, extra = {}) => ({area, title, group, status: "changed", fields: [], ...extra});

test("every item sits under its own area; a group that spans areas shows its part in each", () => {
    const changes = [
        item("Behaviours", "Esc", 0, {groupTitle: "Moved a behaviour", status: "removed"}),
        item("Layout", "Base · Left · row 1, column 1", 0, {groupTitle: "Moved a behaviour"}),
        item("Behaviours", "Q", 0, {groupTitle: "Moved a behaviour", status: "added"}),
        item("Layout", "Base · Left thumb 1", 1),
        item("Macros", "Macro 3", 2),
        item("Lighting", "Navigation colour", 0, {groupTitle: "Moved a behaviour"}),
    ];
    const sections = reviewBlocks(changes);
    assert.deepEqual(sections.map((section) => section.area), ["Layout", "Behaviours", "Macros", "Lighting"], "areas in rail order");
    const [layout, behaviours, , lighting] = sections;
    assert.deepEqual(layout.blocks.map((block) => block.items.map((entry) => entry.title)),
        [["Base · Left · row 1, column 1"], ["Base · Left thumb 1"]], "the group's key sits with the other keys, in host order");
    assert.equal(layout.count, 2);
    assert.deepEqual(behaviours.blocks[0].items.map((entry) => entry.title), ["Esc", "Q"], "a group's items in one area stay one block");
    assert.equal(behaviours.count, 2);
    for (const part of [layout.blocks[0], behaviours.blocks[0], lighting.blocks[0]]) {
        assert.equal(part.group, 0);
        assert.equal(part.title, "Moved a behaviour");
        assert.equal(part.size, 4, "every part knows the whole group, for its Discard");
    }
    assert.deepEqual(behaviours.blocks[0].elsewhere, ["Layout", "Lighting"], "the other areas it reaches, in rail order");
    assert.equal(layout.blocks[1].size, 1);
    assert.deepEqual(layout.blocks[1].elsewhere, []);
});

test("a recovery item with no group is a block of its own", () => {
    const sections = reviewBlocks([item("Recovery", "Complete profile", null), item("Recovery", "Other", null)]);
    assert.deepEqual(sections[0].blocks.map((block) => block.size), [1, 1]);
});

test("a group's header says how much of it is here and where the rest is", () => {
    const [layout, behaviours] = reviewBlocks([
        item("Layout", "a", 0), item("Behaviours", "b", 0), item("Behaviours", "c", 0), item("Combos", "d", 0), item("Lighting", "e", 0),
        item("Layout", "f", 1), item("Layout", "g", 1),
    ]);
    assert.equal(groupNote(layout.blocks[0]), "1 of 5 here, the rest in Behaviours, Combos and Lighting · discarded together");
    assert.equal(groupNote(behaviours.blocks[0]), "2 of 5 here, the rest in Layout, Combos and Lighting · discarded together");
    assert.equal(groupNote(layout.blocks[1]), "2 changes, discarded together");
});

test("the header counts items by what happened to them", () => {
    const changes = [item("Layout", "a", 0), item("Layout", "b", 1, {status: "added"}), item("Macros", "c", 2, {status: "added"})];
    assert.equal(statusSummary(changes), "2 added · 1 changed");
});

test("a Discard says how much it takes back", () => {
    assert.equal(discardLabel({size: 1, items: [1]}), "Discard");
    assert.equal(discardLabel({size: 2, items: [1, 2]}), "Discard both");
    assert.equal(discardLabel({size: 81, items: [1, 2, 3]}), "Discard all 81", "a part of a group names the whole group");
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
    assert.equal(section.screen, "settings");
    assert.equal(placeState({kind: "settings"}).reveal, undefined, "settings with no section just open the screen");
    const mouse = placeState({kind: "settings", section: "sniping", area: "Mouse"});
    assert.equal(mouse.screen, "mouse", "a Mouse section opens on the Mouse screen");
    assert.equal(mouse.reveal, '.settings-group[data-section="sniping"]');
    assert.deepEqual(placeState({kind: "settings", section: "automouseFade", area: "Lighting", stage: "auto"}), {screen: "lighting", stage: "auto"},
        "a section timed on a lighting stage opens that stage");
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

test("a reorder marks the layers it moved, not their keys", () => {
    const marks = draftMarks([{status: "changed", place: {kind: "layers", layers: [2, 3]}}]);
    assert.deepEqual([...marks.layers], [2, 3]);
    assert.equal(marks.keys.size, 0);
    assert.equal(marks.layerNames, true);
});

test("a settings section timed on a lighting stage marks that stage's tab too", () => {
    const marks = draftMarks([{status: "changed", place: {kind: "settings", section: "automouseFade", area: "Lighting", stage: "auto"}}]);
    assert.deepEqual([...marks.settings], ["automouseFade"]);
    assert.deepEqual([...marks.lighting], ["auto"]);
});

test("a profile file's differences are counted by what they configure, split as each screen is", () => {
    const at = (place, extra = {}) => ({status: "changed", place, ...extra});
    const stages = [{id: "base", label: "Base effect"}, {id: "layers", label: "Layer colours"}, {id: "auto", label: "Auto-mouse fade"}, {id: "key", label: "Key feedback"}];
    const groups = categorySummary([
        at({kind: "pointing", slot: 2}), at({kind: "settings", section: "sniping"}), at({kind: "settings", section: "autoMouse"}),
        at({kind: "lighting", stage: "key"}), at({kind: "settings", section: "lightingFeedback"}), at({kind: "lighting", stage: "layers", layer: 3}),
        at({kind: "lighting", stage: null}), at({kind: "settings", section: "rgbAppearance"}),
        at({kind: "settings", section: "automouseFade", area: "Lighting", stage: "auto"}),
        at({kind: "combo", index: 7}, {status: "removed"}), at({kind: "key", layer: 3}), at({kind: "key", layer: 0}), at({kind: "key", layer: 3}),
        at({kind: "settings", section: "keyTiming"}), at({kind: "behaviour"}, {status: "added"}), at({kind: "layers"}, {unit: "layerName:2"}),
        at({kind: "settings"}, {unit: "settings:otherKeyOptions"}), at({kind: "macro", index: 4}), at(null, {unit: "profile", title: "Stored profile"}),
    ], {names: ["Base", "Numbers", "Symbols", "Navigation"], stages});
    assert.deepEqual(groups.map((group) => [group.category, group.count]),
        [["Keys", 8], ["Lighting", 6], ["Macros", 1], ["Mouse", 2], ["Pointing modes", 1], ["Other", 1]], "the rail's order");
    const rows = (category) => groups.find((group) => group.category === category).rows.map((row) => `${row.label} ${row.count}`);
    assert.deepEqual(rows("Keys"), ["Keys on Base 1", "Keys on Navigation 2", "Layer names 1", "Behaviours 1", "Key timing 1", "Combos 1", "Key options 1"],
        "the board's keys by layer first, then the rest of the Keys screen");
    assert.deepEqual(rows("Lighting"), ["Stages on or off 1", "Base effect 1", "Layer colours 1", "Auto-mouse fade 1", "Key feedback 2"],
        "stages in paint order; a setting joins what it tunes, and one timed on a stage counts with it");
    assert.deepEqual(rows("Mouse"), ["Auto-sniping 1", "Auto-mouse 1"], "the pointer's settings are filed with Mouse");
    assert.deepEqual(rows("Pointing modes"), [], "the slots are the whole category");
    assert.deepEqual(rows("Other"), ["Stored profile 1"]);
    assert.deepEqual(rows("Macros"), [], "a category that is its one row shows no rows");
    assert.equal(groups[0].status, "1 added · 6 changed · 1 removed");
});
