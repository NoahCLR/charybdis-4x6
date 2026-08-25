"use strict";

const {
    LIVE_LINK_ERROR_CODES,
    LiveLinkTransportError,
    RAW_HID_REPORT_SIZE,
    liveLinkError,
    normalizeRawHidReport,
} = require("./device-adapter");

const CHARYBDIS_VENDOR_ID = 0xA8F8;
const CHARYBDIS_PRODUCT_ID = 0x1833;
const QMK_RAW_HID_USAGE_PAGE = 0xFF60;
const QMK_RAW_HID_USAGE = 0x61;
const NODE_HID_REPORT_ID = 0;
const NODE_HID_REPORT_SIZE = RAW_HID_REPORT_SIZE + 1;

class NodeHidDeviceAdapter {
    constructor(options = {}) {
        this.hidModule = options.hidModule;
        this.loadHid = options.loadHid || loadNodeHid;
        this.platform = options.platform || process.platform;
        this.hidModulePromise = undefined;
        this.knownDevices = new Map();
    }

    async listDevices() {
        const hid = await this.getHidModule();
        let devices;
        try {
            devices = await hid.devicesAsync(CHARYBDIS_VENDOR_ID, CHARYBDIS_PRODUCT_ID);
        } catch (cause) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.ENUMERATION_FAILED,
                "Could not enumerate Charybdis Raw HID interfaces.",
                {cause}
            );
        }

        if (!Array.isArray(devices)) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.ENUMERATION_FAILED,
                "node-hid devicesAsync() did not return an array."
            );
        }

        this.knownDevices.clear();
        const matches = [];
        for (const device of devices) {
            if (!isCharybdisRawHidDevice(device)) {
                continue;
            }
            const path = typeof device.path === "string" ? device.path : "";
            if (!path || this.knownDevices.has(path)) {
                continue;
            }
            const descriptor = deviceDescriptor(device, path);
            this.knownDevices.set(descriptor.id, descriptor);
            matches.push(descriptor);
        }
        return matches;
    }

    async connect(deviceId) {
        const id = normalizeDeviceId(deviceId);
        let descriptor = this.knownDevices.get(id);
        if (!descriptor) {
            await this.listDevices();
            descriptor = this.knownDevices.get(id);
        }
        if (!descriptor) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.DEVICE_NOT_FOUND,
                `No matching Charybdis Raw HID interface was found for ${id}.`,
                {deviceId: id}
            );
        }

        const hid = await this.getHidModule();
        let nativeDevice;
        try {
            nativeDevice = this.platform === "darwin"
                ? await hid.HIDAsync.open(descriptor.path, {nonExclusive: true})
                : await hid.HIDAsync.open(descriptor.path);
        } catch (cause) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.CONNECT_FAILED,
                `Could not open Charybdis Raw HID interface ${id}.`,
                {cause, deviceId: id}
            );
        }

        try {
            return new NodeHidConnectedDevice(nativeDevice, id);
        } catch (cause) {
            await closeNativeDevice(nativeDevice);
            if (cause instanceof LiveLinkTransportError) {
                throw cause;
            }
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.CONNECT_FAILED,
                `Could not initialize Charybdis Raw HID interface ${id}.`,
                {cause, deviceId: id}
            );
        }
    }

    async getHidModule() {
        if (!this.hidModulePromise) {
            this.hidModulePromise = Promise.resolve()
                .then(() => this.hidModule || this.loadHid())
                .then(validateHidModule)
                .catch((cause) => {
                    this.hidModulePromise = undefined;
                    if (cause instanceof LiveLinkTransportError) {
                        throw cause;
                    }
                    throw liveLinkError(
                        LIVE_LINK_ERROR_CODES.NATIVE_MODULE_UNAVAILABLE,
                        "The node-hid native module is unavailable.",
                        {cause}
                    );
                });
        }
        return this.hidModulePromise;
    }
}

