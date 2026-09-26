import test from "node:test";
import assert from "node:assert/strict";
import {checkGroups, checkSourceGroup, checkTags, checksToConfirm, confirmText} from "../webview/view/checks.mjs";

const check = (level, status, title = `${level} ${status}`) => ({level, status, title, key: title});

test("checks are grouped by where they come from, most serious first, and empty groups are left out", () => {
    const groups = checkGroups([check("notice", "new"), check("trap", "new"), check("blocker", "new"), check("warning", "fixed"), check("warning", "new")]);
    assert.deepEqual(groups.map((group) => group.id), ["new", "fixed"]);
    assert.deepEqual(groups[0].items.map((item) => item.level), ["blocker", "trap", "warning", "notice"]);
    assert.equal(groups[0].open, true);
    assert.equal(groups[1].open, false);
    assert.deepEqual(checkGroups(), []);
});

test("what is already on the keyboard stays folded unless it holds a trap", () => {
    assert.equal(checkGroups([check("warning", "existing")])[0].open, false);
    assert.equal(checkGroups([check("warning", "existing"), check("trap", "existing")])[0].open, true);
    assert.equal(checkGroups([check("blocker", "existing")])[0].open, true);
});

test("active traps and warnings ask for confirmation, including ones already on the keyboard", () => {
    const checks = [check("trap", "new", "A"), check("trap", "existing", "B"), check("trap", "fixed", "C"),
        check("warning", "new", "D"), check("warning", "existing", "E"), check("warning", "fixed", "F"), check("notice", "new", "G")];
    assert.deepEqual(checksToConfirm(checks).map((item) => item.title), ["A", "B", "D", "E"]);
    assert.deepEqual(checksToConfirm(), []);
    assert.deepEqual(checksToConfirm([check("blocker", "new")]), [], "a blocker cannot be confirmed away");
});

test("a new warning names one draft group, but does not guess among several", () => {
    const warning = {...check("warning", "new"), kind: "unreachable", place: {kind: "key", layer: 2}};
    const order = {group: 3, unit: "layerOrder", title: "Layer priority"};
    assert.equal(checkSourceGroup(warning, [order]), 3);
    assert.equal(checkSourceGroup(warning, [order, {group: 3, unit: "layout:0:1"}]), 3, "one edit group may contain several rows");
    assert.equal(checkSourceGroup(warning, [order, {group: 4, unit: "layout:0:1"}]), null);
    assert.equal(checkSourceGroup({...warning, status: "existing"}, [order]), null);
    assert.equal(checkSourceGroup({...warning, status: "fixed"}, [order]), null);
    assert.equal(checkSourceGroup({...warning, level: "notice"}, [order]), null);
    assert.equal(checkSourceGroup(warning, [{...order, group: null}]), null);
});

test("an unowned layer key points to its changed key even with other edit groups", () => {
    const warning = {...check("warning", "new"), kind: "unowned", place: {kind: "key", layer: 3, layoutIndex: 12}};
    const changes = [{group: 1, place: {kind: "key", layer: 3, layoutIndex: 12}}, {group: 2, unit: "layerOrder"}];
    assert.equal(checkSourceGroup(warning, changes), 1);
    assert.equal(checkSourceGroup({...warning, place: {kind: "key", layer: 3, layoutIndex: 13}}, changes), null);
});

test("the confirmation names one finding or counts traps and warnings together", () => {
    assert.equal(confirmText([]), "");
    assert.equal(confirmText([check("trap", "new", "Numbers can lock with no way back to Base")]),
        "Numbers can lock with no way back to Base. A trapped layer can leave no way back to Base until you unplug the keyboard. Apply this profile anyway?");
    assert.equal(confirmText([check("warning", "new", "Nothing reaches Number")]),
        "Nothing reaches Number. Apply this profile with this warning?");
    assert.match(confirmText([check("trap", "new"), check("warning", "existing")]), /^1 trap and 1 warning\./);
    assert.match(confirmText([check("trap", "new"), {...check("trap", "new"), kind: "trapOverflow", count: 6}]), /^7 traps\./);
});

test("a check's tags say its level, and where it comes from unless it is new", () => {
    assert.deepEqual(checkTags(check("blocker", "new")), ["Blocker"]);
    assert.deepEqual(checkTags(check("trap", "new")), ["Trap"]);
    assert.deepEqual(checkTags(check("notice", "existing")), ["Notice", "on the keyboard"]);
    assert.deepEqual(checkTags(check("warning", "fixed")), ["Warning", "fixed"]);
});
