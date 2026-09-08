"use strict";
const {readViaStorage, readRegion, writeRegion, writeViaMacros, VIA_STORAGE} = require("../protocol/via-storage-v1");
const {readSettings, readStorageStatus, waitForStorage} = require("../protocol/portable-profile-v1");
const {readProfileStatus, PROFILE_ACTIVE_KIND} = require("../protocol/profile-wire-v1");
const {readCommittedPayload, readCompiledPayload} = require("../protocol/profile-payload-v1");
const {readDeviceCombos} = require("../protocol/combo-readback-v1");
const {candidateMetadataForBlob, readCandidateStatus, CANDIDATE_STATE} = require("../protocol/profile-candidate-v1");
const {CandidateUploadCoordinator} = require("./candidate-upload-coordinator");
const {createSnapshot, validateSnapshot, materializeProfile, fingerprint, summary, reorderLayers} = require("../model/portable-profile");
const {crc32, fnv1a32} = require("../schema/profile-blob-v1");
const fail = (code, message) => Object.assign(new Error(message), {code});
const statusKey = s => JSON.stringify([s.activeKind, s.activeDigest, s.activeGeneration, s.activeOriginHalf, s.committedDigest, s.committedGeneration, s.stateFlags]);
function requireReady(capabilities, writing = false) {
    if ((writing ? capabilities?.compiledLayerCount !== 8 : ![5, 8].includes(capabilities?.compiledLayerCount)) || (capabilities?.supportedDomainMask & 15) !== 15) {
        const guidance = capabilities?.compiledLayerCount === 5 ? "Use the five-layer backup bridge on both halves and export your profile before installing the eight-layer firmware. Import becomes available after that update." : "Complete backups require firmware with complete-profile support on both halves.";
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
            fingerprint: `incomplete:${capabilities.actionAbiDigest}:${crc32(bytes)}:${fnv1a32(bytes)}`, summary: null, incomplete: true, status: after};
    }
    const document = createSnapshot({profile, via, actionAbiDigest: capabilities.actionAbiDigest});
    validateSnapshot(document, capabilities);
    return {document, fingerprint: fingerprint(document), summary: summary(document), status: after};
}
async function restoreProfile(connection, ids, capabilities, document, {expectedFingerprint, saveRecovery, onProgress = () => {}, operations = {}} = {}) {
    requireReady(capabilities);
    requireReady(capabilities, true);
    const target = validateSnapshot(document, capabilities);
    document = target.document;
    const capture = operations.capture || captureProfile;
    const readCandidate = operations.readCandidate || readCandidateStatus;
    const writeMacros = operations.writeMacros || writeViaMacros;
    const writeLayout = operations.writeLayout || ((c, bytes) => writeRegion(c, VIA_STORAGE.LAYOUT_WRITE, bytes));
    const waitStorage = operations.waitStorage || waitForStorage;
    const readStored = operations.readStored || readRegion;
    const createCoordinator = operations.createCoordinator || ((c, options) => new CandidateUploadCoordinator(c, options));
    if (typeof saveRecovery !== "function") throw fail("RECOVERY_REQUIRED", "Save a recovery copy before restoring this keyboard.");
    const before = await capture(connection, ids, capabilities, onProgress, false, true);
    if (expectedFingerprint && before.fingerprint !== expectedFingerprint) throw fail("PROFILE_CHANGED", "The keyboard changed since the restore was reviewed. Review it again.");
    const recovery = await saveRecovery(before.document);
    if (!recovery) throw fail("RECOVERY_REQUIRED", "A recovery copy could not be saved. The keyboard has not been changed.");
    const options = {nextRequestId: () => ids.next()};
    const candidate = await readCandidate(connection, options);
    if (candidate.state !== CANDIDATE_STATE.IDLE) throw fail("KEYBOARD_BUSY", "The keyboard has an unfinished profile transaction. Finish or recover it before restoring.");
    const coordinator = createCoordinator(connection, {chunkSize: capabilities.candidateChunkMax, requestIds: ids, onProgress: progress => onProgress(`Preparing profile: ${progress.phase}`)});
    let mutated = false, prepared;
    try {
        prepared = await coordinator.upload(target.profile, {metadata: candidateMetadataForBlob(target.profile, {actionAbiDigest: capabilities.actionAbiDigest}), verifyBase: async () => {
            const current = await capture(connection, ids, capabilities, onProgress, true, true);
            if (current.fingerprint !== before.fingerprint) throw fail("PROFILE_CHANGED", "The keyboard changed before restore could start.");
        }});
        // VIA and the custom profile have separate durable owners. The recovery
        // file remains usable if either owner is interrupted between these steps.
        onProgress("Saving behaviours, combos, lighting and settings to both halves");
        mutated = true;
        await coordinator.commit(prepared.transactionId, {digest: prepared.metadata.digest});
        onProgress("Restoring macros and all eight layers");
        await writeMacros(connection, target.macros);
        await writeLayout(connection, target.layout);
        await waitStorage(connection, ids);
        const actualLayout = await readStored(connection, VIA_STORAGE.LAYOUT_READ, target.layout.length);
        const actualMacros = await readStored(connection, VIA_STORAGE.MACRO_READ, target.macros.length);
        if (!actualLayout.equals(target.layout) || !actualMacros.equals(target.macros)) throw fail("RESTORE_VERIFY_FAILED", "Layout or macro readback did not match the imported profile.");
        const after = await capture(connection, ids, capabilities, onProgress);
        if (after.fingerprint !== fingerprint(document)) throw fail("RESTORE_VERIFY_FAILED", "The keyboard's complete readback did not match the imported profile.");
        return {...after, recovery};
    } catch (error) {
        if (mutated) throw fail("RESTORE_INCOMPLETE", `Restore was interrupted. Keep both halves connected. ${before.incomplete ? `Import your original complete backup again. Interrupted data was saved for diagnosis at ${recovery}.` : `Import the recovery file ${recovery}.`} ${error.message}`);
        throw error;
    }
}
module.exports = {captureProfile, restoreProfile, validateSnapshot, summary, fingerprint, reorderLayers};
