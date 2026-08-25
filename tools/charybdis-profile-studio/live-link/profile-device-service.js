"use strict";

const {RAW_HID_REPORT_SIZE} = require("./device-adapter");
const {NodeHidDeviceAdapter} = require("./node-hid-adapter");
const {DeviceRequestCoordinator} = require("./request-coordinator");
const {
    PROFILE_WIRE_KNOWN_MASKS,
    PROFILE_WIRE_V1,
    VIA_READS,
    readProfileCapabilities,
    readProfileStatus,
    readViaIdentity,
} = require("./profile-wire-v1");

const PROFILE_STUDIO_PROTOCOL = Object.freeze({major: 1, minor: 0});
const PROFILE_STUDIO_SCHEMA = Object.freeze({major: 1, minor: 0});
const PROFILE_DOMAIN_FLAGS = Object.freeze({RGB: 1 << 0, KEY_BEHAVIORS: 1 << 1});
const REQUIRED_PROFILE_DOMAIN_MASK = PROFILE_DOMAIN_FLAGS.RGB | PROFILE_DOMAIN_FLAGS.KEY_BEHAVIORS;

class ProfileDeviceService {
    constructor(options = {}) {
        const adapter = options.adapter || new NodeHidDeviceAdapter();
        this.coordinator = options.coordinator || new DeviceRequestCoordinator(adapter, {
            defaultTimeoutMs: options.defaultTimeoutMs,
            onUnexpectedReport: ({reason}) => this.addDiagnostic(`Ignored an unexpected Raw HID report: ${reason}.`),
        });
        this.onChange = typeof options.onChange === "function" ? options.onChange : undefined;
        this.profileSummary = normalizeProfileSummary(options.profileSummary);
        this.requestIdStart = normalizeRequestId(options.requestIdStart === undefined ? 1 : options.requestIdStart);
        this.devices = [];
        this.adapterIdsByPublicId = new Map();
        this.connection = undefined;
        this.connectionPublicId = "";
        this.disposeConnectionListener = undefined;
        this.disconnecting = false;
        this.scanned = false;
        this.busy = false;
        this.phase = "idle";
        this.capabilities = undefined;
        this.status = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.error = undefined;
        this.diagnostics = [];
        this.lastRefreshedAt = "";
    }

    setProfileSummary(summary) {
        this.profileSummary = normalizeProfileSummary(summary);
        this.emitChange();
        return this.snapshot();
    }

    async enumerate() {
        return this.runOperation("enumerating", async () => {
            const descriptors = await this.coordinator.listDevices();
            this.adapterIdsByPublicId.clear();
            this.devices = descriptors.map((descriptor, index) => {
                const id = `charybdis-${index + 1}`;
                this.adapterIdsByPublicId.set(id, descriptor.id);
                return publicDeviceDescriptor(id, descriptor, index);
            });
            this.scanned = true;
            this.phase = this.connection ? "connected" : this.devices.length ? "available" : "empty";
            this.addDiagnostic(
                this.devices.length
                    ? `Found ${this.devices.length} compatible Charybdis Raw HID interface${this.devices.length === 1 ? "" : "s"}.`
                    : "No compatible Charybdis Raw HID interface is currently visible."
            );
        });
    }

    async connect(publicDeviceId) {
        const adapterDeviceId = this.adapterIdsByPublicId.get(String(publicDeviceId || ""));
        if (!adapterDeviceId) {
            this.setError(new Error("Select a keyboard from the latest scan before connecting."));
            this.emitChange();
            return this.snapshot();
        }
        if (this.connection) {
            await this.disconnect();
        }
        const result = await this.runOperation("connecting", async () => {
            const connection = await this.coordinator.connect(adapterDeviceId);
            this.connection = connection;
            this.requestIds = new ProfileRequestIdSequence(this.requestIdStart);
            this.connectionPublicId = String(publicDeviceId);
            this.phase = "connected";
            this.disposeConnectionListener = connection.onDisconnect((reason) => this.handleDisconnect(reason));
            this.addDiagnostic(`Connected to ${this.deviceLabel(this.connectionPublicId)}.`);
        });
        if (this.connection && !this.error) {
            return this.refresh();
        }
        return result;
    }

