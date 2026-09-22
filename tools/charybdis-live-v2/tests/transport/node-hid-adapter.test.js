"use strict";

const assert = require("node:assert/strict");
const {EventEmitter} = require("node:events");
const test = require("node:test");

const {
    LIVE_LINK_ERROR_CODES,
    RAW_HID_REPORT_SIZE,
} = require("../../core/transport/device-adapter");
const {
    CHARYBDIS_PRODUCT_ID,
    CHARYBDIS_VENDOR_ID,
    NODE_HID_REPORT_SIZE,
    NodeHidDeviceAdapter,
    QMK_RAW_HID_USAGE,
    QMK_RAW_HID_USAGE_PAGE,
    addNodeHidReportId,
    normalizeNodeHidInputReport,
} = require("../../core/transport/node-hid-adapter");
const {main: runProbe} = require("../../scripts/probe-live-link");

function matchingDescriptor(overrides = {}) {
    return {
        vendorId: CHARYBDIS_VENDOR_ID,
        productId: CHARYBDIS_PRODUCT_ID,
        usagePage: QMK_RAW_HID_USAGE_PAGE,
        usage: QMK_RAW_HID_USAGE,
        path: "mock://charybdis/raw-hid",
        serialNumber: "CHARYBDIS-TEST",
        manufacturer: "BastardKB",
        product: "Charybdis 4x6",
        release: 1,
        interface: 3,
        ...overrides,
    };
}

class MockNativeHidDevice extends EventEmitter {
    constructor() {
        super();
        this.writes = [];
        this.closeCalls = 0;
    }

    async write(report) {
        this.writes.push(Buffer.from(report));
        return report.byteLength;
    }

    async close() {
        this.closeCalls += 1;
    }
}

function createMockHid(devices = [matchingDescriptor()]) {
    const nativeDevice = new MockNativeHidDevice();
    const calls = {devices: [], open: []};
    const hid = {
        async devicesAsync(...arguments_) {
            calls.devices.push(arguments_);
            return devices;
        },
        HIDAsync: {
            async open(...arguments_) {
                calls.open.push(arguments_);
                return nativeDevice;
            },
        },
    };
    return {calls, hid, nativeDevice};
}

async function rejectsWithCode(promise, code) {
    await assert.rejects(promise, (error) => {
        assert.equal(error.code, code);
        return true;
    });
}

test("node-hid is lazy-loaded only when enumeration starts", async () => {
    const mock = createMockHid();
    let loadCalls = 0;
    const adapter = new NodeHidDeviceAdapter({
        loadHid() {
            loadCalls += 1;
            return mock.hid;
        },
    });

    assert.equal(loadCalls, 0);
    await adapter.listDevices();
    await adapter.listDevices();
    assert.equal(loadCalls, 1);
});

test("enumeration filters exact VID, PID, usage page, and usage", async () => {
    const match = matchingDescriptor();
    const mock = createMockHid([
        match,
        matchingDescriptor({path: "mock://wrong-vendor", vendorId: 0x1234}),
        matchingDescriptor({path: "mock://wrong-product", productId: 0x5678}),
        matchingDescriptor({path: "mock://wrong-page", usagePage: 0x0001}),
        matchingDescriptor({path: "mock://wrong-usage", usage: 0x0006}),
        matchingDescriptor({path: undefined}),
    ]);
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid});

    const devices = await adapter.listDevices();
    assert.deepEqual(mock.calls.devices, [[CHARYBDIS_VENDOR_ID, CHARYBDIS_PRODUCT_ID]]);
    assert.equal(devices.length, 1);
    assert.deepEqual(devices[0], {
        id: match.path,
        path: match.path,
        vendorId: CHARYBDIS_VENDOR_ID,
        productId: CHARYBDIS_PRODUCT_ID,
        usagePage: QMK_RAW_HID_USAGE_PAGE,
        usage: QMK_RAW_HID_USAGE,
        serialNumber: "CHARYBDIS-TEST",
        manufacturer: "BastardKB",
        product: "Charybdis 4x6",
        release: 1,
        interface: 3,
    });
});

test("connect only opens an enumerated Raw HID path and requests non-exclusive macOS access", async () => {
    const mock = createMockHid();
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid, platform: "darwin"});
    const [descriptor] = await adapter.listDevices();

    const connected = await adapter.connect(descriptor.id);
    assert.deepEqual(mock.calls.open, [[descriptor.path, {nonExclusive: true}]]);
    await connected.close();
    assert.equal(mock.nativeDevice.closeCalls, 1);
});

test("connect rejects unknown ids without opening an arbitrary native path", async () => {
    const mock = createMockHid();
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid});

    await rejectsWithCode(adapter.connect("mock://not-enumerated"), LIVE_LINK_ERROR_CODES.DEVICE_NOT_FOUND);
    assert.equal(mock.calls.open.length, 0);
});

