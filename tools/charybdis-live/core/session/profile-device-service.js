"use strict";

const {baseLighting} = require("../model/settings-editor");
const {profilePlacementProblem} = require("../model/profile-placement");
const {actionLimitsFor} = require("../schema/actions");
const {layerName} = require("../model/vocabulary");
const {decodePdDomain} = require("../schema/pd-mode-domain-v1");
const {readSettingsLimits} = require("../protocol/portable-profile-v1");
const {readKeyboardOptions} = require("../protocol/keyboard-options-v1");
const {decodeSettings} = require("../schema/settings-domain-v1");
const {captureProfile, restoreProfile, validateSnapshot} = require("./portable-profile-session");
const {settingsEditorView} = require("../model/settings-editor");
const {macroEditorView} = require("../model/macro-editor");
const {customKeyEditorView} = require("../model/custom-key-editor");
const {RAW_HID_REPORT_SIZE} = require("../transport/device-adapter");
const {NodeHidDeviceAdapter} = require("../transport/node-hid-adapter");
const {DeviceRequestCoordinator} = require("../transport/request-coordinator");
const {readViaLayout} = require("../protocol/via-layout-v1");
const keycodeCatalog = require("../data/keycode-catalog");
const {readViaRgbMatrix} = require("../protocol/via-rgb-matrix-v1");
const {readDeviceCombos} = require("../protocol/combo-readback-v1");
const {readCommittedPayload, readCompiledPayload} = require("../protocol/profile-payload-v1");
const {PROFILE_DOMAIN_IDS, decodeProfileBlob} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1} = require("../schema/rgb-domain-v1");
const {decodeComboDomain} = require("../schema/combo-domain-v1");
const {decodeKeyBehaviorDomain} = require("../schema/key-behavior-domain-v1");
const {CANDIDATE_STATE_NAMES, readCandidateStatus} = require("../protocol/profile-candidate-v1");
const {PROFILE_ACTIVE_KIND, PROFILE_STATE_FLAGS, PROFILE_WIRE_KNOWN_MASKS, PROFILE_WIRE_FEATURES, PROFILE_WIRE_V1, VIA_READS, readProfileCapabilities, readProfileStatus, readViaIdentity} = require("../protocol/profile-wire-v1");

const PROFILE_STUDIO_PROTOCOL = Object.freeze({major: 1, minor: 0});
const PROFILE_STUDIO_SCHEMA = Object.freeze({major: 1, minor: 0});
const PROFILE_DOMAIN_FLAGS = Object.freeze({RGB: 1 << 0, KEY_BEHAVIORS: 1 << 1});
const REQUIRED_PROFILE_DOMAIN_MASK = PROFILE_DOMAIN_FLAGS.RGB | PROFILE_DOMAIN_FLAGS.KEY_BEHAVIORS;
const REQUIRED_LIVE_MUTATION_FEATURES = PROFILE_WIRE_FEATURES.CANDIDATE_WRITE
    | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT
    | PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION
    | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION
    | PROFILE_WIRE_FEATURES.ATOMIC_LOGICAL_APPLY;

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
        this.readCandidateStatus = typeof options.readCandidateStatus === "function"
            ? options.readCandidateStatus
            : readCandidateStatus;
        this.devices = [];
        this.adapterIdsByPublicId = new Map();
        this.publicIdsByAdapterIdentity = new Map();
        this.nextPublicDeviceId = 1;
        this.connection = undefined;
        this.connectionPublicId = "";
        this.connectionSequence = 0;
        this.connectionToken = null;
        this.disposeConnectionListener = undefined;
        this.disconnecting = false;
        this.scanned = false;
        this.busy = false;
        this.phase = "idle";
        this.capabilities = undefined;
        this.status = undefined;
        this.candidateStatus = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.error = undefined;
        this.diagnostics = [];
        this.lastRefreshedAt = "";
        this.liveApply = {state: "idle"};
        this.layout = undefined;
        this.committed = undefined;
        this.profileBytes = undefined;
        this.baseRgb = undefined;
        this.combos = undefined;
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
                const identity = JSON.stringify([descriptor.id, cleanText(descriptor.serialNumber)]);
                let id = this.publicIdsByAdapterIdentity.get(identity);
                if (!id) {
                    id = `charybdis-${this.nextPublicDeviceId++}`;
                    this.publicIdsByAdapterIdentity.set(identity, id);
                }
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
            this.connectionToken = ++this.connectionSequence;
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
            const candidateStatus = (capabilities.featureFlags & PROFILE_WIRE_FEATURES.CANDIDATE_WRITE) !== 0
                ? await this.readCandidateStatus(connection, {nextRequestId: () => this.requestIds.next()})
                : undefined;
            if (this.connection !== connection || !connection.connected) {
                throw new Error("The keyboard disconnected while live state was being refreshed.");
            }
            this.capabilities = capabilities;
            this.status = status;
            this.candidateStatus = candidateStatus;
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

