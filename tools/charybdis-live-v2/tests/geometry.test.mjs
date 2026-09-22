import assert from "node:assert/strict";
import test from "node:test";
import {GEO, LED_INDEX, TRACKBALL_LED, fitText, inLocality, isRightHalf, keyFaceRows, keyVisual, trackballInLocality} from "../webview/view/geometry.mjs";

test("the board splits into halves the way the LED numbering does", () => {
    assert.equal(isRightHalf(0), false, "left home column");
    assert.equal(isRightHalf(6), true, "first right column");
    assert.equal(isRightHalf(50), false, "left thumb cluster");
    assert.equal(isRightHalf(51), true, "right thumb cluster");
    assert.equal(isRightHalf(55), true);
    assert.equal(LED_INDEX[0], 0);
    assert.equal(LED_INDEX[51], 53, "right thumb keys carry right-half LEDs");
    assert.equal(new Set(Object.values(LED_INDEX)).size, 56, "every position has its own LED");
});

test("locality answers for the regions the firmware defines", () => {
    assert.equal(inLocality(0, "RGB_BOTH_HALVES"), true);
    assert.equal(inLocality(0, "RGB_LEFT_HALF"), true);
    assert.equal(inLocality(6, "RGB_LEFT_HALF"), false);
    assert.equal(inLocality(6, "RGB_RIGHT_HALF"), true);
    assert.equal(inLocality(6, "RGB_KEY_HALF", 7), true, "same half as the trigger");
    assert.equal(inLocality(0, "RGB_KEY_HALF", 7), false);
    assert.equal(inLocality(7, "RGB_KEYS_ONLY", 7), true);
    assert.equal(inLocality(6, "RGB_KEYS_ONLY", 7), false);
    assert.equal(inLocality(6, "RGB_KEYS_ONLY", undefined), false, "with no trigger, nothing is the trigger key");
});

test("every position lands inside the drawn board", () => {
    const [vx, vy, vw, vh] = GEO.viewBox.split(" ").map(Number);
    for (let index = 0; index < 56; index += 1) {
        const {x, y} = keyVisual(index);
        assert.ok(x >= vx && x + GEO.keyW <= vx + vw, `key ${index} is inside horizontally`);
        assert.ok(y >= vy && y + GEO.keyH <= vy + vh, `key ${index} is inside vertically`);
    }
});

test("the trackball LED sits on the right half and is nobody's trigger key", () => {
    assert.equal(TRACKBALL_LED, 56, "the index the firmware solders the trackball LED at");
    assert.equal(trackballInLocality("RGB_BOTH_HALVES"), true);
    assert.equal(trackballInLocality("RGB_RIGHT_HALF"), true);
    assert.equal(trackballInLocality("RGB_LEFT_HALF"), false);
    assert.equal(trackballInLocality("RGB_KEY_HALF", 7), true, "a right-half trigger reaches it");
    assert.equal(trackballInLocality("RGB_KEY_HALF", 0), false, "a left-half trigger does not");
    assert.equal(trackballInLocality("RGB_KEYS_ONLY", 7), false, "no key maps to the trackball LED");
});

test("the key face drops its legend by how many rows sit above it", () => {
    const plain = keyFaceRows(0);
    assert.equal(plain.mainY, GEO.keyH / 2, "with nothing above, the legend is simply centred");
    assert.equal(plain.tierY, undefined);

    assert.equal(keyFaceRows(0, {tiers: true}).mainY, 36, "one row above pushes it down");
    assert.equal(keyFaceRows(0, {tiers: true, combos: true}).mainY, 40.4, "two rows push it further");
    assert.deepEqual(
        [keyFaceRows(0, {tiers: true, combos: true}).tierY, keyFaceRows(0, {tiers: true, combos: true}).comboY],
        [6.3, 14.2], "and the two rows stack without overlapping");
    assert.equal(keyFaceRows(0, {combos: true}).comboY, 7.2, "a combo row alone sits where the dots would");

    // A hold legend takes the bottom of the cap, with the rule between them.
    const dual = keyFaceRows(0, {tiers: true, sub: true});
    assert.ok(dual.mainY < dual.separatorY && dual.separatorY < dual.subY, "tap, rule, hold, in that order");
    assert.ok(dual.subY < GEO.keyH, "and the hold legend stays on the cap");
});

test("a legend is sized to its row rather than bucketed by length", () => {
    const row = {max: 12, min: 7, width: 48};
    assert.deepEqual(fitText("Esc", row), {size: 12, squeeze: 0}, "a short legend takes the full size");
    assert.deepEqual(fitText("Tab", row).size, 12, "and so does another of the same width");

    const long = fitText("Left Control", row);
    assert.ok(long.size < 12 && long.size >= 7, "a long one shrinks to fit instead of changing face");
    assert.equal(long.squeeze, 48, "and is compressed into the row it was given");

    // Wider glyphs shrink further than narrow ones of the same count, which a
    // rule counting characters cannot tell apart.
    assert.ok(fitText("MMMMMMMM", row).size < fitText("iiiiiiii", row).size);
    assert.equal(fitText("", row).size, 12, "an empty legend has nothing to fit");
    assert.equal(fitText("Left Control", {max: 12, min: 11, width: 48}).size, 11, "the floor holds");
});