class NodeHidConnectedDevice {
    constructor(nativeDevice, deviceId) {
        if (!nativeDevice || typeof nativeDevice.write !== "function" ||
            typeof nativeDevice.close !== "function" || typeof nativeDevice.on !== "function") {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.INVALID_ADAPTER,
                "node-hid returned an invalid connected device handle.",
                {deviceId}
            );
        }
        this.nativeDevice = nativeDevice;
        this.deviceId = deviceId;
        this.reportListeners = new Set();
        this.disconnectListeners = new Set();
        this.disconnected = false;
        this.disconnectReason = undefined;
        this.closePromise = undefined;
        this.handleData = (data) => this.receiveNativeReport(data);
        this.handleError = (cause) => this.fail(liveLinkError(
            LIVE_LINK_ERROR_CODES.DISCONNECTED,
            `Charybdis Raw HID interface ${this.deviceId} disconnected.`,
            {cause: cause instanceof Error ? cause : undefined, deviceId: this.deviceId}
        ));
        this.nativeDevice.on("data", this.handleData);
        this.nativeDevice.on("error", this.handleError);
    }

    async write(report) {
        if (this.disconnected) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.DISCONNECTED,
                `Charybdis Raw HID interface ${this.deviceId} is disconnected.`,
                {deviceId: this.deviceId}
            );
        }
        const nativeReport = addNodeHidReportId(report);
        try {
            await this.nativeDevice.write(nativeReport);
        } catch (cause) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.WRITE_FAILED,
                `Could not write to Charybdis Raw HID interface ${this.deviceId}.`,
                {cause, deviceId: this.deviceId}
            );
        }
    }

    onReport(listener) {
        if (typeof listener !== "function") {
            throw new TypeError("report listener must be a function.");
        }
        if (!this.disconnected) {
            this.reportListeners.add(listener);
        }
        return () => this.reportListeners.delete(listener);
    }

    onDisconnect(listener) {
        if (typeof listener !== "function") {
            throw new TypeError("disconnect listener must be a function.");
        }
        if (this.disconnectReason) {
            const reason = this.disconnectReason;
            queueMicrotask(() => listener(reason));
            return () => undefined;
        }
        if (!this.disconnected) {
            this.disconnectListeners.add(listener);
        }
        return () => this.disconnectListeners.delete(listener);
    }

    async close() {
        if (this.closePromise) {
            return this.closePromise;
        }
        this.disconnected = true;
        this.detachNativeListeners();
        this.reportListeners.clear();
        this.disconnectListeners.clear();
        this.closePromise = Promise.resolve().then(() => this.nativeDevice.close());
        return this.closePromise;
    }

    receiveNativeReport(data) {
        if (this.disconnected) {
            return;
        }
        let report;
        try {
            report = normalizeNodeHidInputReport(data);
        } catch (cause) {
            this.fail(liveLinkError(
                LIVE_LINK_ERROR_CODES.INVALID_REPORT,
                `Charybdis Raw HID interface ${this.deviceId} returned an invalid native report.`,
                {cause, deviceId: this.deviceId}
            ));
            return;
        }
        for (const listener of Array.from(this.reportListeners)) {
            try {
                listener(Buffer.from(report));
            } catch {
                // Observers must not interrupt the native read loop.
            }
        }
    }

    fail(reason) {
        if (this.disconnected) {
            return;
        }
        this.disconnected = true;
        this.disconnectReason = reason;
        this.detachNativeListeners();
        this.reportListeners.clear();
        for (const listener of Array.from(this.disconnectListeners)) {
            try {
                listener(reason);
            } catch {
                // Observers must not interrupt native cleanup.
            }
        }
        this.disconnectListeners.clear();
        this.closePromise = closeNativeDevice(this.nativeDevice);
    }

    detachNativeListeners() {
        removeNativeListener(this.nativeDevice, "data", this.handleData);
        removeNativeListener(this.nativeDevice, "error", this.handleError);
    }
}

