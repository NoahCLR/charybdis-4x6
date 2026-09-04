"use strict";

const {RAW_HID_REPORT_SIZE} = require("../transport/device-adapter");
const {NodeHidDeviceAdapter} = require("../transport/node-hid-adapter");
const {DeviceRequestCoordinator} = require("../transport/request-coordinator");
const {CandidateUploadCoordinator} = require("./candidate-upload-coordinator");
const {CHARYBDIS_4X6_LAYOUT_MATRIX, readViaKeycode, readViaLayout, synchronizeViaLayout, writeViaKeycode} = require("../protocol/via-layout-v1");
const keycodeCatalog = require("../data/keycode-catalog");
const {readCommittedPayload, readCompiledPayload} = require("../protocol/profile-payload-v1");
const {PROFILE_DOMAIN_IDS, decodeProfileBlob} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1} = require("../schema/rgb-domain-v1");
const {decodeKeyBehaviorDomain} = require("../schema/key-behavior-domain-v1");
const {
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    CANDIDATE_STATE_NAMES,
    candidateMetadataForBlob,
    readCandidateStatus,
} = require("../protocol/profile-candidate-v1");
const {
    PROFILE_WIRE_KNOWN_MASKS,
    PROFILE_WIRE_FEATURES,
    PROFILE_WIRE_V1,
    PROFILE_ACTIVE_KIND,
    VIA_READS,
    readProfileCapabilities,
    readProfileStatus,
    readViaIdentity,
} = require("../protocol/profile-wire-v1");

