import assert from "node:assert/strict";
import test from "node:test";
import {behaviourFor, comboEditInputs, comboReferenceLayer, behaviourListeningTo, canonicalKeycode, behaviourGridSteps, behaviourGroups, behaviourTiers, macroReach, pointingReach, reachablePositions, resolvedPositions, bindingKeycode, bindingsForSlot, comboGroups, combosForKey, keyFace, keyMeaning, macroKeycodes, pointingSlotFor, slotKeycodes} from "../webview/view/keyface.mjs";

test("a key face uses the model's own resolution, and names the layer a dual-role key reaches", () => {
    assert.deepEqual(keyFace({keycode: "KC_TRANSPARENT", display: "▽"}), {main: "▽", sub: "", kind: "transparent"});
    assert.deepEqual(keyFace({keycode: "KC_NO", display: ""}), {main: "", sub: "", kind: "disabled"});
    assert.deepEqual(keyFace({keycode: "KC_A", display: "A"}), {main: "A", sub: "", kind: "key"});
    assert.deepEqual(keyFace({keycode: "LT(LAYER_NAV,KC_F)", display: "F"}), {main: "F", sub: "nav", kind: "layer"});
    assert.deepEqual(keyFace({keycode: "MO(LAYER_SYM)", display: "L2"}), {main: "L2", sub: "momentary", kind: "layer"});
    assert.equal(keyFace(undefined).kind, "none");
});

test("tiers count the branches that use them, so a key shows what it can do", () => {
    const model = {keyBehaviors: [{
        keycode: "LEFT_THUMB",
        steps: [
            {tapCount: 0, tap: {action: "LOCK_LAYER(LAYER_SYM)"}, hold: {action: "MO(LAYER_SYM)"}},
            {tapCount: 1, tap: {action: "KC_MPLY"}, hold: {action: "KC_ESC"}, longHold: {action: "LOCK_LAYER(LAYER_NUM)"}},
            {tapCount: 2, tap: {action: "KC_MNXT"}, longHold: {action: "KC_MNXT"}},
        ],
    }]};
    const behaviour = behaviourFor(model, "LEFT_THUMB");
    assert.deepEqual(behaviourTiers(behaviour), [{kind: "tap", count: 3}, {kind: "hold", count: 2}, {kind: "long", count: 2}]);
    assert.deepEqual(behaviourTiers(undefined), []);
    assert.equal(behaviourFor(model, "KC_A"), undefined);
});

test("the behaviour editor keeps every supported tap column visible", () => {
    const behaviour = {steps: [{tapCount: 0, tap: {action: "KC_A"}}, {tapCount: 3, hold: {action: "KC_B"}}]};
    const steps = behaviourGridSteps(behaviour, 5);
    assert.deepEqual(steps.map((step) => step.tapCount), [0, 1, 2, 3, 4]);
    assert.equal(steps[0], behaviour.steps[0], "populated cells keep the device-backed row");
    assert.deepEqual(steps[1], {tapCount: 1}, "missing cells are empty editor slots");
    assert.equal(steps[3], behaviour.steps[1]);
    assert.equal(behaviourGridSteps(behaviour, 3).length, 3, "a lower advertised device limit is respected");
});

test("combos follow the device's own input positions before falling back to keycodes", () => {
    const model = {combos: [
        {badge: "C1", inputs: ["KC_D", "KC_F"], inputPositions: [27, 28], output: "KC_ESC"},
        {badge: "C2", inputs: ["KC_J", "KC_K"], output: "KC_TAB"},
    ]};
    assert.deepEqual(combosForKey(model, {layoutIndex: 27, keycode: "KC_D"}).map((c) => c.badge), ["C1"]);
    assert.deepEqual(combosForKey(model, {layoutIndex: 99, keycode: "KC_J"}).map((c) => c.badge), ["C2"]);
    assert.deepEqual(combosForKey(model, {layoutIndex: 99, keycode: "KC_Z"}), []);
});

