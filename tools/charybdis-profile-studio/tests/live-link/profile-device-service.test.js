"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {FakeDeviceAdapter} = require("../../live-link/fake-device-adapter");
const {
    ProfileRequestIdSequence,
    ProfileDeviceService,
    evaluateProfileCompatibility,
} = require("../../live-link/profile-device-service");
const {PROFILE_WIRE_V1, VIA_READS} = require("../../live-link/profile-wire-v1");

function response(request, payload) {
    const report = Buffer.from(request);
    report.fill(0, 5);
    report[5] = 0;
    report[6] = payload.length;
    Buffer.from(payload).copy(report, 7);
    return report;
}

function readPages(options = {}) {
    const capabilityIdentity = Buffer.alloc(25);
    capabilityIdentity.set([1, 2, 1, 0, options.schemaMajor || 1, 0, 32, 20, 2, 0x3f], 0);
    const capacity = Buffer.from([
        5, options.maxLayers || 8, 64, 5, 128, 32, 4, 16, 32, 58, 8, 16, 64,
        0xe0, 0x0f, 0xe0, 0x0f, 0x00, 0x10, 0x7f, 0x1d, 3, 0, 0, 0,
    ]);
    const statusIdentity = Buffer.alloc(25);
    statusIdentity.set([1, 2, 0x81, 0], 0);
    return {
        [`${PROFILE_WIRE_V1.VALUE_CAPABILITIES}:0`]: capabilityIdentity,
        [`${PROFILE_WIRE_V1.VALUE_CAPABILITIES}:1`]: capacity,
        [`${PROFILE_WIRE_V1.VALUE_STATUS}:0`]: statusIdentity,
        [`${PROFILE_WIRE_V1.VALUE_STATUS}:1`]: Buffer.alloc(25),
    };
}

function serviceHarness(options = {}) {
    const pages = readPages(options);
    const adapter = new FakeDeviceAdapter({
        devices: [{
            id: "/native/raw/hid/path",
            path: "/native/raw/hid/path",
            product: "Charybdis 4x6",
            serialNumber: "SERIAL-1",
        }],
        onWrite({connection, report}) {
            if (report[0] === VIA_READS.COMMAND_GET_PROTOCOL_VERSION) {
                const viaResponse = Buffer.from(report);
                viaResponse[1] = 0;
                viaResponse[2] = VIA_READS.EXPECTED_PROTOCOL_VERSION;
                queueMicrotask(() => connection.emitReport(viaResponse));
                return;
            }
            if (report[0] === VIA_READS.COMMAND_GET_KEYBOARD_VALUE && report[1] === VIA_READS.VALUE_FIRMWARE_VERSION) {
                queueMicrotask(() => connection.emitReport(Buffer.from(report)));
                return;
            }
            assert.equal(report[0], PROFILE_WIRE_V1.COMMAND_GET, "Stage 01 service must only send read requests");
            const payload = pages[`${report[2]}:${report[4]}`];
            assert.ok(payload, `unexpected Profile Wire read ${report[2]} page ${report[4]}`);
            queueMicrotask(() => connection.emitReport(response(report, payload)));
        },
    });
    const changes = [];
    const service = new ProfileDeviceService({
        adapter,
        defaultTimeoutMs: 100,
        onChange(snapshot) {
            changes.push(snapshot);
        },
        profileSummary: {
            layerCount: 5,
            behaviorRows: 5,
            maxTapStepsPerBehavior: 3,
            populatedBehaviorSteps: 8,
            comboCount: 4,
            maxKeysPerCombo: 2,
            reusableRgbGroups: 5,
            rgbStageGroupRows: 12,
            highestLedIndex: 57,
        },
    });
    return {adapter, changes, service};
}

test("service exposes opaque descriptors and performs only capability/status reads", async () => {
    const {adapter, service} = serviceHarness();
    const scanned = await service.enumerate();
    assert.equal(scanned.devices.length, 1);
    assert.equal(scanned.devices[0].id, "charybdis-1");
    assert.equal(JSON.stringify(scanned).includes("/native/raw/hid/path"), false);

    const connected = await service.connect(scanned.devices[0].id);
    assert.equal(connected.connected, true);
    assert.equal(connected.capabilities.schema.major, 1);
    assert.equal(connected.status.activeKind, 0);
    assert.equal(connected.compatibility.compatible, true);
    assert.equal(adapter.lastConnection().writes.length, 6);
    assert.deepEqual(adapter.lastConnection().writes.map((report) => report[0]), [1, 2, 8, 8, 8, 8]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[2]), [1, 1, 2, 2]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[3]), [1, 2, 3, 4]);

    const disconnected = await service.disconnect();
    assert.equal(disconnected.connected, false);
    assert.equal(disconnected.capabilities, null);
});

