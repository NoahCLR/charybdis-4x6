"use strict";
const {readViaStorage, readRegion, changedRanges, viaStorageDigest, VIA_STORAGE} = require("../protocol/via-storage-v1");
const {readSettings, readStorageStatus, waitForStorage} = require("../protocol/portable-profile-v1");
const {readProfileStatus, PROFILE_ACTIVE_KIND} = require("../protocol/profile-wire-v1");
const {readCommittedPayload, readCompiledPayload} = require("../protocol/profile-payload-v1");
const {readDeviceCombos} = require("../protocol/combo-readback-v1");
const {candidateMetadataForBlob, readCandidateStatus, CANDIDATE_STATE} = require("../protocol/profile-candidate-v1");
const {CandidateUploadCoordinator} = require("./candidate-upload-coordinator");
const {LogicalViaStageCoordinator} = require("./logical-via-stage-coordinator");
const {createSnapshot, validateSnapshot, materializeProfile, fingerprint, summary, reorderLayers} = require("../model/portable-profile");
const {encodeSettings} = require("../schema/settings-domain-v1");
const {crc32, fnv1a32} = require("../schema/profile-blob-v1");
const fail = (code, message) => Object.assign(new Error(message), {code});
const statusKey = s => JSON.stringify([s.activeKind, s.activeDigest, s.activeGeneration, s.activeOriginHalf, s.committedDigest, s.committedGeneration, s.stateFlags]);
const identityStatusKey = s => JSON.stringify([s.activeKind, s.activeDigest, s.activeGeneration, s.activeOriginHalf, s.committedDigest, s.committedGeneration, s.stateFlags & ~4]);
const identityKey = identity => JSON.stringify(identity);
const snapshotIdentity = (profile, storage, settings) => ({profile: identityStatusKey(profile), storageGeneration: storage.generation, storageDigest: storage.digest, settingsCrc: crc32(settings), settingsDigest: fnv1a32(settings)});
function capturedBase(before, capabilities) {
    if (!before.incomplete) return validateSnapshot(before.document, capabilities);
    const decode = field => Buffer.from(before.document[field], "base64");
    const base = {profile: decode("profile"), layout: decode("layout"), macros: decode("macros")};
    if (base.layout.length !== 960 || base.macros.length !== capabilities.viaMacroBytes || base.profile.length < 1) throw fail("INVALID_RECOVERY_CAPTURE", "The interrupted recovery capture is malformed.");
    return base;
}
function requireReady(capabilities, writing = false) {
    if ((writing ? capabilities?.compiledLayerCount !== 8 : ![5, 8].includes(capabilities?.compiledLayerCount)) || (capabilities?.supportedDomainMask & 15) !== 15 || (writing && !(capabilities?.featureFlags & (1 << 12)))) {
        const guidance = capabilities?.compiledLayerCount === 5 ? "Use the five-layer backup bridge on both halves and export your profile before installing the eight-layer firmware. Import becomes available after that update." : "Complete profile Apply requires firmware with atomic profile support on both halves.";
        throw fail("FIRMWARE_UPDATE_REQUIRED", guidance + " Your current keyboard configuration has not been changed.");
    }
}
async function captureProfile(connection, ids, capabilities, onProgress = () => {}, allowCandidate = false, allowIncomplete = false) {
    requireReady(capabilities);
    const options = {nextRequestId: () => ids.next()};
    onProgress("Reading the complete keyboard configuration");
    const storageBefore = await waitForStorage(connection, ids), before = await readProfileStatus(connection, options);
    if (![PROFILE_ACTIVE_KIND.COMMITTED, PROFILE_ACTIVE_KIND.COMPILED_ONLY].includes(before.activeKind) || !(before.stateFlags & 32) || (before.stateFlags & (allowCandidate ? 8 : 12)) || before.conflictCount) throw fail("KEYBOARD_NOT_READY", "Let both halves finish saving before taking a backup.");
    const defaults = await readCompiledPayload(connection, options);
    const active = before.activeKind === PROFILE_ACTIVE_KIND.COMMITTED ? await readCommittedPayload(connection, options) : defaults;
    const combos = await readDeviceCombos(connection, options), settings = await readSettings(connection, ids);
    const via = await readViaStorage(connection, {allowIncomplete});
    if (!settings.equals(await readSettings(connection, ids))) throw fail("PROFILE_CHANGED", "Keyboard settings changed during the backup. Read it again before continuing.");
    const profile = materializeProfile(active.bytes, defaults.bytes, combos, settings);
    const after = await readProfileStatus(connection, options), storageAfter = await readStorageStatus(connection, ids);
    if (statusKey(before) !== statusKey(after) || !storageAfter.ready || storageBefore.generation !== storageAfter.generation || storageBefore.digest !== storageAfter.digest) throw fail("PROFILE_CHANGED", "The keyboard changed during the backup. Read it again before continuing.");
    if (allowIncomplete && via.macros.at(-1) !== 0) {
        // Preserve the interrupted bytes for diagnosis without presenting them
        // as an importable profile. A valid chosen backup can replace them.
        const bytes = Buffer.concat([profile, via.layout, via.macros]);
        return {document: {format: "charybdis-recovery-capture", version: 1, keyboard: "charybdis-4x6", actionAbiDigest: capabilities.actionAbiDigest,
            profile: profile.toString("base64"), layout: via.layout.toString("base64"), macros: via.macros.toString("base64")},
            fingerprint: `incomplete:${capabilities.actionAbiDigest}:${crc32(bytes)}:${fnv1a32(bytes)}`, summary: null, incomplete: true, status: after,
            identity: snapshotIdentity(after, storageAfter, settings)};
    }
    const document = createSnapshot({profile, via, actionAbiDigest: capabilities.actionAbiDigest});
    validateSnapshot(document, capabilities);
    return {document, fingerprint: fingerprint(document), summary: summary(document), status: after, identity: snapshotIdentity(after, storageAfter, settings)};
}
async function readIdentity(connection, ids, {allowCandidate = true} = {}) {
    const options = {nextRequestId: () => ids.next()};
    const storageBefore = await waitForStorage(connection, ids), before = await readProfileStatus(connection, options);
    if ((before.stateFlags & (allowCandidate ? 8 : 12)) || before.conflictCount) throw fail("KEYBOARD_NOT_READY", "The keyboard changed before the save could start.");
    const settings = await readSettings(connection, ids);
    const after = await readProfileStatus(connection, options), storageAfter = await readStorageStatus(connection, ids);
    if (statusKey(before) !== statusKey(after) || !storageAfter.ready || storageBefore.generation !== storageAfter.generation || storageBefore.digest !== storageAfter.digest) throw fail("PROFILE_CHANGED", "The keyboard changed before the save could start.");
    return snapshotIdentity(after, storageAfter, settings);
}
async function verifyRanges(connection, readStored, command, target, ranges, {verifyFinalByte = false} = {}) {
    for (const range of ranges) {
        const actual = await readStored(connection, command, range.bytes.length, {startOffset: range.offset});
        if (!actual.equals(range.bytes)) throw fail("RESTORE_VERIFY_FAILED", "Layout or macro readback did not match the imported profile.");
    }
    if (verifyFinalByte) {
        const actual = await readStored(connection, command, 1, {startOffset: target.length - 1});
        if (actual[0] !== target.at(-1)) throw fail("RESTORE_VERIFY_FAILED", "Layout or macro readback did not match the imported profile.");
    }
}
async function restoreProfile(connection, ids, capabilities, document, {expectedFingerprint, saveRecovery, baseSnapshot, onProgress = () => {}, operations = {}} = {}) {
    const startedAt = Date.now();
    requireReady(capabilities);
    requireReady(capabilities, true);
    const target = validateSnapshot(document, capabilities);
    document = target.document;
    const capture = operations.capture || captureProfile;
    const currentIdentity = operations.readIdentity || readIdentity;
    const readCandidate = operations.readCandidate || readCandidateStatus;
    const waitStorage = operations.waitStorage || waitForStorage;
    const readStorage = operations.readStorage || readStorageStatus;
    const readStored = operations.readStored || readRegion;
    const readProfile = operations.readProfile || readProfileStatus;
    const createCoordinator = operations.createCoordinator || ((c, options) => new CandidateUploadCoordinator(c, options));
    const createViaCoordinator = operations.createViaCoordinator || ((c, options) => new LogicalViaStageCoordinator(c, options));
    if (typeof saveRecovery !== "function") throw fail("RECOVERY_REQUIRED", "Save a recovery copy before restoring this keyboard.");
    let before;
    if (baseSnapshot?.document && baseSnapshot.identity && (!expectedFingerprint || baseSnapshot.fingerprint === expectedFingerprint)) {
        if (!baseSnapshot.incomplete) validateSnapshot(baseSnapshot.document, capabilities);
        onProgress("Verifying the current keyboard configuration");
        const liveIdentity = await currentIdentity(connection, ids, {allowCandidate: false});
        if (identityKey(liveIdentity) !== identityKey(baseSnapshot.identity)) throw fail("PROFILE_CHANGED", "The keyboard changed since the restore was reviewed. Review it again.");
        before = baseSnapshot;
    } else {
        before = await capture(connection, ids, capabilities, onProgress, false, true);
    }
    if (expectedFingerprint && before.fingerprint !== expectedFingerprint) throw fail("PROFILE_CHANGED", "The keyboard changed since the restore was reviewed. Review it again.");
    const recovery = await saveRecovery(before.document);
    if (!recovery) throw fail("RECOVERY_REQUIRED", "A recovery copy could not be saved. The keyboard has not been changed.");
    const options = {nextRequestId: () => ids.next()};
    const base = capturedBase(before, capabilities);
    const beforeIdentity = before.identity;
    if (!beforeIdentity) throw fail("PROFILE_IDENTITY_UNAVAILABLE", "The keyboard read did not include a stable save identity.");
    const candidate = await readCandidate(connection, options);
    if (candidate.state !== CANDIDATE_STATE.IDLE) throw fail("KEYBOARD_BUSY", "The keyboard has an unfinished profile transaction. Finish or recover it before restoring.");
    const coordinator = createCoordinator(connection, {chunkSize: capabilities.candidateChunkMax, requestIds: ids, onProgress: progress => onProgress(`Preparing profile: ${progress.phase}`)});
    const viaCoordinator = createViaCoordinator(connection, {requestIds: ids, onProgress: progress => onProgress(`Preparing keyboard storage: ${progress.completed} / ${progress.total} bytes`)});
    const expectedStorageDigest = viaStorageDigest(target);
    if (beforeIdentity.storageGeneration >= 0xffffffff) throw fail("STORAGE_GENERATION_EXHAUSTED", "The keyboard storage generation cannot advance safely.");
    const targetStorageGeneration = beforeIdentity.storageGeneration + 1;
    let mutated = false, prepared;
    try {
        prepared = await coordinator.upload(target.profile, {metadata: candidateMetadataForBlob(target.profile, {actionAbiDigest: capabilities.actionAbiDigest, viaGeneration: targetStorageGeneration, viaDigest: expectedStorageDigest}), verifyBase: async () => {
            const identity = await currentIdentity(connection, ids, {allowCandidate: true});
            if (identityKey(identity) !== identityKey(beforeIdentity)) throw fail("PROFILE_CHANGED", "The keyboard changed before restore could start.");
        }});
        mutated = true;
        onProgress("Staging changed keys and macros on the other half");
        await viaCoordinator.stage({transactionId: prepared.transactionId, generation: targetStorageGeneration, digest: expectedStorageDigest, target, current: base});
        onProgress("Publishing one complete profile to both halves");
        await coordinator.commit(prepared.transactionId, {digest: prepared.metadata.digest});
        const macroRanges = changedRanges(base.macros, target.macros, {end: target.macros.length - 1});
        const layoutRanges = changedRanges(base.layout, target.layout);
        const macroBytes = macroRanges.reduce((sum, range) => sum + range.bytes.length, 0);
        const layoutBytes = layoutRanges.reduce((sum, range) => sum + range.bytes.length, 0);
        const macroWriteNeeded = macroRanges.length > 0 || base.macros.at(-1) !== target.macros.at(-1);
        const storage = await waitStorage(connection, ids);
        await verifyRanges(connection, readStored, VIA_STORAGE.LAYOUT_READ, target.layout, layoutRanges);
        await verifyRanges(connection, readStored, VIA_STORAGE.MACRO_READ, target.macros, macroRanges, {verifyFinalByte: macroRanges.length > 0});
        const status = await readProfile(connection, options);
        const storageAfter = await readStorage(connection, ids);
        if (status.activeKind !== PROFILE_ACTIVE_KIND.COMMITTED || status.activeDigest !== fnv1a32(target.profile) || status.committedDigest !== fnv1a32(target.profile) || !(status.stateFlags & 32) || status.conflictCount || !storage.ready || !storageAfter.ready || storage.generation !== targetStorageGeneration || storageAfter.generation !== targetStorageGeneration || storage.generation !== storageAfter.generation || storage.digest !== storageAfter.digest || storageAfter.digest !== expectedStorageDigest) throw fail("RESTORE_VERIFY_FAILED", "The keyboard did not confirm the imported profile on both halves.");
        const resultFingerprint = fingerprint(document);
        return {document, fingerprint: resultFingerprint, summary: summary(document), status, identity: snapshotIdentity(status, storageAfter, encodeSettings(target.settings)), recovery,
            performance: {elapsedMs: Date.now() - startedAt, baseSource: before === baseSnapshot ? "verified-cache" : "device-read", layoutBytes, macroBytes, viaConfigReports: 1, layoutReports: layoutRanges.reduce((sum, range) => sum + Math.ceil(range.bytes.length / 12), 0), macroReports: macroWriteNeeded ? macroRanges.reduce((sum, range) => sum + Math.ceil(range.bytes.length / 12), 0) : 0}};
    } catch (error) {
        if (prepared) {
            try { await viaCoordinator.abort({transactionId: prepared.transactionId, generation: targetStorageGeneration, digest: expectedStorageDigest}); } catch {}
            try { await coordinator.abort(prepared.transactionId); } catch {}
        }
        if (mutated) throw fail("RESTORE_INCOMPLETE", `Restore was interrupted. Keep both halves connected. ${before.incomplete ? `Import your original complete backup again. Interrupted data was saved for diagnosis at ${recovery}.` : `Import the recovery file ${recovery}.`} ${error.message}`);
        throw error;
    }
}
module.exports = {captureProfile, readIdentity, restoreProfile, validateSnapshot, summary, fingerprint, reorderLayers};