test("macro and pointing-mode keycodes resolve to the slots the keyboard reported", () => {
    assert.deepEqual(macroKeycodes("VIA_MACRO_11"), ["VIA_MACRO_11"]);
    assert.deepEqual(macroKeycodes("KC_A"), []);
    const model = {pdModes: [{id: 0, name: "Dragscroll", kind: 2}, {id: 6, name: "", kind: 0}]};
    assert.equal(pointingSlotFor(model, "DRAGSCROLL").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "DRAGSCROLL_LOCK").name, "Dragscroll", "the lock keycode reaches the same slot");
    assert.equal(pointingSlotFor(model, "PD_SLOT_6").id, 6);
    assert.equal(pointingSlotFor(model, "KC_A"), undefined);
});

test("a key for an empty pointing slot says so on its second line", () => {
    // The catalogue label carries "(empty)" so the picker can tell the two
    // apart; on the cap that belongs under the name, not inside it.
    assert.deepEqual(keyFace({keycode: "0x7EF0", display: "Slot 7 · hold (empty)"}),
        {main: "Slot 7 · hold", sub: "empty", kind: "key"});
    assert.equal(keyFace({keycode: "QK_USER_16", display: "Dragscroll · hold"}).sub, "");
});

test("what a key reaches is looked up by what its value means", () => {
    // The keyboard stores a behaviour target, a macro and a pointing mode as
    // plain user keycodes; every domain that refers back to them uses the
    // semantic name. A lookup that matched the stored name would find nothing.
    const model = {
        keyBehaviors: [{keycode: "DRAGSCROLL", steps: [{tapCount: 1, hold: {action: "KC_ESC"}}]}],
        combos: [{badge: "C1", inputs: ["VIA_MACRO_0", "KC_F"], output: "KC_ESC"}],
        pdModes: [{id: 0, name: "Dragscroll", kind: 2}],
    };
    const pointingKey = {layoutIndex: 3, keycode: "QK_USER_16", semantic: "DRAGSCROLL"};
    const macroKey = {layoutIndex: 4, keycode: "QK_MACRO_0", semantic: "VIA_MACRO_0"};

    assert.equal(keyMeaning(pointingKey), "DRAGSCROLL");
    assert.equal(keyMeaning({keycode: "KC_A"}), "KC_A", "an ordinary key means itself");
    assert.equal(keyMeaning(undefined), "");
    assert.equal(behaviourFor(model, keyMeaning(pointingKey)).keycode, "DRAGSCROLL");
    assert.deepEqual(macroKeycodes(keyMeaning(macroKey)), ["VIA_MACRO_0"]);
    assert.equal(pointingSlotFor(model, keyMeaning(pointingKey)).name, "Dragscroll");
    assert.deepEqual(combosForKey(model, macroKey).map((combo) => combo.badge), ["C1"],
        "a combo input named semantically still finds its key");
});

test("a pointing key is recognised as the keyboard names it, not only as the app labels it", () => {
    // Layout positions carry the device's own name for the value: a bare user
    // keycode for the six named modes, and plain hex for a slot the shipped
    // vocabulary never named. Both must reach the slot, or a bound key would
    // read as an ordinary key on the board and in the hover card.
    const model = {
        pdModes: [{id: 0, name: "Dragscroll", kind: 2}, {id: 6, name: "", kind: 0}],
        qmkKeycodeAliases: {QK_USER_16: "DRAGSCROLL", QK_USER_22: "DRAGSCROLL_LOCK", "0x7EF0": "PD_SLOT_6"},
    };
    assert.equal(pointingSlotFor(model, "QK_USER_16").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "QK_USER_22").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "0x7EF0").id, 6);
    assert.equal(pointingSlotFor(model, "0x7EF1").id, 6, "the toggle keycode of an unnamed slot too");
    assert.equal(pointingSlotFor(model, 0x7ef0).id, 6);
    assert.equal(pointingSlotFor(model, "0x0041"), undefined, "an ordinary value is not a pointing key");
});

test("what still reaches a pointing slot is listed, so an inert key can be named", () => {
    const slot = {id: 0, kind: 0, name: ""};
    const model = {
        layers: [
            {name: "Layer 0", displayName: "Base", positions: [
                {layoutIndex: 3, keycode: "DRAGSCROLL"}, {layoutIndex: 4, keycode: "KC_A"}]},
            {name: "Layer 3", displayName: "Navigation", positions: [
                {layoutIndex: 3, keycode: "DRAGSCROLL_LOCK"}]},
        ],
        keyBehaviors: [
            {keycode: "LEFT_THUMB", steps: [{tapCount: 1, hold: {action: "DRAGSCROLL"}}]},
            {keycode: "RIGHT_THUMB", steps: [{tapCount: 0, tap: {action: "KC_ESC"}}]},
        ],
    };
    const found = bindingsForSlot(model, slot);
    assert.equal(found.keys.length, 2, "both the hold and the lock keycode count");
    assert.deepEqual(found.layers, ["Base", "Navigation"]);
    assert.deepEqual(found.behaviours.map((row) => row.keycode), ["LEFT_THUMB"]);
    assert.equal(bindingKeycode({id: 6}), "PD_SLOT_6", "slots past the named six use their number");
    assert.deepEqual(bindingsForSlot(model, undefined).keys, []);
});

