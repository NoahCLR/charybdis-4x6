"use strict";

const RAW_HID_REPORT_SIZE = 32;

const LIVE_LINK_ERROR_CODES = Object.freeze({
    ALREADY_CONNECTED: "ALREADY_CONNECTED",
    CANCELLED: "CANCELLED",
    CONNECT_FAILED: "CONNECT_FAILED",
    DEVICE_NOT_FOUND: "DEVICE_NOT_FOUND",
    DISCONNECTED: "DISCONNECTED",
    ENUMERATION_FAILED: "ENUMERATION_FAILED",
    INVALID_ADAPTER: "INVALID_ADAPTER",
    INVALID_REPORT: "INVALID_REPORT",
    NATIVE_MODULE_UNAVAILABLE: "NATIVE_MODULE_UNAVAILABLE",
    NOT_CONNECTED: "NOT_CONNECTED",
    RESPONSE_MATCH_FAILED: "RESPONSE_MATCH_FAILED",
    TIMEOUT: "TIMEOUT",
    WRITE_FAILED: "WRITE_FAILED",
});

class LiveLinkTransportError extends Error {
    constructor(code, message, options = {}) {
        super(message, options.cause === undefined ? undefined : {cause: options.cause});
        this.name = "LiveLinkTransportError";
        this.code = code;
        if (options.deviceId !== undefined) {
            this.deviceId = options.deviceId;
        }
    }
}

function liveLinkError(code, message, options) {
    return new LiveLinkTransportError(code, message, options);
}

function normalizeRawHidReport(value, label = "Raw HID report") {
    if (!(value instanceof Uint8Array)) {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_REPORT,
            `${label} must be a Buffer or Uint8Array.`
        );
    }
    if (value.byteLength !== RAW_HID_REPORT_SIZE) {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_REPORT,
            `${label} must be exactly ${RAW_HID_REPORT_SIZE} bytes; received ${value.byteLength}.`
        );
    }
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function assertDeviceAdapter(adapter) {
    if (!adapter || typeof adapter.listDevices !== "function" || typeof adapter.connect !== "function") {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_ADAPTER,
            "A live-link device adapter must implement listDevices() and connect(deviceId)."
        );
    }
    return adapter;
}

function assertConnectedDevice(device, deviceId) {
    const requiredMethods = ["write", "onReport", "onDisconnect", "close"];
    const missing = requiredMethods.filter((method) => typeof device?.[method] !== "function");
    if (missing.length) {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_ADAPTER,
            `Connected device ${deviceId} is missing: ${missing.join(", ")}.`,
            {deviceId}
        );
    }
    return device;
}

function normalizeDeviceDescriptors(devices) {
    if (!Array.isArray(devices)) {
        throw liveLinkError(
            LIVE_LINK_ERROR_CODES.INVALID_ADAPTER,
            "Device adapter listDevices() must return an array."
        );
    }

    const seen = new Set();
    return devices.map((device, index) => {
        const id = typeof device?.id === "string" ? device.id.trim() : "";
        if (!id || seen.has(id)) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.INVALID_ADAPTER,
                `Device descriptor ${index} must have a unique non-empty string id.`
            );
        }
        seen.add(id);
        return {...device, id};
    });
}

module.exports = {
    LIVE_LINK_ERROR_CODES,
    LiveLinkTransportError,
    RAW_HID_REPORT_SIZE,
    assertConnectedDevice,
    assertDeviceAdapter,
    liveLinkError,
    normalizeDeviceDescriptors,
    normalizeRawHidReport,
};