// Reads the layout the keyboard is actually running and resolves each
    // keycode through the vendored catalog. This is device-first: nothing here
    // consults the authored source. Layer count comes from the firmware's own
    // advertised capacity rather than from a compiled constant.
    async readLayout() {
        if (!this.connection?.connected) {
            this.setError(new Error("Connect to a keyboard before reading its layout."));
            this.emitChange();
            return this.snapshot();
        }
        const layerCount = this.capabilities?.compiledLayerCount;
        if (!Number.isInteger(layerCount) || layerCount < 1) {
            this.setError(new Error("Refresh capabilities before reading the layout."));
            this.emitChange();
            return this.snapshot();
        }

        return this.runOperation("reading layout", async () => {
            const connection = this.connection;
            this.layout = {state: "reading", progress: {done: 0, total: 0}, layers: [], readAt: ""};
            this.emitChange();

            const layers = await readViaLayout(connection, {
                layerCount,
                onProgress: (progress) => {
                    this.layout = {...this.layout, progress};
                    this.emitChange();
                },
            });
            if (this.connection !== connection || !connection.connected) {
                throw new Error("The keyboard disconnected while its layout was being read.");
            }

            this.layout = {
                state: "read",
                progress: null,
                readAt: new Date().toISOString(),
                catalog: keycodeCatalog.metadata(),
                layers: layers.map(({layer, positions}) => ({
                    layer,
                    keys: positions.map((position) => ({
                        ...position,
                        resolved: keycodeCatalog.resolve(position.keycode),
                    })),
                })),
            };
            const unknown = this.layout.layers
                .flatMap((entry) => entry.keys)
                .filter((key) => !key.resolved.known).length;
            this.addDiagnostic(
                unknown === 0
                    ? `Read ${layerCount} layers from the keyboard.`
                    : `Read ${layerCount} layers; ${unknown} keycodes are not in the vendored catalog.`
            );
        });
    }

// Applies layout edits to the keyboard and reads each one back.
    layerIndexFor(name) {
        const match = String(name || "").match(/(\d+)/);
        if (!match) {
            return undefined;
        }
        const index = Number(match[1]);
        const known = this.layout?.layers?.some((entry) => entry.layer === index);
        return known ? index : undefined;
    }

    // Keep the cached layout in step with the device so the UI does not need a
    // full re-read after every edit.
    applyLayoutKey(entry, keycode) {
        const layer = this.layout?.layers?.find((candidate) => candidate.layer === entry.layer);
        const key = layer?.keys?.find((candidate) => candidate.layoutIndex === entry.layoutIndex);
        if (key) {
            key.keycode = keycode;
            key.resolved = keycodeCatalog.resolve(keycode);
        }
    }