    async refresh() {
        if (!this.connection?.connected) {
            this.setError(new Error("Connect to a keyboard before refreshing live capabilities."));
            this.emitChange();
            return this.snapshot();
        }
        return this.runOperation("refreshing", async () => {
            const connection = this.connection;
            const viaIdentity = await readViaIdentity(connection);
            const capabilities = await readProfileCapabilities(connection, {nextRequestId: () => this.requestIds.next()});
            const status = await readProfileStatus(connection, {nextRequestId: () => this.requestIds.next()});
            if (this.connection !== connection || !connection.connected) {
                throw new Error("The keyboard disconnected while live state was being refreshed.");
            }
            this.capabilities = capabilities;
            this.status = status;
            this.viaIdentity = viaIdentity;
            this.lastRefreshedAt = new Date().toISOString();
            this.phase = "connected";
            const compatibility = evaluateProfileCompatibility(capabilities, this.profileSummary, viaIdentity);
            this.addDiagnostic(
                compatibility.compatible
                    ? "Live profile schema and current source capacities are compatible."
                    : `Live compatibility check found ${compatibility.reasons.length} blocking issue${compatibility.reasons.length === 1 ? "" : "s"}.`
            );
        });
    }

    async disconnect() {
        const connection = this.connection;
        if (!connection) {
            this.phase = this.scanned ? (this.devices.length ? "available" : "empty") : "idle";
            this.emitChange();
            return this.snapshot();
        }
        this.busy = true;
        this.phase = "disconnecting";
        this.error = undefined;
        this.emitChange();
        this.disconnecting = true;
        this.disposeConnectionListener?.();
        this.disposeConnectionListener = undefined;
        try {
            await connection.disconnect();
        } catch (error) {
            this.setError(error);
        } finally {
            this.disconnecting = false;
            this.clearConnection();
            this.busy = false;
            this.phase = this.scanned ? (this.devices.length ? "available" : "empty") : "idle";
            this.addDiagnostic("Disconnected from the live keyboard.");
            this.emitChange();
        }
        return this.snapshot();
    }

    async close() {
        this.disposeConnectionListener?.();
        this.disposeConnectionListener = undefined;
        this.disconnecting = true;
        try {
            await this.coordinator.close();
        } finally {
            this.disconnecting = false;
            this.clearConnection();
        }
    }

    snapshot() {
        return {
            phase: this.phase,
            scanned: this.scanned,
            busy: this.busy,
            connected: Boolean(this.connection?.connected),
            devices: this.devices.map((device) => ({...device})),
            selectedDeviceId: this.connectionPublicId,
            capabilities: this.capabilities ? {...this.capabilities} : null,
            status: this.status ? {...this.status} : null,
            viaIdentity: this.viaIdentity ? {...this.viaIdentity} : null,
            compatibility: this.capabilities
                ? evaluateProfileCompatibility(this.capabilities, this.profileSummary, this.viaIdentity)
                : null,
            error: this.error ? {...this.error} : null,
            diagnostics: this.diagnostics.slice(),
            lastRefreshedAt: this.lastRefreshedAt,
        };
    }

    async runOperation(phase, operation) {
        this.busy = true;
        this.phase = phase;
        this.error = undefined;
        this.emitChange();
        try {
            await operation();
        } catch (error) {
            this.setError(error);
            if (!this.connection?.connected) {
                this.clearConnection();
            }
            this.phase = this.connection?.connected
                ? "connected"
                : this.scanned ? (this.devices.length ? "available" : "empty") : "idle";
        } finally {
            this.busy = false;
            this.emitChange();
        }
        return this.snapshot();
    }

