"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {ProfileDeviceService} = require("../../core/session/profile-device-service");
const {buildDeviceModel} = require("../../core/session/device-model");
const {baseRgbForView} = require("../../core/session/device-profile-view");

test("base RGB survives independently of the profile, but never survives failure or disconnect", async () => {
    const service = new ProfileDeviceService();
    let fail = false;
    service.connection = {connected: true, async request(request) {
        const response = Buffer.from(request);
        if (fail) response[0] = 0xff;
        else response.set({1: [255], 2: [1], 3: [32], 4: [0, 255]}[request[2]], 3);
        return response;
    }};
    service.error = {message: "Earlier profile failure"};
    let state = await service.readBaseRgb();
    assert.equal(state.baseRgb.state, "read");
    assert.equal(state.error.message, "Earlier profile failure");
    assert.deepEqual(buildDeviceModel(state).rgb.baseEffect.previewColor, {h: "0", s: "255", v: "255"});
    state.baseRgb.hue = 99;
    assert.equal(service.snapshot().baseRgb.hue, 0, "snapshots cannot mutate the cached read");
    fail = true;
    state = await service.readBaseRgb();
    assert.equal(state.baseRgb.state, "unavailable");
    assert.equal(buildDeviceModel(state).rgb.baseEffect.previewColor, undefined);
    assert.equal(state.error.message, "Earlier profile failure");
    service.handleDisconnect(new Error("Unplugged"));
    assert.equal(service.snapshot().baseRgb, null);
});

test("off, brightness zero, and unrecognised effects have distinct presentation", () => {
    const state = {state: "read", effectId: 1, brightness: 0, hue: 0, saturation: 255, speed: 0};
    assert.equal(baseRgbForView(state).enabled, true);
    assert.equal(baseRgbForView(state).previewColor.v, "0");
    const off = baseRgbForView({...state, effectId: 0});
    assert.equal(off.enabled, false);
    assert.equal(off.effectName, "Off");
    const unknown = baseRgbForView({...state, effectId: 42});
    assert.equal(unknown.effectName, "Effect 42");
    assert.equal(unknown.previewColor, undefined, "an effect's configured hue is not its rendered frame");
});