test("an empty slot's bindings are found by the keyboard's own values, not by a name it has none of", () => {
    // A cleared slot has no name for the model to resolve, so its keys arrive
    // as whatever the catalogue calls the raw value. The slot still answers to
    // the same two numbers, so that is what the lookup matches on.
    assert.deepEqual(slotKeycodes({id: 0}), [0x7e50, 0x7e56], "hold and toggle for a named slot");
    assert.deepEqual(slotKeycodes({id: 6}), [0x7ef0, 0x7ef1], "and for a numbered one");
    const model = {
        qmkKeycodes: [{keycode: 0x7ef0, value: "QK_USER_32"}],
        layers: [{name: "Layer 0", displayName: "Base", positions: [
            {layoutIndex: 5, keycode: "QK_USER_32", value: 0x7ef0},
            {layoutIndex: 6, keycode: "KC_A", value: 0x0004},
        ]}],
        keyBehaviors: [{keycode: "LEFT_THUMB", steps: [{tapCount: 0, hold: {action: "QK_USER_32"}}]}],
    };
    const found = bindingsForSlot(model, {id: 6, kind: 0});
    assert.equal(found.keys.length, 1, "the key bound to the empty slot is found");
    assert.deepEqual(found.behaviours.map((row) => row.keycode), ["LEFT_THUMB"]);
});

test("the combo table and the board's badges count the same keys", () => {
    // Real keyboards do not always report per-layer input references, and when
    // they do not, the board matched by keycode while the table looked at the
    // missing field and called every combo unreachable.
    const combo = {badge: "C1", inputs: ["KC_D", "LT(3,KC_F)"], output: "KC_TAB"};
    const layer = {name: "Layer 0", positions: [
        {layoutIndex: 27, keycode: "KC_D"}, {layoutIndex: 28, keycode: "LT(3,KC_F)"}, {layoutIndex: 29, keycode: "KC_G"}]};
    const model = {combos: [combo], layers: [layer]};

    const whole = comboGroups(model, [layer], 0);
    assert.deepEqual(whole.onKeys[0].keys.map((key) => key.position.layoutIndex), [27, 28]);
    assert.deepEqual(combosForKey(model, layer.positions[0]).map((row) => row.badge), ["C1"],
        "the badge and the count come from one predicate");
    assert.deepEqual(combosForKey(model, layer.positions[2]), []);

    const half = {name: "Layer 3", positions: [{layoutIndex: 27, keycode: "KC_D"}]};
    const partial = comboGroups({combos: [combo]}, [half], 0);
    assert.deepEqual(partial.onKeys, [], "a partly present combo is not reachable");
    assert.equal(partial.elsewhere[0].keys.length, 1, "and the table still says how much of it is here");

    // A device that does report positions still wins, on those positions.
    const reported = {badge: "C2", inputs: ["KC_N", "KC_M"], inputPositions: [42]};
    const byPosition = comboGroups({combos: [reported]},
        [{positions: [{layoutIndex: 42, keycode: "KC_X"}, {layoutIndex: 43, keycode: "KC_M"}]}], 0);
    assert.deepEqual(byPosition.elsewhere[0].keys.map((key) => key.position.layoutIndex), [42]);
});

