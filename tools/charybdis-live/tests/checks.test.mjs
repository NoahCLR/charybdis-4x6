import test from "node:test";
import assert from "node:assert/strict";
import {checkGroups, checkTags, confirmText, trapsToConfirm} from "../webview/view/checks.mjs";

const check = (level, status, title = `${level} ${status}`) => ({level, status, title, key: title});

test("checks are grouped by where they come from, most serious first, and empty groups are left out", () => {
    const groups = checkGroups([check("notice", "new"), check("trap", "new"), check("warning", "fixed"), check("warning", "new")]);
    assert.deepEqual(groups.map((group) => group.id), ["new", "fixed"]);
    assert.deepEqual(groups[0].items.map((item) => item.level), ["trap", "warning", "notice"]);
    assert.equal(groups[0].open, true);
    assert.equal(groups[1].open, false);
    assert.deepEqual(checkGroups(), []);
});

test("what is already on the keyboard stays folded unless it holds a trap", () => {
    assert.equal(checkGroups([check("warning", "existing")])[0].open, false);
    assert.equal(checkGroups([check("warning", "existing"), check("trap", "existing")])[0].open, true);
});

test("every trap the draft keeps asks for confirmation, whoever made it; a fixed one does not", () => {
    const checks = [check("trap", "new", "A"), check("trap", "existing", "B"), check("trap", "fixed", "C"), check("warning", "new", "D")];
    assert.deepEqual(trapsToConfirm(checks).map((item) => item.title), ["A", "B"]);
    assert.deepEqual(trapsToConfirm(), []);
});

test("the confirmation names one trap, or counts several", () => {
    assert.equal(confirmText([]), "");
    assert.equal(confirmText([check("trap", "new", "Numbers can lock with no way back to Base")]),
        "Numbers can lock with no way back to Base. Once locked, only unplugging the keyboard clears it.");
    assert.match(confirmText([check("trap", "new"), check("trap", "existing")]), /^2 sets of layers can lock/);
});

test("a check's tags say its level, and where it comes from unless it is new", () => {
    assert.deepEqual(checkTags(check("trap", "new")), ["Trap"]);
    assert.deepEqual(checkTags(check("notice", "existing")), ["Notice", "on the keyboard"]);
    assert.deepEqual(checkTags(check("warning", "fixed")), ["Warning", "fixed"]);
});
