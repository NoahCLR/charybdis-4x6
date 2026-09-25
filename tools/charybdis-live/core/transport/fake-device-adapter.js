"use strict";

const {
    LIVE_LINK_ERROR_CODES,
    liveLinkError,
    normalizeRawHidReport,
} = require("./device-adapter");

class FakeDeviceAdapter {
    constructor(options = {}) {
        this.devices = Array.isArray(options.devices)
            ? options.devices.map((device) => ({...device}))
            : [{id: "fake-charybdis", product: "Fake Charybdis"}];
        this.onWrite = typeof options.onWrite === "function" ? options.onWrite : undefined;
        this.connectionHistory = [];
    }

    async listDevices() {
        return this.devices.map((device) => ({...device}));
    }

    async connect(deviceId) {
        if (!this.devices.some((device) => device.id === deviceId)) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.DEVICE_NOT_FOUND,
                `Fake device ${deviceId} does not exist.`,
                {deviceId}
            );
        }
        const connection = new FakeConnectedDevice(this, deviceId);
        this.connectionHistory.push(connection);
        return connection;
    }

    connectionsFor(deviceId) {
        return this.connectionHistory.filter((connection) => connection.deviceId === deviceId);
    }

    lastConnection(deviceId) {
        const matches = deviceId === undefined
            ? this.connectionHistory
            : this.connectionsFor(deviceId);
        return matches[matches.length - 1];
    }
}

class FakeConnectedDevice {
    constructor(adapter, deviceId) {
        this.adapter = adapter;
        this.deviceId = deviceId;
        this.closed = false;
        this.writes = [];
        this.reportListeners = new Set();
        this.disconnectListeners = new Set();
    }

    async write(report) {
        if (this.closed) {
            throw liveLinkError(
                LIVE_LINK_ERROR_CODES.DISCONNECTED,
                `Fake device ${this.deviceId} is closed.`,
                {deviceId: this.deviceId}
            );
        }
        const normalized = Buffer.from(normalizeRawHidReport(report, "Fake adapter request"));
        this.writes.push(normalized);
        if (this.adapter.onWrite) {
            await this.adapter.onWrite({
                connection: this,
                deviceId: this.deviceId,
                report: Buffer.from(normalized),
                writeIndex: this.writes.length - 1,
            });
        }
    }

    onReport(listener) {
        this.reportListeners.add(listener);
        return () => this.reportListeners.delete(listener);
    }

    onDisconnect(listener) {
        this.disconnectListeners.add(listener);
        return () => this.disconnectListeners.delete(listener);
    }

    emitReport(report) {
        if (this.closed) {
            return false;
        }
        const normalized = Buffer.from(normalizeRawHidReport(report, "Fake adapter response"));
        for (const listener of Array.from(this.reportListeners)) {
            listener(Buffer.from(normalized));
        }
        return true;
    }

    emitDisconnect(reason = new Error("Fake device unplugged.")) {
        if (this.closed) {
            return false;
        }
        this.closed = true;
        const listeners = Array.from(this.disconnectListeners);
        this.reportListeners.clear();
        this.disconnectListeners.clear();
        for (const listener of listeners) {
            listener(reason);
        }
        return true;
    }

    async close() {
        this.emitDisconnect(new Error("Fake device closed by host."));
    }
}

module.exports = {
    FakeConnectedDevice,
    FakeDeviceAdapter,
};