const PROFILE_STUDIO_PROTOCOL = Object.freeze({major: 1, minor: 0});
const PROFILE_STUDIO_SCHEMA = Object.freeze({major: 1, minor: 0});
const PROFILE_DOMAIN_FLAGS = Object.freeze({RGB: 1 << 0, KEY_BEHAVIORS: 1 << 1});
const REQUIRED_PROFILE_DOMAIN_MASK = PROFILE_DOMAIN_FLAGS.RGB | PROFILE_DOMAIN_FLAGS.KEY_BEHAVIORS;
const REQUIRED_LIVE_MUTATION_FEATURES = PROFILE_WIRE_FEATURES.CANDIDATE_WRITE
    | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT
    | PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION
    | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION;

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
        this.createCandidateUploadCoordinator = typeof options.createCandidateUploadCoordinator === "function"
            ? options.createCandidateUploadCoordinator
            : (connection, coordinatorOptions) => new CandidateUploadCoordinator(connection, coordinatorOptions);
        this.readCandidateStatus = typeof options.readCandidateStatus === "function"
            ? options.readCandidateStatus
            : readCandidateStatus;
        this.synchronizeViaLayout = typeof options.synchronizeViaLayout === "function"
            ? options.synchronizeViaLayout
            : synchronizeViaLayout;
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
        this.candidateStatus = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.error = undefined;
        this.diagnostics = [];
        this.lastRefreshedAt = "";
        this.liveApply = {state: "idle", progress: null, result: null, error: null};
        this.layout = undefined;
        this.committed = undefined;
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
    //
    // A keycode expression the vendored catalog cannot encode is refused, not
    // approximated: writing a guessed value would silently change what the
    // key does. Refusals are returned so the caller can report them.
    async writeLayoutKeys(groups) {
        if (!this.connection?.connected) {
            throw new Error("Connect to a keyboard before changing its layout.");
        }

        const planned = [];
        const rejected = [];
        for (const group of Array.isArray(groups) ? groups : []) {
            const layer = this.layerIndexFor(group.layer);
            if (layer === undefined) {
                for (const change of group.changes || []) {
                    rejected.push({...change, reason: `unknown layer ${group.layer}`});
                }
                continue;
            }
            for (const change of group.changes || []) {
                const position = CHARYBDIS_4X6_LAYOUT_MATRIX[change.layoutIndex];
                const keycode = keycodeCatalog.encode(change.keycode);
                if (!position || keycode === undefined) {
                    rejected.push({...change, reason: position ? "keycode not in the vendored catalog" : "position outside the layout"});
                    continue;
                }
                planned.push({layer, row: position[0], column: position[1], keycode, layoutIndex: change.layoutIndex, expression: change.keycode});
            }
        }

        // runOperation reports through the snapshot and swallows the error
        // into error state, so count outside it. A partial count after a
        // failure is the truth: those keys really were written.
        const outcome = {written: 0, rejected};
        await this.runOperation("writing layout", async () => {
            const connection = this.connection;
            for (const entry of planned) {
                await writeViaKeycode(connection, entry);
                const readBack = await readViaKeycode(connection, entry);
                if (readBack !== entry.keycode) {
                    throw new Error(
                        `The keyboard reported 0x${readBack.toString(16)} after writing ${entry.expression}; layout write aborted.`
                    );
                }
                this.applyLayoutKey(entry, readBack);
                outcome.written += 1;
            }
            this.addDiagnostic(
                rejected.length
                    ? `Wrote ${outcome.written} keys and refused ${rejected.length}.`
                    : `Wrote ${outcome.written} keys to the keyboard.`
            );
        });
        return outcome;
    }

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
            this.committed = {state: "reading", progress: {done: 0, total: 0}};
            this.emitChange();

            let read;
            let source = "committed";
            try {
                read = await readCommittedPayload(connection, {
                    nextRequestId: () => this.requestIds.next(),
                    onProgress: (progress) => {
                        this.committed = {...this.committed, progress};
                        this.emitChange();
                    },
                });
            } catch (error) {
                if (error?.code !== "DEVICE_REJECTED") {
                    throw error;
                }
                // Nothing committed. The keyboard is still running something —
                // the defaults it was compiled with — so read those instead of
                // showing an empty editor and calling it device state.
                read = await readCompiledPayload(connection, {
                    nextRequestId: () => this.requestIds.next(),
                    onProgress: (progress) => {
                        this.committed = {...this.committed, progress};
                        this.emitChange();
                    },
                });
                source = "compiled";
            }
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
                        domains.keyBehaviors = decodeKeyBehaviorDomain(domain.payload);
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

    async applyLiveProfile(value, options = {}) {
        const blob = copyBytes(value, "Live profile blob");
        const layoutEntries = options.layoutEntries === undefined
            ? null
            : copyLayoutEntries(options.layoutEntries);
        const compatibility = this.capabilities
            ? evaluateProfileCompatibility(this.capabilities, this.profileSummary, this.viaIdentity)
            : null;
        const mutation = evaluateLiveMutationCompatibility(
            this.capabilities,
            compatibility,
            Boolean(this.connection?.connected),
            this.status
        );
        if (!mutation.available) {
            this.setError(new Error(mutation.reasons[0] || "This keyboard is not ready for persistent live apply."));
            this.emitChange();
            return this.snapshot();
        }
        if (blob.length > this.capabilities.maxProfilePayload) {
            this.setError(new Error(`Compiled live profile is ${blob.length} bytes; firmware accepts at most ${this.capabilities.maxProfilePayload}.`));
            this.emitChange();
            return this.snapshot();
        }

        let metadata;
        try {
            metadata = candidateMetadataForBlob(blob, {
                actionAbiDigest: this.capabilities.actionAbiDigest,
                requestedDomains: REQUIRED_PROFILE_DOMAIN_MASK,
            });
        } catch (error) {
            this.setError(error);
            this.liveApply = {...this.liveApply, state: "failed", error: publicError(error)};
            this.emitChange();
            return this.snapshot();
        }

        return this.runOperation("applying-live", async () => {
            this.liveApply = {state: "uploading", progress: null, result: null, error: null};
            const coordinator = this.createCandidateUploadCoordinator(this.connection, {
                chunkSize: this.capabilities.candidateChunkMax,
                requestIds: this.requestIds,
                onProgress: (progress) => {
                    this.liveApply = {...this.liveApply, state: progress.phase, progress: {...progress}};
                    this.emitChange();
                },
            });
            try {
                this.status = await readProfileStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
                const currentMutation = evaluateLiveMutationCompatibility(
                    this.capabilities,
                    compatibility,
                    Boolean(this.connection?.connected),
                    this.status
                );
                if (!currentMutation.available) {
                    throw liveApplyError("LIVE_APPLY_PEER_NOT_READY", currentMutation.reasons[0] || "The split keyboard is not ready for live apply.");
                }

                this.candidateStatus = await this.readCandidateStatus(this.connection, {
                    nextRequestId: () => this.requestIds.next(),
                });
                let prepared;
                if (candidateCanResumeCommit(this.candidateStatus, metadata.digest)) {
                    prepared = {transactionId: this.candidateStatus.transactionId, metadata};
                    this.liveApply = {
                        ...this.liveApply,
                        state: "resuming-commit",
                        result: {transactionId: prepared.transactionId, digest: metadata.digest},
                    };
                    this.addDiagnostic(
                        `Resuming matching candidate transaction ${prepared.transactionId} from ${candidateStateName(this.candidateStatus.state)}.`
                    );
                    this.emitChange();
                } else if (this.candidateStatus.state === CANDIDATE_STATE.IDLE) {
                    prepared = await coordinator.upload(blob, {metadata});
                } else {
                    throw activeCandidateError(this.candidateStatus, metadata.digest);
                }
                this.liveApply = {...this.liveApply, state: "committing", result: {transactionId: prepared.transactionId, digest: prepared.metadata.digest}};
                this.emitChange();
                const committed = await coordinator.commit(prepared.transactionId, {digest: prepared.metadata.digest});
                this.status = await readProfileStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
                this.candidateStatus = committed.status || await this.readCandidateStatus(this.connection, {
                    nextRequestId: () => this.requestIds.next(),
                });
                assertAppliedStatus(this.status, prepared.metadata.digest);
                let layoutResult = null;
                if (layoutEntries) {
                    this.liveApply = {
                        ...this.liveApply,
                        state: "reading-layout",
                        progress: {phase: "reading-layout", completed: 0, total: layoutEntries.length, changed: 0},
                    };
                    this.emitChange();
                    layoutResult = await this.synchronizeViaLayout(this.connection, layoutEntries, {
                        onProgress: (progress) => {
                            this.liveApply = {...this.liveApply, state: progress.phase, progress: {...progress}};
                            this.emitChange();
                        },
                    });
                }
                this.lastRefreshedAt = new Date().toISOString();
                this.liveApply = {
                    state: "complete",
                    progress: {...committed.progress},
                    result: {
                        transactionId: prepared.transactionId,
                        digest: prepared.metadata.digest,
                        byteLength: blob.length,
                        ...(layoutResult ? {layout: {...layoutResult}} : {}),
                    },
                    error: null,
                };
                this.addDiagnostic(`Applied and persisted live profile ${hexDigest(prepared.metadata.digest)} (${blob.length} bytes).`);
                if (layoutResult) {
                    this.addDiagnostic(
                        `Verified ${layoutResult.checkedKeys} VIA layout keys; changed ${layoutResult.changedKeys} on the connected half and queued split persistence.`
                    );
                }
            } catch (error) {
                this.liveApply = {...this.liveApply, state: "failed", error: publicError(error)};
                await this.refreshStatusAfterFailure();
                throw error;
            }
        });
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
            liveApply: cloneLiveApply(this.liveApply),
            layout: this.layout ? JSON.parse(JSON.stringify(this.layout)) : null,
            committed: this.committed ? JSON.parse(JSON.stringify(this.committed)) : null,
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
        this.layout = undefined;
        this.committed = undefined;
        this.disposeConnectionListener?.();
        this.disposeConnectionListener = undefined;
        this.connection = undefined;
        this.connectionPublicId = "";
        this.capabilities = undefined;
        this.status = undefined;
        this.candidateStatus = undefined;
        this.viaIdentity = undefined;
        this.requestIds = undefined;
        this.lastRefreshedAt = "";
        this.liveApply = {state: "idle", progress: null, result: null, error: null};
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

    async refreshStatusAfterFailure() {
        if (!this.connection?.connected || !this.requestIds) return;
        try {
            this.status = await readProfileStatus(this.connection, {nextRequestId: () => this.requestIds.next()});
            this.candidateStatus = await this.readCandidateStatus(this.connection, {
                nextRequestId: () => this.requestIds.next(),
            });
            this.lastRefreshedAt = new Date().toISOString();
        } catch {
            // Preserve the original mutation failure; Refresh remains available for a later explicit retry.
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

function copyBytes(value, label) {
    if (!(value instanceof Uint8Array)) throw new TypeError(`${label} must be a Buffer or Uint8Array.`);
    return Buffer.from(value);
}

function copyLayoutEntries(entries) {
    if (!Array.isArray(entries)) throw new TypeError("Live layout entries must be an array.");
    return entries.map((entry) => ({...entry}));
}

function cloneLiveApply(value) {
    return {
        state: value?.state || "idle",
        progress: value?.progress ? {...value.progress} : null,
        result: value?.result ? {...value.result} : null,
        error: value?.error ? {...value.error} : null,
    };
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

function candidateCanResumeCommit(status, digest) {
    if (!status || (Number(status.digest) >>> 0) !== (Number(digest) >>> 0) || !status.transactionId) return false;
    return [
        CANDIDATE_STATE.VALIDATED,
        CANDIDATE_STATE.PREPARING_PEER,
        CANDIDATE_STATE.COMMITTING,
        CANDIDATE_STATE.CONVERGING_PEER,
        CANDIDATE_STATE.ACTIVATING,
    ].includes(status.state)
        || (status.state === CANDIDATE_STATE.IDLE && status.lastOperation === CANDIDATE_OPERATION.COMMIT);
}

function activeCandidateError(status, requestedDigest) {
    const stateName = candidateStateName(status?.state);
    const sameDigest = (Number(status?.digest) >>> 0) === (Number(requestedDigest) >>> 0);
    const reason = sameDigest
        ? `Its ${stateName} state cannot be resumed safely by Profile Studio.`
        : `Its digest ${hexDigest(status?.digest)} does not match the current source digest ${hexDigest(requestedDigest)}.`;
    const error = liveApplyError(
        "ACTIVE_CANDIDATE",
        `Firmware already has candidate transaction ${status?.transactionId || 0} in ${stateName}. `
            + reason + " "
            + "Power-cycle both halves together to discard this pre-commit candidate, then reconnect and apply again."
    );
    error.status = cloneCandidateStatus(status);
    return error;
}

function liveApplyError(code, message) {
    const error = new Error(message);
    error.code = code;
    return error;
}

function hexDigest(value) {
    return `0x${(Number(value) >>> 0).toString(16).padStart(8, "0")}`;
}

function assertAppliedStatus(status, digest) {
    const expected = Number(digest) >>> 0;
    if (status?.activeKind !== PROFILE_ACTIVE_KIND.COMMITTED
        || (Number(status?.activeDigest) >>> 0) !== expected
        || (Number(status?.committedDigest) >>> 0) !== expected) {
        const error = new Error(`Firmware completed the candidate transaction, but its active and committed status did not confirm ${hexDigest(expected)}.`);
        error.code = "LIVE_APPLY_VERIFICATION_FAILED";
        throw error;
    }
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