test("combos group by whether this layer produces all of their inputs", () => {
    const combo = {id: 1, badge: "C1", inputs: ["KC_D", "KC_F"], output: "KC_TAB"};
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const stack = [
        {index: 0, name: "Base", positions: [at(27, "KC_D"), at(28, "KC_F")]},
        {index: 1, name: "Numbers", positions: [at(27, "KC_TRANSPARENT"), at(28, "KC_TRANSPARENT")]},
        {index: 2, name: "Symbols", positions: [at(27, "KC_TRANSPARENT"), at(28, "KC_NO")]},
    ];
    const model = {combos: [combo]};

    assert.equal(comboGroups(model, stack, 0).onKeys.length, 1, "both inputs are stored here");
    const above = comboGroups(model, stack, 1);
    assert.equal(above.onKeys.length, 0);
    assert.equal(above.throughKeys.length, 1, "both inputs fall through, so the combo still fires");
    const blocked = comboGroups(model, stack, 2);
    assert.equal(blocked.throughKeys.length, 0);
    assert.equal(blocked.elsewhere[0].keys.length, 1, "KC_NO takes one input away, so it cannot fire");
});

test("behaviours group by how this layer reaches them", () => {
    const model = {keyBehaviors: [
        {keycode: "KC_ESCAPE", steps: []},
        {keycode: "LEFT_THUMB", steps: []},
        {keycode: "KC_1", steps: []},
        {keycode: "KC_9", steps: []},
    ]};
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "KC_ESCAPE"), at(1, "KC_1"), at(2, "KC_9"), at(3, "KC_B")]},
        {index: 1, name: "Numbers", positions: [at(0, "LEFT_THUMB"), at(1, "KC_TRANSPARENT"), at(2, "KC_NO"), at(3, "KC_TRANSPARENT")]},
    ];

    const groups = behaviourGroups(model, stack, 1);
    assert.deepEqual(groups.here.map((row) => row.keycode), ["LEFT_THUMB"], "stored on this layer");
    assert.deepEqual(groups.through.map((entry) => entry.row.keycode), ["KC_1"],
        "a transparent key lets the layer underneath answer");
    assert.equal(groups.through[0].layer.name, "Base", "and the group names the layer that answers");
    assert.deepEqual(groups.elsewhere.map((row) => row.keycode), ["KC_ESCAPE", "KC_9"],
        "KC_ESCAPE is covered on this layer and KC_NO stops the fall-through to KC_9");

    const base = behaviourGroups(model, stack, 0);
    assert.deepEqual(base.here.map((row) => row.keycode), ["KC_ESCAPE", "KC_1", "KC_9"]);
    assert.deepEqual(base.through, [], "nothing lies under the base layer");
    assert.deepEqual(base.elsewhere.map((row) => row.keycode), ["LEFT_THUMB"]);
});

test("a macro fired from a behaviour branch is reached, though no key shows it", () => {
    const model = {
        viaMacros: [
            {kind: "via", keycode: "VIA_MACRO_0", payload: "{KC_A}", bytes: 3, empty: false},
            {kind: "via", keycode: "VIA_MACRO_1", payload: "{KC_B}", bytes: 3, empty: false},
            {kind: "via", keycode: "VIA_MACRO_2", payload: "{KC_C}", bytes: 3, empty: false},
            {kind: "via", keycode: "VIA_MACRO_3", payload: "", bytes: 0, empty: true},
        ],
        keyBehaviors: [
            {keycode: "KC_ESCAPE", steps: [{tapCount: 1, tap: {action: "VIA_MACRO_1"}}]},
            {keycode: "KC_B", steps: [{tapCount: 0, longHold: {action: "VIA_MACRO_2"}}]},
        ],
    };
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "KC_ESCAPE"), at(1, "VIA_MACRO_0"), at(2, "KC_B")]},
        {index: 1, name: "Numbers", positions: [at(0, "KC_TRANSPARENT"), at(1, "KC_TRANSPARENT"), at(2, "KC_NO")]},
    ];

    const base = macroReach(model, stack, 0);
    assert.deepEqual(base.onKeys.map((entry) => entry.name), ["VIA_MACRO_0"], "the only macro a key carries");
    assert.deepEqual(base.throughKeys, [], "the base layer has nothing under it to fall through to");
    assert.deepEqual(base.fromBranches.map((entry) => entry.name), ["VIA_MACRO_1", "VIA_MACRO_2"],
        "both branches are found, tap and long hold alike");
    assert.deepEqual(base.fromBranches[0].behaviours,
        [{keycode: "KC_ESCAPE", action: "VIA_MACRO_1", layer: null, whileHeld: false}],
        "named by the behaviour that fires it and by what its branch sends");
    assert.deepEqual(base.fromBranchesBelow, [], "nothing lies under the base layer to reach a behaviour through");
    assert.deepEqual(base.elsewhere, [], "an empty slot is not a macro this layer is missing");

    const above = macroReach(model, stack, 1);
    assert.deepEqual(above.onKeys, [], "no key on Numbers carries a macro itself");
    assert.deepEqual(above.throughKeys.map((entry) => entry.name), ["VIA_MACRO_0"],
        "a transparent key lets the macro key underneath answer");
    assert.equal(above.throughKeys[0].keys[0].layer.name, "Base", "and the group names the layer it came from");
    assert.deepEqual(above.fromBranches, [], "no behaviour mapped on Numbers sends a macro");
    assert.equal(above.fromBranchesBelow.find((entry) => entry.name === "VIA_MACRO_1").behaviours[0].layer.name,
        "Base", "and the entry names the layer holding that behaviour, since this one does not");
    assert.deepEqual(above.elsewhere, ["VIA_MACRO_2"],
        "KC_NO on the layer being viewed is the top answer, so KC_B on Base is not reached");
});