// Reads the committed profile off the keyboard and decodes it.
    //
    // This is the read D-026 requires and the first time the app can show RGB
    // and key behaviours as the device actually holds them, rather than as the
    // authored source describes them. A domain that fails to decode is
    // reported as such and leaves the others readable; the alternative is
    // discarding a whole profile because one section drifted.
    async readCommittedProfile() {
        if (!this.connection?.connected) {
            this.setError(new Error("Connect to a keyboard before reading its profile."));
            this.emitChange();
            return this.snapshot();
        }

        return this.runOperation("reading profile", async () => {
            const connection = this.connection;
            this.profileBytes = undefined;
            this.committed = {state: "reading", progress: {done: 0, total: 0}};
            this.emitChange();

            // Decide from a fresh status before requesting payload pages. A
            // rejected committed chunk can mean a read failed mid-transfer;
            // it must not be interpreted as proof that nothing is committed.
            const status = await readProfileStatus(connection, {nextRequestId: () => this.requestIds.next()});
            if (this.connection !== connection || !connection.connected) {
                throw new Error("The keyboard disconnected while its profile status was being read.");
            }
            this.status = status;
            const source = status.activeKind === PROFILE_ACTIVE_KIND.COMPILED_ONLY
                && !(status.stateFlags & PROFILE_STATE_FLAGS.COMMITTED_VALID) ? "compiled" : "committed";
            const read = await (source === "compiled" ? readCompiledPayload : readCommittedPayload)(connection, {
                nextRequestId: () => this.requestIds.next(),
                onProgress: (progress) => {
                    this.committed = {...this.committed, progress};
                    this.emitChange();
                },
            });
            const {metadata, bytes} = read;
            if (this.connection !== connection || !connection.connected) {
                throw new Error("The keyboard disconnected while its profile was being read.");
            }
            const blob = decodeProfileBlob(bytes);
            const domains = {};
            const failures = [];
            for (const domain of blob.domains) {
                try {
                    if (domain.id === PROFILE_DOMAIN_IDS.RGB) {
                        domains.rgb = decodeRgbDomainV1(domain.payload);
                    } else if (domain.id === PROFILE_DOMAIN_IDS.KEY_BEHAVIORS) {
                        domains.keyBehaviors = decodeKeyBehaviorDomain(domain.payload, actionLimitsFor(blob.schema.major));
                    } else if (domain.id === PROFILE_DOMAIN_IDS.PD_MODES) {
                        domains.pdModes = decodePdDomain(domain.payload);
                    } else if (domain.id === PROFILE_DOMAIN_IDS.SETTINGS) {
                        domains.settings = decodeSettings(domain.payload);
                    } else if (domain.id === PROFILE_DOMAIN_IDS.COMBOS) {
                        domains.combos = decodeComboDomain(domain.payload, domain.version, actionLimitsFor(blob.schema.major));
                    }
                } catch (error) {
                    failures.push({domainId: domain.id, message: error instanceof Error ? error.message : String(error)});
                }
            }

            this.committed = {
                state: "read",
                source,
                progress: null,
                readAt: new Date().toISOString(),
                generation: metadata.generation,
                digest: metadata.digest,
                schema: metadata.schema,
                originHalf: metadata.originHalf,
                byteLength: bytes.length,
                domainIds: blob.domains.map((domain) => domain.id),
                domains,
                failures,
            };
            this.profileBytes = Buffer.from(bytes);
            const described = source === "compiled"
                ? "the firmware's compiled defaults"
                : `committed generation ${metadata.generation}`;
            this.addDiagnostic(
                failures.length
                    ? `Read ${described}; ${failures.length} domain${failures.length === 1 ? "" : "s"} failed to decode.`
                    : `Read ${described} from the keyboard (${bytes.length} bytes).`
            );
        });
    }

    async readCombos() {
        const connection = this.connection;
        this.combos = {state: "reading"};
        this.emitChange();
        try {
            if (!connection?.connected) throw new Error("Connect to a keyboard before reading combos.");
            const read = await readDeviceCombos(connection, {nextRequestId: () => this.requestIds.next()});
            if (this.connection !== connection || !connection.connected) return this.snapshot();
            this.combos = {state: "read", ...read, readAt: new Date().toISOString()};
        } catch (error) {
            if (this.connection !== connection) return this.snapshot();
            this.combos = connection?.connected ? {state: "unavailable", error: publicError(error)} : undefined;
        }
        this.emitChange();
        return this.snapshot();
    }

    async readBaseRgb() {
        const connection = this.connection;
        this.baseRgb = {state: "reading"};
        this.emitChange();
        try {
            if (!connection?.connected) throw new Error("Connect to a keyboard before reading base RGB.");
            const settings = await readViaRgbMatrix(connection);
            if (this.connection !== connection || !connection.connected) return this.snapshot();
            this.baseRgb = {state: "read", ...settings, readAt: new Date().toISOString()};
        } catch (error) {
            if (this.connection !== connection) return this.snapshot();
            if (!connection?.connected) {
                this.baseRgb = undefined;
            } else {
                // This optional read must not discard a usable profile or
                // mask an earlier layout/profile error. Clear stale colour.
                this.baseRgb = {state: "unavailable", error: publicError(error)};
                this.addDiagnostic(`Base RGB: ${this.baseRgb.error.message}`);
            }
        }
        this.emitChange();
        return this.snapshot();
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

    async readPortableProfile({forRestore = false} = {}) {
        if (!this.connection?.connected || this.busy) throw new Error("Connect the keyboard and wait for the current operation to finish.");
        let result;
        await this.runOperation("reading complete profile", async () => {
            result = await captureProfile(this.connection, this.requestIds, this.capabilities, message => {
                this.portableProgress = message; this.emitChange();
            }, false, forRestore);
            result.limits = await readSettingsLimits(this.connection, this.requestIds);
            result.options = await readKeyboardOptions(this.connection, this.requestIds);
            this.portable = result;
            this.macroView = macroEditorView(result, this.capabilities);
            this.customKeyView = customKeyEditorView(result, this.capabilities);
            this.settingsView = settingsEditorView(result);
        });
        this.portableProgress = "";
        if (this.error) throw Object.assign(new Error(this.error.message), this.error);
        return result;
    }

    async restorePortableProfile(document, options) {
        if (!this.connection?.connected || this.busy) throw new Error("Connect the keyboard and wait for the current operation to finish.");
        let result, started = false;
        this.liveApply = {state: "idle"};
        await this.runOperation("restoring complete profile", async () => {
            const cachedBase = options?.expectedFingerprint && this.portable?.fingerprint === options.expectedFingerprint
                ? this.portable
                : undefined;
            const limits = cachedBase?.limits || await readSettingsLimits(this.connection, this.requestIds);
            const lighting = baseLighting(validateSnapshot(document, this.capabilities).settings.values);
            const brightness = lighting.brightness;
            if (limits && brightness > limits.brightnessMax) throw Object.assign(new Error(`This profile's brightness exceeds the keyboard's reported limit of ${limits.brightnessMax}. Lower the brightness before restoring it.`), {code: "SETTINGS_LIMIT_EXCEEDED"});
            const keyboardOptions = cachedBase?.options || await readKeyboardOptions(this.connection, this.requestIds);
            const effect = lighting.effect;
            if (keyboardOptions && !keyboardOptions.effects.some(item => item.id === effect)) throw Object.assign(new Error("This profile uses a lighting effect unavailable on this keyboard."), {code: "SETTINGS_LIMIT_EXCEEDED"});
            // The keyboard refuses a profile whose actions sit where it cannot
            // run them; say which one before anything is sent.
            const misplaced = profilePlacementProblem(Buffer.from(document.profile, "base64"), {layerCount: this.capabilities?.compiledLayerCount ?? 8,
                behaviorQmkFunctions: Boolean(this.capabilities?.featureFlags & PROFILE_WIRE_FEATURES.BEHAVIOR_QMK_FUNCTIONS)});
            if (misplaced) throw Object.assign(new Error(`${misplaced} Fix it before saving this profile.`), {code: "PLACEMENT_REFUSED"});
            started = true;
            result = await restoreProfile(this.connection, this.requestIds, this.capabilities, document, {...options,
                baseSnapshot: cachedBase,
                onProgress: message => {this.portableProgress = message;},
                // The step view the commit bar draws; it outlives the apply
                // when it failed, so the person can see where and why.
                onApplyProgress: view => {this.liveApply = view; this.emitChange();},
            });
            result.limits = limits;
            result.options = keyboardOptions;
            this.portable = result;
            this.macroView = macroEditorView(result, this.capabilities);
            this.customKeyView = customKeyEditorView(result, this.capabilities);
            this.settingsView = settingsEditorView(result);
            if (result.performance) {
                const seconds = (result.performance.elapsedMs / 1000).toFixed(1);
                this.addDiagnostic(`Saved the complete profile in ${seconds} s; transferred ${result.performance.layoutBytes} changed layout bytes and ${result.performance.macroBytes} changed macro bytes.`);
            }
        });
        this.portableProgress = "";
        // Once the restore has started — landed, failed, or stopped past the
        // point of no return — the health the panel shows must be the
        // keyboard's now, not the status read before the apply began. A
        // refusal before the first write changed nothing, so it reads nothing.
        if (started) {
            await this.refreshStatus();
            this.emitChange();
        }
        if (this.error) throw Object.assign(new Error(this.error.message), this.error);
        return result;
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
            connectionToken: this.connectionToken,
            capabilities: this.capabilities ? {...this.capabilities} : null,
            status: this.status ? {...this.status} : null,
            candidateStatus: this.candidateStatus ? cloneCandidateStatus(this.candidateStatus) : null,
            viaIdentity: this.viaIdentity ? {...this.viaIdentity} : null,
            compatibility: this.capabilities
                ? evaluateProfileCompatibility(this.capabilities, this.profileSummary, this.viaIdentity)
                : null,
            mutationCompatibility: evaluateLiveMutationCompatibility(
                this.capabilities,
                this.capabilities ? evaluateProfileCompatibility(this.capabilities, this.profileSummary, this.viaIdentity) : null,
                Boolean(this.connection?.connected),
                this.status
            ),
            portableSummary: this.committed?.domains?.settings ? {names: this.committed.domains.settings.names.map((name, index, names) => layerName(names, index))} : this.portable?.summary || null,
            portableProgress: this.portableProgress || "",
            settingsView: this.settingsView ? JSON.parse(JSON.stringify(this.settingsView)) : null,
            macroView: this.macroView ? JSON.parse(JSON.stringify(this.macroView)) : null,
            customKeyView: this.customKeyView ? JSON.parse(JSON.stringify(this.customKeyView)) : null,
            liveApply: cloneLiveApply(this.liveApply),
            layout: this.layout ? JSON.parse(JSON.stringify(this.layout)) : null,
            committed: this.committed ? JSON.parse(JSON.stringify(this.committed)) : null,
            baseRgb: this.baseRgb ? JSON.parse(JSON.stringify(this.baseRgb)) : null,
            combos: this.combos ? JSON.parse(JSON.stringify(this.combos)) : null,
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
        this.portable = undefined; this.portableProgress = "";
        this.macroView = undefined;
        this.customKeyView = undefined;
        this.settingsView = undefined;
        this.profileBytes = undefined;
        this.layout = undefined;
        this.committed = undefined;
        this.baseRgb = undefined;
        this.combos = undefined;
        this.disposeConnectionListener?.();
        this.disposeConnectionListener = undefined;
        this.connection = undefined;
        this.connectionPublicId = "";
        this.connectionToken = null;
        this.capabilities = undefined;
        this.status = undefined;
        this.candidateStatus = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.lastRefreshedAt = "";
        this.liveApply = {state: "idle"};
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

    async refreshStatus() {
        if (!this.connection?.connected || !this.requestIds) return;
        try {
            this.status = await readProfileStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
            this.candidateStatus = await this.readCandidateStatus(this.connection, {
                nextRequestId: () => this.requestIds.next(),
            });
            this.lastRefreshedAt = new Date().toISOString();
        } catch {
            // Preserve the original outcome; Refresh remains available for a later explicit retry.
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
    const result = {
        code: cleanText(error?.code) || cleanText(error?.name) || "LIVE_LINK_ERROR",
        message: cleanText(error?.message) || String(error || "Unknown live-link error."),
    };
    // Where an Apply failed, why, and what was saved (see apply-progress.js).
    for (const key of ["step", "stepLabel", "reason", "saved"]) {
        if (typeof error?.[key] === "string" && error[key]) result[key] = cleanText(error[key]);
    }
    return result;
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
        equalityCheck("Schema major", capabilities?.schema?.major, capabilities?.supportedDomainMask & 16 ? 2 : PROFILE_STUDIO_SCHEMA.major),
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
    return {label, ok, actual, limit: expected, message: ok ? "" : `${label} is ${actual}; Charybdis Live requires ${expected}.`};
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

function evaluateLiveMutationCompatibility(capabilities, compatibility, connected = true, status) {
    const reasons = [];
    if (!connected) reasons.push("Connect to the keyboard before applying a live profile.");
    if (!compatibility?.compatible) reasons.push(...(compatibility?.reasons || ["Read compatibility has not been established."]));
    const flags = Number(capabilities?.featureFlags) || 0;
    if ((flags & REQUIRED_LIVE_MUTATION_FEATURES) !== REQUIRED_LIVE_MUTATION_FEATURES) {
        reasons.push(`Firmware mutation flags 0x${flags.toString(16)} do not include required persistent split apply mask 0x${REQUIRED_LIVE_MUTATION_FEATURES.toString(16)}.`);
    }
    if (!Number.isInteger(capabilities?.candidateChunkMax) || capabilities.candidateChunkMax < 1) {
        reasons.push("Firmware does not advertise a candidate upload chunk size.");
    }
    if (connected && !status) {
        reasons.push("Live profile status has not been read yet.");
    } else if (status && !status.peerKnown) {
        reasons.push("The second keyboard half has not been detected over the split link.");
    } else if (status && !status.peerConverged) {
        reasons.push("The two keyboard halves have not established a converged profile state yet.");
    }
    return {
        available: reasons.length === 0,
        reasons,
        requiredFeatureMask: REQUIRED_LIVE_MUTATION_FEATURES,
        peerReady: Boolean(status?.peerKnown && status?.peerConverged),
        recoveryPending: Boolean(status?.candidatePending),
    };
}

function cloneLiveApply(value) {
    return value && value.state !== "idle" ? JSON.parse(JSON.stringify(value)) : {state: "idle"};
}

function cloneCandidateStatus(status) {
    return {
        ...status,
        error: status?.error ? {...status.error} : null,
        stateName: candidateStateName(status?.state),
    };
}

function candidateStateName(state) {
    return CANDIDATE_STATE_NAMES[state] || `UNKNOWN_${state}`;
}

module.exports = {
    PROFILE_DOMAIN_FLAGS,
    PROFILE_STUDIO_PROTOCOL,
    PROFILE_STUDIO_SCHEMA,
    ProfileRequestIdSequence,
    ProfileDeviceService,
    REQUIRED_PROFILE_DOMAIN_MASK,
    REQUIRED_LIVE_MUTATION_FEATURES,
    evaluateProfileCompatibility,
    evaluateLiveMutationCompatibility,
    normalizeProfileSummary,
};