test("outbound protocol frames get the node-hid report-id prefix", async () => {
    const frame = Buffer.alloc(RAW_HID_REPORT_SIZE, 0xA5);
    const nativeFrame = addNodeHidReportId(frame);
    assert.equal(nativeFrame.length, NODE_HID_REPORT_SIZE);
    assert.equal(nativeFrame[0], 0);
    assert.deepEqual(nativeFrame.subarray(1), frame);
    assert.throws(() => addNodeHidReportId(Buffer.alloc(31)), (error) => {
        assert.equal(error.code, LIVE_LINK_ERROR_CODES.INVALID_REPORT);
        return true;
    });

    const mock = createMockHid();
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid});
    const [descriptor] = await adapter.listDevices();
    const connected = await adapter.connect(descriptor.id);
    await connected.write(frame);
    assert.deepEqual(mock.nativeDevice.writes, [nativeFrame]);
    await connected.close();
});

test("inbound native reports normalize with or without a zero report id", () => {
    const frame = Buffer.alloc(RAW_HID_REPORT_SIZE, 0x5A);
    assert.deepEqual(normalizeNodeHidInputReport(frame), frame);
    assert.deepEqual(normalizeNodeHidInputReport(Buffer.concat([Buffer.from([0]), frame])), frame);
    assert.throws(() => normalizeNodeHidInputReport(Buffer.concat([Buffer.from([1]), frame])), (error) => {
        assert.equal(error.code, LIVE_LINK_ERROR_CODES.INVALID_REPORT);
        return true;
    });
    assert.throws(() => normalizeNodeHidInputReport(Buffer.alloc(31)), (error) => {
        assert.equal(error.code, LIVE_LINK_ERROR_CODES.INVALID_REPORT);
        return true;
    });
});

test("connected adapter emits only exact 32-byte reports and isolates malformed native input", async () => {
    const mock = createMockHid();
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid});
    const [descriptor] = await adapter.listDevices();
    const connected = await adapter.connect(descriptor.id);
    const reports = [];
    const disconnects = [];
    connected.onReport((report) => reports.push(report));
    connected.onDisconnect((reason) => disconnects.push(reason));

    const frame = Buffer.alloc(RAW_HID_REPORT_SIZE, 0x11);
    mock.nativeDevice.emit("data", Buffer.concat([Buffer.from([0]), frame]));
    assert.deepEqual(reports, [frame]);

    mock.nativeDevice.emit("data", Buffer.alloc(31));
    await Promise.resolve();
    assert.equal(disconnects.length, 1);
    assert.equal(disconnects[0].code, LIVE_LINK_ERROR_CODES.INVALID_REPORT);
    assert.equal(mock.nativeDevice.closeCalls, 1);
});

test("native errors become one disconnect notification and close the handle", async () => {
    const mock = createMockHid();
    const adapter = new NodeHidDeviceAdapter({hidModule: mock.hid});
    const [descriptor] = await adapter.listDevices();
    const connected = await adapter.connect(descriptor.id);
    const reasons = [];
    connected.onDisconnect((reason) => reasons.push(reason));

    mock.nativeDevice.emit("error", new Error("unplugged"));
    await Promise.resolve();
    assert.equal(reasons.length, 1);
    assert.equal(reasons[0].code, LIVE_LINK_ERROR_CODES.DISCONNECTED);
    assert.equal(reasons[0].cause.message, "unplugged");
    assert.equal(mock.nativeDevice.closeCalls, 1);
});

test("native module load and enumeration errors use stable transport codes", async () => {
    const missing = new NodeHidDeviceAdapter({
        loadHid() {
            throw new Error("missing native binary");
        },
    });
    await rejectsWithCode(missing.listDevices(), LIVE_LINK_ERROR_CODES.NATIVE_MODULE_UNAVAILABLE);

    const broken = createMockHid();
    broken.hid.devicesAsync = async () => {
        throw new Error("enumeration denied");
    };
    await rejectsWithCode(
        new NodeHidDeviceAdapter({hidModule: broken.hid}).listDevices(),
        LIVE_LINK_ERROR_CODES.ENUMERATION_FAILED
    );
});

test("CLI probe is enumeration-only", async () => {
    const output = [];
    const calls = {listDevices: 0, connect: 0};
    const adapter = {
        async listDevices() {
            calls.listDevices += 1;
            return [{id: "mock://raw", path: "mock://raw", product: "Test"}];
        },
        async connect() {
            calls.connect += 1;
            throw new Error("must not connect");
        },
    };

    const result = await runProbe({
        adapter,
        argv: ["--json"],
        stdout: {write(value) { output.push(value); }},
    });
    assert.equal(result.count, 1);
    assert.equal(calls.listDevices, 1);
    assert.equal(calls.connect, 0);
    assert.match(output.join(""), /"count": 1/);
});
