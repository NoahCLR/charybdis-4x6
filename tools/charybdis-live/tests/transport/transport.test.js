"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {
    LIVE_LINK_ERROR_CODES,
    RAW_HID_REPORT_SIZE,
} = require("../../core/transport/device-adapter");
const {FakeDeviceAdapter} = require("../../core/transport/fake-device-adapter");
const {DeviceRequestCoordinator} = require("../../core/transport/request-coordinator");

function report(command, marker = 0) {
    const value = Buffer.alloc(RAW_HID_REPORT_SIZE);
    value[0] = command;
    value[RAW_HID_REPORT_SIZE - 1] = marker;
    return value;
}

function tick() {
    return new Promise((resolve) => setImmediate(resolve));
}

async function rejectsWithCode(promise, code) {
    await assert.rejects(promise, (error) => {
        assert.equal(error.code, code);
        return true;
    });
}

test("adapter boundary enforces exact 32-byte outbound reports", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter);
    const connection = await coordinator.connect("fake-charybdis");

    await rejectsWithCode(connection.request(Buffer.alloc(31)), LIVE_LINK_ERROR_CODES.INVALID_REPORT);
    await rejectsWithCode(connection.request(Buffer.alloc(33)), LIVE_LINK_ERROR_CODES.INVALID_REPORT);
    await assert.rejects(connection.request(new Array(32).fill(0)), /Buffer or Uint8Array/);
    assert.equal(adapter.lastConnection().writes.length, 0);

    await coordinator.close();
});

test("requests are FIFO and only one request is active per device", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter);
    const connection = await coordinator.connect("fake-charybdis");
    const fake = adapter.lastConnection();

    const first = connection.request(report(0x11, 1));
    const second = connection.request(report(0x12, 2));
    const third = connection.request(report(0x13, 3));
    await tick();
    assert.deepEqual(fake.writes.map((value) => value[0]), [0x11]);

    fake.emitReport(report(0x11, 11));
    assert.equal((await first)[31], 11);
    await tick();
    assert.deepEqual(fake.writes.map((value) => value[0]), [0x11, 0x12]);

    fake.emitReport(report(0x12, 12));
    assert.equal((await second)[31], 12);
    await tick();
    assert.deepEqual(fake.writes.map((value) => value[0]), [0x11, 0x12, 0x13]);

    fake.emitReport(report(0x13, 13));
    assert.equal((await third)[31], 13);
    await coordinator.close();
});

test("serialization is per device rather than global", async () => {
    const adapter = new FakeDeviceAdapter({devices: [{id: "left"}, {id: "right"}]});
    const coordinator = new DeviceRequestCoordinator(adapter);
    const left = await coordinator.connect("left");
    const right = await coordinator.connect("right");
    const leftFake = adapter.lastConnection("left");
    const rightFake = adapter.lastConnection("right");

    const leftRequest = left.request(report(0x21));
    const rightRequest = right.request(report(0x22));
    await tick();
    assert.equal(leftFake.writes.length, 1);
    assert.equal(rightFake.writes.length, 1);

    leftFake.emitReport(report(0x21));
    rightFake.emitReport(report(0x22));
    await Promise.all([leftRequest, rightRequest]);
    await coordinator.close();
});

test("unexpected replies are isolated until the active reply arrives", async () => {
    const unexpected = [];
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter, {
        onUnexpectedReport(event) {
            unexpected.push(event);
        },
    });
    const connection = await coordinator.connect("fake-charybdis");
    const fake = adapter.lastConnection();

    const pending = connection.request(report(0x31));
    fake.emitReport(report(0x30));
    await tick();
    assert.equal(unexpected.length, 1);
    assert.equal(unexpected[0].report[0], 0x30);
    assert.equal(connection.connected, true);

    fake.emitReport(report(0x31, 7));
    assert.equal((await pending)[31], 7);
    await coordinator.close();
});

test("timeout rejects the active request and invalidates queued work", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter, {defaultTimeoutMs: 20});
    const connection = await coordinator.connect("fake-charybdis");
    const fake = adapter.lastConnection();

    const active = connection.request(report(0x41));
    const queued = connection.request(report(0x42));
    await rejectsWithCode(active, LIVE_LINK_ERROR_CODES.TIMEOUT);
    await rejectsWithCode(queued, LIVE_LINK_ERROR_CODES.DISCONNECTED);
    assert.equal(connection.connected, false);
    assert.equal(fake.closed, true);
    assert.deepEqual(fake.writes.map((value) => value[0]), [0x41]);
});