    handleDisconnect(reason) {
        if (this.disconnecting) {
            return;
        }
        const label = this.deviceLabel(this.connectionPublicId);
        this.clearConnection();
        this.busy = false;
        this.phase = this.scanned ? (this.devices.length ? "available" : "empty") : "idle";
        this.setError(reason instanceof Error ? reason : new Error(`${label} disconnected.`));
        this.addDiagnostic(`${label} disconnected; reconnect before sending another read request.`);
        this.emitChange();
    }

    clearConnection() {
        this.disposeConnectionListener?.();
        this.disposeConnectionListener = undefined;
        this.connection = undefined;
        this.connectionPublicId = "";
        this.capabilities = undefined;
        this.status = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.lastRefreshedAt = "";
    }

    setError(error) {
        this.error = publicError(error);
        this.addDiagnostic(`${this.error.code}: ${this.error.message}`);
    }

    addDiagnostic(message) {
        const text = String(message || "").trim();
        if (!text || this.diagnostics[this.diagnostics.length - 1] === text) {
            return;
        }
        this.diagnostics.push(text);
        if (this.diagnostics.length > 8) {
            this.diagnostics.splice(0, this.diagnostics.length - 8);
        }
    }

    deviceLabel(publicDeviceId) {
        return this.devices.find((device) => device.id === publicDeviceId)?.label || "Charybdis keyboard";
    }

    emitChange() {
        if (!this.onChange) {
            return;
        }
        try {
            this.onChange(this.snapshot());
        } catch {
            // A closed or reloading webview must not break device cleanup.
        }
    }
}

function publicDeviceDescriptor(id, descriptor, index) {
    const product = cleanText(descriptor.product) || "Charybdis 4x6";
    const serial = cleanText(descriptor.serialNumber);
    const suffix = serial ? ` · ${serial}` : index ? ` · interface ${index + 1}` : "";
    return {
        id,
        label: product + suffix,
        product,
        serialNumber: serial,
        manufacturer: cleanText(descriptor.manufacturer),
    };
}

function cleanText(value) {
    return typeof value === "string" ? value.trim() : "";
}

function publicError(error) {
    return {
        code: cleanText(error?.code) || cleanText(error?.name) || "LIVE_LINK_ERROR",
        message: cleanText(error?.message) || String(error || "Unknown live-link error."),
    };
}

function normalizeProfileSummary(summary = {}) {
    const normalized = {};
    for (const key of [
        "layerCount",
        "behaviorRows",
        "maxTapStepsPerBehavior",
        "populatedBehaviorSteps",
        "comboCount",
        "maxKeysPerCombo",
        "reusableRgbGroups",
        "rgbStageGroupRows",
        "highestLedIndex",
    ]) {
        const value = Number(summary?.[key]);
        normalized[key] = Number.isInteger(value) && value >= 0 ? value : 0;
    }
    return normalized;
}

