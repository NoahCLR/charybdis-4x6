import assert from "node:assert/strict";
import test from "node:test";
import {GEO, LED_INDEX, TRACKBALL_LED, inLocality, isRightHalf, keyVisual, trackballInLocality} from "../webview/view/geometry.mjs";

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
