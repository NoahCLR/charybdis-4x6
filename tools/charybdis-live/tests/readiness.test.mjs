import test from "node:test";
import assert from "node:assert/strict";
import {configureReady, postApplyReadText, readScreen, screenAvailable} from "../webview/view/readiness.mjs";

test("loading names the active read and never claims the board is ready", () => {
    assert.deepEqual(readScreen(null), {state: "loading", title: "Reading your keyboard", detail: "Looking for a keyboard"});
    assert.match(readScreen({load: {state: "loading", phase: "reading layout", progress: {done: 5, total: 8}}}).detail,
        /Reading keys and layers · 5 of 8/);
    assert.match(readScreen({load: {state: "loading", phase: "reading complete profile"}}).detail, /Reading the complete profile/);
});

test("only a ready model releases Configure screens", () => {
    assert.equal(configureReady(null), false);
    assert.equal(configureReady({load: {state: "loading"}}), false);
    assert.equal(configureReady({load: {state: "unavailable"}}), false);
    assert.equal(configureReady({load: {state: "ready"}}), true);
    assert.equal(readScreen({load: {state: "ready"}}), null);
    assert.match(readScreen({load: {state: "unavailable"}, device: {connected: true}}).detail, /complete profile/);
    assert.match(readScreen({load: {state: "unavailable"}, device: {connected: true}, portable: {legacy: true}}).detail, /five layers/);
});

test("rail screens follow the read and the capability each screen needs", () => {
    const loading = {load: {state: "loading"}, device: {connected: true}, portable: {available: true}};
    for (const screen of ["keys", "lighting", "profile", "device"]) assert.equal(screenAvailable(loading, screen), false, screen);

    const noKeyboard = {load: {state: "unavailable"}, device: {connected: false}, portable: {available: false}};
    assert.equal(screenAvailable(noKeyboard, "keys"), false);
    assert.equal(screenAvailable(noKeyboard, "profile"), false);
    assert.equal(screenAvailable(noKeyboard, "device"), false, "a scan without a keyboard has no device status to show");
    assert.match(readScreen(noKeyboard, "profile").title, /Backups unavailable/);
    assert.equal(screenAvailable({...noKeyboard, portable: {available: false, review: {fileName: "old.json"}}}, "profile"), false,
        "a retained review cannot make a disconnected keyboard available");

    const legacy = {load: {state: "unavailable"}, device: {connected: true}, portable: {available: true, legacy: true}};
    assert.equal(screenAvailable(legacy, "keys"), false);
    assert.equal(screenAvailable(legacy, "profile"), true, "the read-only bridge can export a backup");
    assert.equal(screenAvailable(legacy, "device"), true);

    const failedCapture = {...legacy, portable: {available: true, legacy: false}};
    assert.equal(screenAvailable(failedCapture, "profile"), true, "a connected device can retry profile export after capture failed");
    const ready = {...failedCapture, load: {state: "ready"}};
    for (const screen of ["keys", "lighting", "profile", "device"]) assert.equal(screenAvailable(ready, screen), true, screen);
});

test("post-Apply readback names the current data instead of a generic operation", () => {
    assert.equal(postApplyReadText(null), "Confirming the completed save");
    assert.equal(postApplyReadText({step: "layout", progress: {done: 12, total: 56}}), "Reading keys and layers · 12 of 56");
    assert.equal(postApplyReadText({step: "profile"}), "Reading saved lighting, behaviours and settings");
    assert.equal(postApplyReadText({step: "combos"}), "Checking active combos");
    assert.equal(postApplyReadText({step: "baseRgb"}), "Checking base lighting");
});