function evaluateProfileCompatibility(capabilities, summary = {}, viaIdentity = {}) {
    const source = normalizeProfileSummary(summary);
    const checks = [
        equalityCheck("Protocol major", capabilities?.protocol?.major, PROFILE_STUDIO_PROTOCOL.major),
        equalityCheck("Schema major", capabilities?.schema?.major, PROFILE_STUDIO_SCHEMA.major),
        equalityCheck("VIA protocol version", viaIdentity?.protocolVersion, VIA_READS.EXPECTED_PROTOCOL_VERSION),
        equalityCheck("VIA firmware version", viaIdentity?.firmwareVersion, capabilities?.firmwareVersion),
        equalityCheck("Raw HID report size", capabilities?.reportSize, RAW_HID_REPORT_SIZE),
        minimumCheck("Status pages", capabilities?.statusPageCount, PROFILE_WIRE_V1.STATUS_PAGE_COUNT),
        maskCheck("Required Profile Wire features", capabilities?.featureFlags, PROFILE_WIRE_KNOWN_MASKS.REQUIRED_READ_FEATURES),
        maskCheck("Milestone A domains", capabilities?.supportedDomainMask, REQUIRED_PROFILE_DOMAIN_MASK),
        capacityCheck("Logical layers", source.layerCount, capabilities?.maxLogicalLayers),
        capacityCheck("Key behavior rows", source.behaviorRows, capabilities?.maxBehaviorRows),
        capacityCheck("Tap steps per behavior", source.maxTapStepsPerBehavior, capabilities?.maxTapStepsPerBehavior),
        capacityCheck("Populated behavior steps", source.populatedBehaviorSteps, capabilities?.maxPopulatedBehaviorSteps),
        capacityCheck("Combos", source.comboCount, capabilities?.maxCombos),
        capacityCheck("Keys per combo", source.maxKeysPerCombo, capabilities?.maxKeysPerCombo),
        capacityCheck("Reusable RGB groups", source.reusableRgbGroups, capabilities?.maxReusableRgbGroups),
        capacityCheck("RGB stage-group rows", source.rgbStageGroupRows, capabilities?.maxRgbStageGroupRows),
        ledIndexCheck(source.highestLedIndex, capabilities?.physicalLedCount),
    ];
    const reasons = checks.filter((check) => !check.ok).map((check) => check.message);
    return {
        compatible: reasons.length === 0,
        reasons,
        checks,
        requiredDomainMask: REQUIRED_PROFILE_DOMAIN_MASK,
        source,
    };
}

class ProfileRequestIdSequence {
    constructor(start = 1) {
        this.value = normalizeRequestId(start);
    }

    next() {
        const current = this.value;
        this.value = current === 0xff ? 1 : current + 1;
        return current;
    }
}

function normalizeRequestId(value) {
    const requestId = Number(value);
    if (!Number.isInteger(requestId) || requestId < 1 || requestId > 0xff) {
        throw new RangeError("Profile Wire request id must be an integer from 1 through 255.");
    }
    return requestId;
}

function equalityCheck(label, actual, expected) {
    const ok = Number(actual) === Number(expected);
    return {label, ok, actual, limit: expected, message: ok ? "" : `${label} is ${actual}; Profile Studio requires ${expected}.`};
}

function minimumCheck(label, actual, expected) {
    const ok = Number.isInteger(actual) && actual >= expected;
    return {label, ok, actual, limit: expected, message: ok ? "" : `${label} is ${actual}; at least ${expected} is required.`};
}

function maskCheck(label, actual, expected) {
    const numeric = Number(actual) || 0;
    const ok = (numeric & expected) === expected;
    return {label, ok, actual: numeric, limit: expected, message: ok ? "" : `${label} mask 0x${numeric.toString(16)} does not include required mask 0x${expected.toString(16)}.`};
}

function capacityCheck(label, actual, limit) {
    const ok = Number.isInteger(limit) && actual <= limit;
    return {label, ok, actual, limit, message: ok ? "" : `${label} needs ${actual}; firmware capacity is ${limit}.`};
}

function ledIndexCheck(highestLedIndex, physicalLedCount) {
    const limit = Number.isInteger(physicalLedCount) ? Math.max(0, physicalLedCount - 1) : physicalLedCount;
    const ok = highestLedIndex === 0 || (Number.isInteger(limit) && highestLedIndex <= limit);
    return {
        label: "Highest RGB LED index",
        ok,
        actual: highestLedIndex,
        limit,
        message: ok ? "" : `RGB data references LED ${highestLedIndex}; firmware ends at LED ${limit}.`,
    };
}

module.exports = {
    PROFILE_DOMAIN_FLAGS,
    PROFILE_STUDIO_PROTOCOL,
    PROFILE_STUDIO_SCHEMA,
    ProfileRequestIdSequence,
    ProfileDeviceService,
    REQUIRED_PROFILE_DOMAIN_MASK,
    evaluateProfileCompatibility,
    normalizeProfileSummary,
};