test("a transparent key can be answered by any layer below that is held with this one", () => {
    // The real stack: Base, Numbers, Symbols, with LT(NUM,SPC) on a thumb and
    // LT(SYM,J) on the other hand, so {Base, Numbers, Symbols} is a real hold.
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "KC_8"), at(1, "KC_RIGHT_ALT"), at(2, "VIA_MACRO_0")]},
        {index: 1, name: "Numbers", positions: [at(0, "KC_P7"), at(1, "KC_NO"), at(2, "KC_TRANSPARENT")]},
        {index: 2, name: "Symbols", positions: [at(0, "KC_TRANSPARENT"), at(1, "KC_TRANSPARENT"), at(2, "KC_TRANSPARENT")]},
    ];

    // Symbols alone: the default layer answers everything.
    assert.deepEqual(resolvedPositions(stack, 2, []).map((entry) => entry.position.keycode),
        ["KC_8", "KC_RIGHT_ALT", "VIA_MACRO_0"]);
    // Symbols with Numbers held: Numbers wins where it has a key, KC_NO included.
    assert.deepEqual(resolvedPositions(stack, 2, [1]).map((entry) => entry.position.keycode),
        ["KC_P7", "KC_NO", "VIA_MACRO_0"], "and its transparent position still falls to Base");

    // Both are real, so both are reachable, each named with what it needs.
    const reachable = reachablePositions(stack, 2);
    const atZero = reachable.filter((entry) => entry.position.layoutIndex === 0);
    assert.deepEqual(atZero.map((entry) => [entry.position.keycode, entry.layer.name, entry.whileHeld]),
        [["KC_P7", "Numbers", true], ["KC_8", "Base", false]],
        "Numbers answers only while held; Base always does");

    // An intermediate KC_NO is one real outcome — the key does nothing while
    // Numbers is held — but it takes nothing away, because dropping Numbers
    // leaves Base answering. It names nothing, so no group ever lists it.
    assert.deepEqual(reachable.filter((entry) => entry.position.layoutIndex === 1)
        .map((entry) => [entry.position.keycode, entry.whileHeld]),
        [["KC_NO", true], ["KC_RIGHT_ALT", false]]);
    const model = {keyBehaviors: [{keycode: "KC_RIGHT_ALT", steps: [{tapCount: 0, hold: {action: "ARROW_MODE"}}]}],
        pdModes: [{id: 4, name: "Arrow", kind: 1}]};
    assert.deepEqual(pointingReach(model, stack, 2).fromBranchesBelow.map((entry) => entry.name), ["4"],
        "so the behaviour on Base is reached, and the mode its branch sends with it");
});

