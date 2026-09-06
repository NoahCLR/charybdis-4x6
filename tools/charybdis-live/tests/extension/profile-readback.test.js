"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const {createRequire} = require("node:module");
const test = require("node:test");
const {ProfileDeviceService, ProfileRequestIdSequence} = require("../../core/session/profile-device-service");
const {PROFILE_WIRE_V1, PROFILE_WIRE_STATUS} = require("../../core/protocol/profile-wire-v1");
const {PROFILE_PAYLOAD_V1} = require("../../core/protocol/profile-payload-v1");
const {crc32, fnv1a32} = require("../../core/schema/profile-blob-v1");
const {bytes, capabilities} = require("../fixtures/device-profile");

const {fixturePages, responseFor} = require("../fixtures/device-combos");

function harness({source = "compiled", fail = false, failBase = false, failCombos = false, payload = bytes, allowSaves = false, corruptSave = false} = {}) {
    const messages = [];
    const requests = [];
    const candidates = [];
    let receive, open, dispose;
    const panel = {webview: {postMessage(message) {messages.push(message);}, onDidReceiveMessage(handler) {receive = handler;}}, onDidDispose(handler) {dispose = handler;}};
    const connection = {connected: true, async request(request) {
        requests.push(Buffer.from(request));
        assert.equal(request[0], PROFILE_WIRE_V1.COMMAND_GET, "readback must never mutate the device");
        if (request[1] === 3) {
            const response = Buffer.from(request);
            if (failBase) response[0] = 0xff;
            else response.set({1: [255], 2: [1], 3: [32], 4: [0, 255]}[request[2]], 3);
            return response;
        }
        if (request[2] === 6) return failCombos ? Buffer.alloc(32, 0xff) : responseFor(request, fixturePages());
        assert.ok([PROFILE_PAYLOAD_V1.VALUE, PROFILE_PAYLOAD_V1.COMPILED_VALUE].includes(request[2]));
        const response = Buffer.alloc(32);
        request.copy(response, 0, 0, 5);
        if (fail || (source === "compiled" && request[2] === PROFILE_PAYLOAD_V1.VALUE)) {
            response[5] = PROFILE_WIRE_STATUS.UNAVAILABLE;
            return response;
        }
        let page;
        if (request[4] === 0) {
            page = Buffer.alloc(25);
            page[0] = 1; page[1] = 25;
            page.writeUInt16LE(payload.length, 2);
            page.writeUInt32LE(source === "compiled" ? 0 : 9, 4);
            page.writeUInt32LE(fnv1a32(payload), 8);
            page.writeUInt32LE(crc32(payload), 12);
            page[16] = 1; page[18] = 3;
        } else {
            const offset = (request[4] - 1) * 25;
            page = payload.subarray(offset, offset + 25);
        }
        response[6] = page.length;
        page.copy(response, 7);
        return response;
    }};
    // Supply connection/identity at the adapter seam. The real chunk reader,
    // domain decoders, service snapshot, extension and model run below it.
    class Service extends ProfileDeviceService {
        constructor(options) {
            super(options);
            this.connection = connection;
            this.devices = [{id: "test-device", product: "Test keyboard"}];
            this.connectionPublicId = "test-device";
            this.capabilities = capabilities;
            this.requestIds = new ProfileRequestIdSequence(1);
            this.status = {activeGeneration: source === "compiled" ? 0 : 9, committedGeneration: source === "compiled" ? 0 : 9, peerGeneration: source === "compiled" ? 0 : 9};
        }
        async enumerate() {return this.snapshot();}
        async refresh() {return this.snapshot();}
        async readLayout() {this.layout = {state: "read", layers: [{layer: 0, keys: []}]}; return this.snapshot();}
        async applyLiveProfile(candidate, options) {
            if (!allowSaves) return super.applyLiveProfile(candidate, options);
            // Stand in only for the already-tested transaction coordinator.
            // The real edit, save orchestration, HID readback and UI relay run.
            assert.equal(options.expectedBase.digest, fnv1a32(payload));
            assert.equal(options.expectedBase.source, source);
            candidates.push(Buffer.from(candidate));
            payload = Buffer.from(candidate);
            if (corruptSave) payload[22] ^= 1;
            source = "committed";
            this.status = {...this.status, committedGeneration: 9, activeGeneration: 9, peerGeneration: 9};
            this.liveApply = {state: "complete", error: null};
            return this.snapshot();
        }
        async close() {connection.connected = false;}
    }
    const vscode = {StatusBarAlignment: {Left: 1}, ViewColumn: {One: 1}, ProgressLocation: {Notification: 1},
        commands: {registerCommand(id, callback) {open = callback; return {}; }},
        window: {createStatusBarItem: () => ({show() {}}), createWebviewPanel: () => panel,
            withProgress: (options, task) => task(), showErrorMessage() {}}};
    const file = path.resolve(__dirname, "../../extension.js");
    const localRequire = createRequire(file);
    const sandbox = {module: {exports: {}}, require(name) {
        if (name === "vscode") return vscode;
        if (name === "./core/session/profile-device-service") return {ProfileDeviceService: Service};
        return localRequire(name);
    }};
    vm.runInNewContext(fs.readFileSync(file, "utf8"), sandbox, {filename: file});
    sandbox.module.exports.activate({subscriptions: []});
    open();
    return {messages, requests, candidates, panel, read: () => receive({type: "ready"}), send: (message) => receive(message), close: () => dispose()};
}