test("compatibility reports schema and source-capacity blockers", async () => {
    const {service} = serviceHarness({schemaMajor: 2, maxLayers: 8});
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const changed = service.setProfileSummary({layerCount: 9});
    assert.equal(changed.compatibility.compatible, false);
    assert.match(changed.compatibility.reasons.join("\n"), /Schema major/);
    assert.match(changed.compatibility.reasons.join("\n"), /Logical layers needs 9/);
});

test("compatibility is recomputed when the active source profile changes", async () => {
    const {service} = serviceHarness();
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const changed = service.setProfileSummary({layerCount: 9});
    assert.equal(changed.compatibility.compatible, false);
    assert.match(changed.compatibility.reasons[0], /Logical layers/);
});

test("compatibility checks Milestone A domains and fixed report framing", () => {
    const compatibility = evaluateProfileCompatibility({
        protocol: {major: 1},
        schema: {major: 1},
        reportSize: 31,
        statusPageCount: 2,
        supportedDomainMask: 1,
        maxLogicalLayers: 8,
        maxBehaviorRows: 64,
        maxTapStepsPerBehavior: 5,
        maxPopulatedBehaviorSteps: 128,
        maxCombos: 32,
        maxKeysPerCombo: 4,
        maxReusableRgbGroups: 16,
        maxRgbStageGroupRows: 32,
        physicalLedCount: 58,
    }, {}, {protocolVersion: 12, firmwareVersion: 0});
    assert.equal(compatibility.compatible, false);
    assert.match(compatibility.reasons.join("\n"), /Raw HID report size/);
    assert.match(compatibility.reasons.join("\n"), /Milestone A domains/);
});

test("request ids advance across refreshes and reject a delayed duplicate", async () => {
    const pages = readPages();
    let delayedResponse;
    let customWriteCount = 0;
    const adapter = new FakeDeviceAdapter({
        devices: [{id: "native-device", product: "Charybdis"}],
        onWrite({connection, report}) {
            if (report[0] === VIA_READS.COMMAND_GET_PROTOCOL_VERSION) {
                const viaResponse = Buffer.from(report);
                viaResponse[2] = VIA_READS.EXPECTED_PROTOCOL_VERSION;
                queueMicrotask(() => connection.emitReport(viaResponse));
                return;
            }
            if (report[0] === VIA_READS.COMMAND_GET_KEYBOARD_VALUE) {
                queueMicrotask(() => connection.emitReport(Buffer.from(report)));
                return;
            }
            const current = response(report, pages[`${report[2]}:${report[4]}`]);
            customWriteCount += 1;
            if (customWriteCount === 1) delayedResponse = Buffer.from(current);
            if (customWriteCount === 5) {
                queueMicrotask(() => connection.emitReport(delayedResponse));
            }
            queueMicrotask(() => connection.emitReport(current));
        },
    });
    const service = new ProfileDeviceService({adapter, defaultTimeoutMs: 100});
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const refreshed = await service.refresh();
    assert.equal(refreshed.connected, true);
    assert.equal(refreshed.error, null);
    const customWrites = adapter.lastConnection().writes.filter((report) => report[0] === PROFILE_WIRE_V1.COMMAND_GET);
    assert.deepEqual(customWrites.map((report) => report[3]), [1, 2, 3, 4, 5, 6, 7, 8]);
    assert.ok(refreshed.diagnostics.some((entry) => entry.includes("unexpected Raw HID report")));
});

test("request id allocation wraps from 255 to 1 without emitting zero", () => {
    const ids = new ProfileRequestIdSequence(0xfe);
    assert.deepEqual([ids.next(), ids.next(), ids.next(), ids.next()], [0xfe, 0xff, 1, 2]);
    assert.throws(() => new ProfileRequestIdSequence(0), /1 through 255/);
});