test("queued cancellation is local but active cancellation invalidates the session", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter);
    const connection = await coordinator.connect("fake-charybdis");
    const fake = adapter.lastConnection();
    const queuedController = new AbortController();

    const first = connection.request(report(0x51));
    const cancelledQueued = connection.request(report(0x52), {signal: queuedController.signal});
    queuedController.abort();
    await rejectsWithCode(cancelledQueued, LIVE_LINK_ERROR_CODES.CANCELLED);
    assert.equal(connection.connected, true);

    fake.emitReport(report(0x51));
    await first;

    const activeController = new AbortController();
    const cancelledActive = connection.request(report(0x53), {signal: activeController.signal});
    const strandedQueued = connection.request(report(0x54));
    activeController.abort();
    await rejectsWithCode(cancelledActive, LIVE_LINK_ERROR_CODES.CANCELLED);
    await rejectsWithCode(strandedQueued, LIVE_LINK_ERROR_CODES.DISCONNECTED);
    assert.equal(connection.connected, false);
    assert.deepEqual(fake.writes.map((value) => value[0]), [0x51, 0x53]);
});

test("adapter disconnect rejects active and queued requests", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter);
    const connection = await coordinator.connect("fake-charybdis");
    const fake = adapter.lastConnection();
    const observed = [];
    connection.onDisconnect((reason) => observed.push(reason));

    const active = connection.request(report(0x61));
    const queued = connection.request(report(0x62));
    fake.emitDisconnect(new Error("USB unplugged"));

    await rejectsWithCode(active, LIVE_LINK_ERROR_CODES.DISCONNECTED);
    await rejectsWithCode(queued, LIVE_LINK_ERROR_CODES.DISCONNECTED);
    assert.equal(observed.length, 1);
    assert.equal(observed[0].cause.message, "USB unplugged");
    assert.equal(coordinator.getConnection("fake-charybdis"), undefined);
});

test("malformed inbound reports invalidate the session", async () => {
    let reportListener;
    const device = {
        async write() {
            queueMicrotask(() => reportListener(Buffer.alloc(31)));
        },
        onReport(listener) {
            reportListener = listener;
            return () => { reportListener = undefined; };
        },
        onDisconnect() {
            return () => undefined;
        },
        async close() {},
    };
    const adapter = {
        async listDevices() { return [{id: "malformed"}]; },
        async connect() { return device; },
    };
    const coordinator = new DeviceRequestCoordinator(adapter);
    const connection = await coordinator.connect("malformed");

    await rejectsWithCode(connection.request(report(0x71)), LIVE_LINK_ERROR_CODES.INVALID_REPORT);
    assert.equal(connection.connected, false);
});

test("a stale reply from an invalidated session cannot resolve a reconnected request", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter, {defaultTimeoutMs: 15});
    const firstConnection = await coordinator.connect("fake-charybdis");
    const firstFake = adapter.lastConnection();

    await rejectsWithCode(firstConnection.request(report(0x81)), LIVE_LINK_ERROR_CODES.TIMEOUT);
    assert.equal(firstFake.closed, true);

    const secondConnection = await coordinator.connect("fake-charybdis");
    const secondFake = adapter.lastConnection();
    let settled = false;
    const secondRequest = secondConnection.request(report(0x81), {timeoutMs: 100});
    secondRequest.then(() => { settled = true; });
    await tick();

    assert.equal(firstFake.emitReport(report(0x81, 1)), false);
    await tick();
    assert.equal(settled, false);

    secondFake.emitReport(report(0x81, 2));
    assert.equal((await secondRequest)[31], 2);
    await coordinator.close();
});

test("manual disconnect permits a clean reconnect", async () => {
    const adapter = new FakeDeviceAdapter();
    const coordinator = new DeviceRequestCoordinator(adapter);
    const first = await coordinator.connect("fake-charybdis");

    await first.disconnect();
    await rejectsWithCode(first.request(report(0x91)), LIVE_LINK_ERROR_CODES.NOT_CONNECTED);
    const second = await coordinator.connect("fake-charybdis");
    assert.equal(second.connected, true);
    assert.equal(adapter.connectionsFor("fake-charybdis").length, 2);

    await coordinator.close();
});