test("a combo fires only if one activation carries every input at once", () => {
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const combo = {id: 1, badge: "C1", inputs: ["KC_D", "KC_F"], output: "KC_TAB"};
    // KC_D answers only with Numbers held; KC_F only without it. Each input is
    // reachable, the pair never is — which a per-key union would miss.
    const split = [
        {index: 0, name: "Base", positions: [at(27, "KC_X"), at(28, "KC_F")]},
        {index: 1, name: "Numbers", positions: [at(27, "KC_D"), at(28, "KC_NO")]},
        {index: 2, name: "Symbols", positions: [at(27, "KC_TRANSPARENT"), at(28, "KC_TRANSPARENT")]},
    ];
    const never = comboGroups({combos: [combo]}, split, 2);
    assert.deepEqual(never.onKeys, []);
    assert.deepEqual(never.throughKeys, [], "no single hold carries both inputs");
    assert.equal(never.elsewhere.length, 1);
    assert.equal(never.elsewhere[0].keys.length, 1, "and the row says how far any one hold gets");

    // Move KC_F onto Numbers and one hold carries both.
    const together = [
        split[0],
        {index: 1, name: "Numbers", positions: [at(27, "KC_D"), at(28, "KC_F")]},
        split[2],
    ];
    const fires = comboGroups({combos: [combo]}, together, 2);
    assert.deepEqual(fires.elsewhere, []);
    assert.equal(fires.throughKeys.length, 1);
    assert.deepEqual(fires.throughKeys[0].held.map((layer) => layer.name), ["Numbers"],
        "and it names the layer you have to hold as well");
});

test("a thing reached several ways is listed under each of them", () => {
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const model = {
        viaMacros: [
            {kind: "via", keycode: "VIA_MACRO_0", payload: "{KC_A}", bytes: 3, empty: false},
            {kind: "via", keycode: "VIA_MACRO_1", payload: "{KC_B}", bytes: 3, empty: false},
        ],
        keyBehaviors: [
            {keycode: "KC_ESCAPE", steps: [{tapCount: 1, tap: {action: "VIA_MACRO_0"}}]},
            {keycode: "KC_B", steps: [{tapCount: 1, tap: {action: "VIA_MACRO_1"}}]},
        ],
    };
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "KC_ESCAPE"), at(1, "VIA_MACRO_1"), at(2, "KC_X")]},
        {index: 1, name: "Numbers", positions: [at(0, "KC_ESCAPE"), at(1, "KC_TRANSPARENT"), at(2, "KC_B")]},
    ];

    // VIA_MACRO_1 sits on a key below and is also fired by KC_B, a behaviour
    // mapped on Numbers itself. Both are real ways to reach it, so both are
    // answered — each entry carrying only the route it is filed under.
    const above = macroReach(model, stack, 1);
    assert.deepEqual(above.onKeys, [], "no macro keycode is on a key of Numbers");
    assert.deepEqual(above.fromBranches.map((entry) => entry.name).sort(), ["VIA_MACRO_0", "VIA_MACRO_1"]);
    assert.deepEqual(above.throughKeys.map((entry) => entry.name), ["VIA_MACRO_1"],
        "the key below is a second way to the same macro, not a lost one");
    assert.deepEqual(above.throughKeys[0].behaviours, [],
        "and that entry answers for the key alone, so picking it rings only that key");
    assert.deepEqual(above.fromBranches.find((entry) => entry.name === "VIA_MACRO_1").keys, []);
    assert.deepEqual(above.elsewhere, []);
});

test("reach lists read down the stack, the default layer first", () => {
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const clear = (indexes) => indexes.map((i) => at(i, "KC_TRANSPARENT"));
    const model = {
        viaMacros: [0, 1, 2].map((n) => ({kind: "via", keycode: `VIA_MACRO_${n}`, payload: "{KC_A}", bytes: 3, empty: false})),
        keyBehaviors: [
            {keycode: "KC_A", steps: [{tapCount: 0, tap: {action: "KC_A"}}]},
            {keycode: "KC_B", steps: [{tapCount: 0, tap: {action: "KC_B"}}]},
            {keycode: "KC_C", steps: [{tapCount: 0, tap: {action: "KC_C"}}]},
        ],
    };
    // Each layer under the top one answers a different position, so the order
    // the rows come back in is the order of the layers that answer them.
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "VIA_MACRO_2"), at(1, "KC_NO"), at(2, "KC_NO"), at(3, "KC_C")]},
        {index: 1, name: "Numbers", positions: [at(0, "KC_NO"), at(1, "VIA_MACRO_1"), at(2, "KC_NO"), at(3, "KC_B")]},
        {index: 2, name: "Symbols", positions: [at(0, "KC_NO"), at(1, "KC_NO"), at(2, "VIA_MACRO_0"), at(3, "KC_A")]},
        {index: 3, name: "Top", positions: clear([0, 1, 2, 3])},
    ];

    assert.deepEqual(macroReach(model, stack, 3).throughKeys.map((entry) => entry.keys[0].layer.name),
        ["Base", "Numbers", "Symbols"], "macros answered lower in the stack come first");
    assert.deepEqual(behaviourGroups(model, stack, 3).through.map((entry) => entry.layer.name),
        ["Base", "Numbers", "Symbols"], "and so do behaviours");
});