test("behaviour messages save through the service and publish only verified readback", async () => {
    const app = harness({allowSaves: true});
    await app.read();
    const original = app.messages.at(-1).model.keyBehaviors[0];
    const count = app.requests.length;
    await app.send({type: "saveBehavior", behavior: {...original, tapHoldTerm: "175"}});
    assert.equal(app.candidates.length, 1);
    assert.match(app.messages.at(-1).notice, /Saved to both halves and verified/);
    assert.equal(app.messages.at(-1).model.keyBehaviors.find(row => row.keycode === original.keycode).tapHoldTerm, "175");
    assert.ok(app.requests.length > count + 40, "saving must reread the payload over HID");
    await app.send({type: "addBehavior", behavior: {keycode: "KC_A", tap: {helper: "TAP_SENDS", action: "G(KC_N)"}}});
    assert.equal(app.messages.at(-1).model.keyBehaviors.length, 38);
    await app.send({type: "deleteBehavior", keycode: "KC_A"});
    assert.equal(app.candidates.length, 3);
    assert.equal(app.messages.at(-1).model.keyBehaviors.length, 37);
    assert.match(app.messages.at(-1).notice, /Saved to both halves and verified/);
    app.close();
});

test("invalid behaviour edits send no candidate and failed readback never reports save success", async () => {
    const app = harness({allowSaves: true, corruptSave: true});
    await app.read();
    const behavior = app.messages.at(-1).model.keyBehaviors[0];
    const count = app.requests.length;
    await app.send({type: "saveBehavior", behavior: {...behavior, tapHoldTerm: "not a duration"}});
    assert.equal(app.candidates.length, 0);
    assert.equal(app.requests.length, count);
    assert.match(app.messages.at(-1).notice, /Failed.*Tap\/hold time/);
    await app.send({type: "saveBehavior", behavior: {...behavior, tapHoldTerm: "200"}});
    assert.equal(app.candidates.length, 1);
    assert.match(app.messages.at(-1).notice, /Failed.*did not match the readback/);
    app.close();
});