function loadNodeHid() {
    return require("node-hid");
}

function validateHidModule(hid) {
    if (!hid || typeof hid.devicesAsync !== "function" || typeof hid.HIDAsync?.open !== "function") {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.NATIVE_MODULE_UNAVAILABLE,
            "node-hid must provide devicesAsync() and HIDAsync.open()."
        );
    }
    return hid;
}

function isCharybdisRawHidDevice(device) {
    return Number(device?.vendorId) === CHARYBDIS_VENDOR_ID &&
        Number(device?.productId) === CHARYBDIS_PRODUCT_ID &&
        Number(device?.usagePage) === QMK_RAW_HID_USAGE_PAGE &&
        Number(device?.usage) === QMK_RAW_HID_USAGE;
}

function deviceDescriptor(device, path) {
    return {
        id: path,
        path,
        vendorId: CHARYBDIS_VENDOR_ID,
        productId: CHARYBDIS_PRODUCT_ID,
        usagePage: QMK_RAW_HID_USAGE_PAGE,
        usage: QMK_RAW_HID_USAGE,
        serialNumber: optionalString(device.serialNumber),
        manufacturer: optionalString(device.manufacturer),
        product: optionalString(device.product),
        release: optionalNumber(device.release),
        interface: optionalNumber(device.interface),
    };
}

function optionalString(value) {
    return typeof value === "string" ? value : undefined;
}

function optionalNumber(value) {
    return Number.isInteger(value) ? value : undefined;
}

function normalizeDeviceId(deviceId) {
    const id = typeof deviceId === "string" ? deviceId.trim() : "";
    if (!id) {
        throw new TypeError("deviceId must be a non-empty string.");
    }
    return id;
}

function addNodeHidReportId(report) {
    const frame = normalizeRawHidReport(report, "Raw HID output report");
    const nativeReport = Buffer.alloc(NODE_HID_REPORT_SIZE);
    nativeReport[NODE_HID_REPORT_ID] = NODE_HID_REPORT_ID;
    frame.copy(nativeReport, 1);
    return nativeReport;
}

function normalizeNodeHidInputReport(report) {
    if (!(report instanceof Uint8Array)) {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_REPORT,
            "node-hid input report must be a Buffer or Uint8Array."
        );
    }
    if (report.byteLength === RAW_HID_REPORT_SIZE) {
        return Buffer.from(report.buffer, report.byteOffset, report.byteLength);
    }
    if (report.byteLength === NODE_HID_REPORT_SIZE && report[0] === NODE_HID_REPORT_ID) {
        return Buffer.from(report.buffer, report.byteOffset + 1, RAW_HID_REPORT_SIZE);
    }
    throw liveLinkError(
        LIVE_LINK_ERROR_CODES.INVALID_REPORT,
        `node-hid input report must be ${RAW_HID_REPORT_SIZE} protocol bytes or ` +
            `${NODE_HID_REPORT_SIZE} bytes beginning with report id ${NODE_HID_REPORT_ID}.`
    );
}

function removeNativeListener(device, eventName, listener) {
    if (typeof device.off === "function") {
        device.off(eventName, listener);
    } else if (typeof device.removeListener === "function") {
        device.removeListener(eventName, listener);
    }
}

function closeNativeDevice(device) {
    if (!device || typeof device.close !== "function") {
        return Promise.resolve();
    }
    return Promise.resolve().then(() => device.close()).catch(() => undefined);
}

module.exports = {
    CHARYBDIS_PRODUCT_ID,
    CHARYBDIS_VENDOR_ID,
    NODE_HID_REPORT_ID,
    NODE_HID_REPORT_SIZE,
    NodeHidConnectedDevice,
    NodeHidDeviceAdapter,
    QMK_RAW_HID_USAGE,
    QMK_RAW_HID_USAGE_PAGE,
    addNodeHidReportId,
    isCharybdisRawHidDevice,
    normalizeNodeHidInputReport,
};