test("a branch keeps which of a pointing mode's two keycodes it sends", () => {
    // A slot answers to both a hold and a toggle keycode. Collapsing them to the
    // slot is right for saying which mode is reached, but the row has to keep
    // the difference or holding and toggling read the same.
    const model = {
        pdModes: [{id: 0, name: "Dragscroll", kind: 2}],
        qmkKeycodeAliases: {},
        keyBehaviors: [{keycode: "DRAGSCROLL", steps: [{tapCount: 0, hold: {action: "DRAGSCROLL_LOCK"}}]}],
    };
    const stack = [{index: 0, name: "Base", positions: [{layoutIndex: 50, keycode: "DRAGSCROLL", display: "Dragscroll · hold"}]}];
    const reach = pointingReach(model, stack, 0);

    assert.deepEqual(reach.onKeys.map((entry) => entry.name), ["0"], "the key holds the mode");
    assert.deepEqual(reach.fromBranches.map((entry) => entry.name), ["0"], "and its own behaviour toggles it");
    assert.deepEqual(reach.fromBranches[0].behaviours,
        [{keycode: "DRAGSCROLL", action: "DRAGSCROLL_LOCK", layer: null, whileHeld: false}],
        "so the branch keeps the toggle keycode, not just the slot it lands on");
});

test("a picked key finds the behaviour it would collide with, whatever its spelling", () => {
    const model = {
        qmkKeycodeAliases: {KC_ENT: "KC_ENTER", KC_ENTER: "KC_ENTER", KC_SLSH: "KC_SLASH", KC_SLASH: "KC_SLASH"},
        keyBehaviors: [{keycode: "KC_ENTER"}, {keycode: "LT(3,KC_SLASH)"}, {keycode: "DRAGSCROLL"}],
    };
    assert.equal(canonicalKeycode(model, "KC_ENT"), "KC_ENTER");
    assert.equal(canonicalKeycode(model, "LT(3, KC_SLSH)"), "LT(3,KC_SLASH)");
    assert.equal(behaviourListeningTo(model, "KC_ENT").keycode, "KC_ENTER");
    assert.equal(behaviourListeningTo(model, "LT(3, KC_SLSH)").keycode, "LT(3,KC_SLASH)");
    assert.equal(behaviourListeningTo(model, "DRAGSCROLL").keycode, "DRAGSCROLL");
    assert.equal(behaviourListeningTo(model, "KC_A"), undefined);
});

test("an existing combo opens with its inputs on the board, and keeps the ones this layer cannot reach", () => {
    const layer = (index, keycodes) => ({index, positions: keycodes.map((keycode, layoutIndex) => ({layoutIndex, keycode, display: keycode}))});
    const stack = [
        layer(0, ["KC_A", "KC_B", "KC_C", "KC_D"]),
        layer(1, ["KC_TRANSPARENT", "KC_BTN1", "KC_BTN2", "KC_X"]),
    ];
    const onLayer = {badge: "C1", inputs: ["KC_BTN1", "KC_BTN2"], output: "KC_ESC"};
    assert.deepEqual(comboEditInputs({}, stack, 1, onLayer),
        {positions: [1, 2], codes: {1: "KC_BTN1", 2: "KC_BTN2"}, extras: []});
    const through = {badge: "C2", inputs: ["KC_A", "KC_BTN1"], output: "KC_TAB"};
    assert.deepEqual(comboEditInputs({}, stack, 1, through),
        {positions: [0, 1], codes: {0: "KC_A", 1: "KC_BTN1"}, extras: []}, "a key falling through keeps the name it resolves to");
    const partly = {badge: "C3", inputs: ["KC_BTN1", "KC_F13"], output: "KC_TAB"};
    assert.deepEqual(comboEditInputs({}, stack, 1, partly),
        {positions: [1], codes: {1: "KC_BTN1"}, extras: ["KC_F13"]});
});