for (const source of ["compiled", "committed"]) test(`${source} HID read reaches the actual extension publication with UI fields`, async () => {
    const app = harness({source});
    await app.read();
    const last = app.messages.at(-1);
    assert.equal(last.model.keyBehaviors.length, 37);
    assert.equal(last.model.combos.length, 2);
    assert.equal(last.model.combos[1].outputDisplay, "Cmd+A");
    assert.equal(last.model.rgb.layerColors.length, 5);
    assert.equal(last.model.rgb.layerColors[1].layer, "Layer 1");
    assert.equal(last.model.rgb.layerColors[1].color.h, "85");
    assert.equal(last.model.rgb.pdModeColors[0].pointingMode, "PD_MODE_DRAGSCROLL");
    assert.equal(last.model.rgb.ledGroups[0].usageCount, 3);
    assert.equal(last.model.rgb.baseEffect.effectName, "Solid colour");
    assert.deepEqual(last.model.rgb.baseEffect.previewColor, {h: "0", s: "255", v: "255"});
    assert.match(last.model.keyBehaviors[0].steps[0].tap.helper, /TAP_SENDS/);
    assert.match(last.notice, source === "compiled" ? /compiled defaults/ : /generation 9/);
    assert.doesNotMatch(last.notice, /generation 0/);
    assert.doesNotMatch(last.model.diagnostics.join(" "), /need the committed profile read/);
    assert.ok(app.requests.length > 40, "must exercise chunked readback, not just inject a decoded model");
    for (const interim of app.messages.filter(message => !message.model.rgb.layerColors)) assert.equal(interim.model.keyBehaviors.length, 0);
    app.close();
});

test("failed read is visible, never described as successful compiled-default readback", async () => {
    const app = harness({fail: true});
    await app.read();
    const last = app.messages.at(-1);
    assert.match(last.notice, /could not be read/);
    assert.doesNotMatch(last.notice, /Read the layout and/);
    assert.equal(last.model.keyBehaviors.length, 0);
    app.close();
});

test("a domain decode failure preserves the other domain and remains visible", async () => {
    const payload = Buffer.from(bytes);
    payload[12] = 255; // Unknown RGB domain version inside the verified blob.
    const app = harness({payload});
    await app.read();
    const last = app.messages.at(-1);
    assert.equal(last.model.keyBehaviors.length, 37);
    assert.equal(last.model.combos.length, 2);
    assert.equal(last.model.combos[1].outputDisplay, "Cmd+A");
    assert.deepEqual(Object.keys(last.model.rgb), ["baseEffect"], "failed profile RGB cannot erase the independent base read");
    assert.match(last.notice, /could not be decoded/);
    assert.match(last.model.diagnostics.join(" "), /0x10 did not decode/);
    app.close();
});

test("an unsupported base effect read leaves the verified profile visible", async () => {
    const app = harness({failBase: true});
    await app.read();
    const model = app.messages.at(-1).model;
    assert.equal(model.keyBehaviors.length, 37);
    assert.equal(model.rgb.layerColors.length, 5);
    assert.equal(model.rgb.baseEffect.state, "unavailable");
    assert.equal(model.rgb.baseEffect.previewColor, undefined);
    app.close();
});

test("invalid RGB edits and unsupported combo edits send no HID mutations", async () => {
    const app = harness();
    await app.read();
    const count = app.requests.length;
    await app.send({type: "updateLayerColor", layer: "Layer 1", color: {h: 2, s: 3, v: 4}});
    assert.match(app.messages.at(-1).notice, /Hue|must be/);
    assert.equal(app.requests.length, count);
    for (const type of ["addCombo", "saveCombo"]) {
        await app.send({type, output: "KC_TAB", inputs: "KC_D, KC_F"});
        assert.match(app.messages.at(-1).notice, /firmware can read combos but cannot save/);
        assert.equal(app.requests.length, count);
    }
    app.close();
});


test("an unsupported combo read leaves the other device domains visible", async () => {
    const app = harness({failCombos: true});
    await app.read();
    const model = app.messages.at(-1).model;
    assert.equal(model.keyBehaviors.length, 37);
    assert.equal(model.rgb.layerColors.length, 5);
    assert.equal(model.rgb.baseEffect.effectName, "Solid colour");
    assert.equal(model.combos.length, 0);
    assert.equal(model.comboReadback.error.code, "COMBO_UNSUPPORTED");
    assert.match(model.diagnostics.join(" "), /updated firmware pair/);
    app.close();
});