test("an input placed on two keys rings both, and still counts as one input", () => {
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const combo = {id: 7, badge: "C7", inputs: ["G(KC_C)", "G(KC_V)"], output: "G(KC_A)"};
    const layer = {index: 0, name: "Navigation", positions: [
        at(3, "G(KC_C)"), at(4, "G(KC_V)"), at(19, "G(KC_C)"), at(21, "G(KC_V)"), at(20, "KC_UP")]};
    const groups = comboGroups({combos: [combo]}, [layer], 0);
    assert.equal(groups.onKeys.length, 1);
    assert.deepEqual(groups.onKeys[0].keys.map((key) => key.position.layoutIndex), [3, 4, 19, 21],
        "every key carrying an input is where the combo can be pressed");
    assert.equal(groups.onKeys[0].covered, 2);

    const half = {index: 0, name: "Half", positions: [at(3, "G(KC_C)"), at(19, "G(KC_C)")]};
    const partial = comboGroups({combos: [combo]}, [half], 0);
    assert.deepEqual(partial.onKeys, [], "two copies of one input do not complete a two-input combo");
    assert.equal(partial.elsewhere[0].covered, 1);

    const edit = comboEditInputs({}, [layer], 0, combo);
    assert.deepEqual(edit, {positions: [3, 4], codes: {3: "G(KC_C)", 4: "G(KC_V)"}, extras: []},
        "the builder still holds one key per input, so saving does not duplicate inputs");
});

test("a picked key matches its behaviour row however its modifiers are spelled", () => {
    const model = {qmkKeycodeAliases: {KC_C: "KC_C", KC_ENT: "KC_ENTER", KC_ENTER: "KC_ENTER"}, keyBehaviors: [{keycode: "LGUI(KC_C)"}, {keycode: "LCTL(LSFT(KC_ENTER))"}]};
    assert.equal(canonicalKeycode(model, "G(KC_C)"), "LGUI(KC_C)");
    assert.equal(behaviourListeningTo(model, "G(KC_C)")?.keycode, "LGUI(KC_C)", "the picker's Cmd wrapper");
    assert.equal(behaviourListeningTo(model, "LCMD(KC_C)")?.keycode, "LGUI(KC_C)");
    assert.equal(behaviourListeningTo(model, "S(C(KC_ENT))")?.keycode, "LCTL(LSFT(KC_ENTER))", "nesting order and aliases");
    assert.equal(behaviourListeningTo(model, "LCS(KC_ENT)")?.keycode, "LCTL(LSFT(KC_ENTER))", "a combined wrapper");
    assert.equal(behaviourListeningTo(model, "C(KC_C)"), undefined, "a different modifier is a different key");
});

test("combos follow the keyboard's combo layer matching", () => {
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const combo = {id: 1, badge: "C1", inputs: ["KC_Q", "KC_W"], output: "KC_ESC"};
    const stack = [
        {index: 0, name: "Base", positions: [at(13, "KC_Q"), at(14, "KC_W")]},
        {index: 1, name: "Numbers", positions: [at(13, "KC_1"), at(14, "KC_2")]},
    ];
    const own = comboGroups({combos: [combo]}, stack, 1);
    assert.equal(own.elsewhere.length, 1, "matched on its own keys, Numbers cannot fire a Q+W combo");

    // "Combos on Numbers → Base": the keyboard matches Base's keycodes while Numbers is on top.
    const settings = {configDefaults: [{id: "comboReferences", fields: [{macro: "comboReference1", value: "Layer 0"}]}]};
    const referenced = comboGroups({...settings, combos: [combo]}, stack, 1);
    assert.equal(referenced.onKeys.length, 1);
    assert.deepEqual(referenced.onKeys[0].keys.map((key) => key.position.layoutIndex), [13, 14]);
    assert.ok(referenced.onKeys[0].keys.every((key) => key.reference && key.layer.name === "Base"));
    assert.equal(comboReferenceLayer({comboReadback: {layerReferences: [0, 0]}}, 1), 0, "the readback answers without settings");
    assert.equal(comboReferenceLayer({}, 1), 1, "and each layer matches itself by default");

    const edit = comboEditInputs({...settings}, stack, 1, combo);
    assert.deepEqual(edit.codes, {13: "KC_Q", 14: "KC_W"}, "the builder opens with the keys the keyboard matches");
});
