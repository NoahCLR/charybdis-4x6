"use strict";
const {readViaStorage, readRegion, writeRegion, writeViaMacros, changedRanges, viaStorageDigest, VIA_STORAGE} = require("../protocol/via-storage-v1");
const {readLegacyPdSource, readSettings, readStorageStatus, waitForStorage} = require("../protocol/portable-profile-v1");
const {readProfileStatus, PROFILE_ACTIVE_KIND, PROFILE_STATE_FLAGS} = require("../protocol/profile-wire-v1");
const {readCommittedPayload, readCompiledPayload} = require("../protocol/profile-payload-v1");
const {readDeviceCombos} = require("../protocol/combo-readback-v1");
const {candidateMetadataForBlob, readCandidatePeerStatus, readCandidateStatus, CANDIDATE_STATE} = require("../protocol/profile-candidate-v1");
const {ApplyProgress, failureReason} = require("./apply-progress");
const {CandidateUploadCoordinator} = require("./candidate-upload-coordinator");
const {LogicalViaStageCoordinator} = require("./logical-via-stage-coordinator");
const {upgradePdSnapshot, createSnapshot, validateSnapshot, materializeProfile, fingerprint, summary, reorderLayers} = require("../model/portable-profile");
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
    const pdSource = capabilities.featureFlags & (1 << 13) ? await readLegacyPdSource(connection, ids) : null;
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
    if (pdSource) document.pdModeSource = {version: 1, actionAbiDigest: capabilities.actionAbiDigest, compiledDefaultDigest: capabilities.compiledDefaultDigest, domain: pdSource.toString("base64")};
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

async function rollForwardLocalStorage(connection, target, base, layoutRanges, macroRanges, onProgress) {
    const layoutBytes = layoutRanges.reduce((sum, range) => sum + range.bytes.length, 0);
    const macroBytes = macroRanges.reduce((sum, range) => sum + range.bytes.length, 0);
    const total = layoutBytes + macroBytes || 2;
    let completed = 0;
    for (const range of layoutRanges) {
        await writeRegion(connection, VIA_STORAGE.LAYOUT_WRITE, range.bytes, {
            startOffset: range.offset,
            onProgress: progress => onProgress({completed: completed + progress.completed, total}),
        });
        completed += range.bytes.length;
    }
    if (macroRanges.length) {
        await writeViaMacros(connection, target.macros, {
            current: base.macros,
            onProgress: progress => onProgress({completed: completed + progress.completed, total}),
        });
        completed += macroBytes;
    }
    if (!layoutRanges.length && !macroRanges.length) {
        // A custom-only change still advances the bound VIA generation. A
        // verified no-op keycode write gives QMK's normal mutation tracker the
        // durable generation transition without changing the layout.
        await writeRegion(connection, VIA_STORAGE.LAYOUT_WRITE, target.layout.subarray(0, 2), {
            onProgress: progress => onProgress({completed: progress.completed, total}),
        });
    }
}
// A peer that never acknowledged a cancelled save keeps its lease until it
// restarts, and the keyboard refuses new saves until then. Best effort: the
// save has already failed, so an unreadable status only loses the hint.
async function peerRestartGuidance(connection, readProfile, options) {
    try {
        const status = await readProfile(connection, options);
        if (status?.peerCleanupPending) return " The other half did not confirm the cancel. Unplug the USB cable (not the cable between the halves), wait a few seconds and plug it back in before saving again.";
    } catch {}
    return "";
}
// What the keyboard is doing for the "Copy the profile to the other half" step,
// from candidate status page 1.
const PEER_DETAIL = Object.freeze({
    BINDING: "Binding keys and macros to the new profile",
    BEGINNING: "Starting the copy",
    SENDING: "Sending",
    PREPARING: "The other half is checking and storing its copy",
    PREPARED: "The other half is ready",
    COMMITTING: "The other half is saving",
    ABORTING: "Cancelling the copy on the other half",
    WAITING: "Waiting for the link between the halves",
});
// What the other half says it is waiting on, once it has said BUSY a few
// times in a row; a single BUSY is the routine first answer to a request.
const PEER_BUSY_DETAIL = Object.freeze({
    MAILBOX_FULL: "the other half has not handled the last request yet",
    OTHER_COPY: "the other half still holds an earlier copy",
    NO_LEASE: "the other half dropped this copy; starting it again",
    STORE_WORKING: "the other half is still storing",
    PULLING: "the other half is fetching a profile itself",
    CONVERGENCE_ONLY: "the other half is finishing another save",
});
function peerReport(peer) {
    if (!peer) return {detail: "Waiting for the other half"};
    if (peer.waitingSafeBoundary) return {detail: "Release held keys and turn off locked layers or pointer modes to finish saving"};
    const busy = peer.busyStreak >= 3 ? ` · ${PEER_BUSY_DETAIL[peer.busyReason] || "the other half is busy"}` : "";
    const report = {detail: `${PEER_DETAIL[peer.phaseName] || "Waiting for the other half"}${busy}`};
    if (peer.phaseName === "SENDING" || peer.phaseName === "BEGINNING") Object.assign(report, {completed: peer.transferOffset, total: peer.transferLength});
    return report;
}
// The coordinators report phases; each lands on the step it belongs to.
function coordinatorReport(progress, applyProgress) {
    const state = progress.status?.state;
    if (["preflight", "begin", "writing"].includes(progress.phase)) applyProgress.report("upload", {completed: progress.bytesSent, total: progress.totalBytes});
    else if (["validate", "validating", "complete"].includes(progress.phase) && !progress.operation) applyProgress.report("validate");
    else if (state === CANDIDATE_STATE.PREPARING_PEER) applyProgress.report("peer", peerReport(progress.peer));
    else if (state === CANDIDATE_STATE.COMMITTING) applyProgress.report("commit", {detail: "Writing the profile"});
    else if (state === CANDIDATE_STATE.CONVERGING_PEER) applyProgress.report("converge");
    else if (state === CANDIDATE_STATE.ACTIVATING) applyProgress.report("verify", {detail: "Switching to the new profile"});
}
// The step view as one line, for the Profile screen and notifications.
function progressText(view) {
    if (view.failure) return `Failed at: ${view.failure.label}`;
    const step = view.steps.find(item => item.id === view.current);
    const counted = view.bytes ? ` ${view.bytes.completed} / ${view.bytes.total} bytes` : "";
    return `${step?.label || ""}${view.detail ? ` · ${view.detail}` : ""}${counted}`;
}

// Every failure leaves with the step it happened in, the keyboard's reason,
// and what can honestly be said about what was saved.
function withFailure(error, applyProgress, saved) {
    const cause = error.cause || error;
    const failure = applyProgress.fail({reason: failureReason(cause, cause.progress?.peer), saved});
    return Object.assign(error, {step: failure.step, stepLabel: failure.label, reason: failure.reason, saved: failure.saved});
}

async function restoreProfile(connection, ids, capabilities, document, {expectedFingerprint, saveRecovery, baseSnapshot, onProgress = () => {}, onApplyProgress = () => {}, operations = {}} = {}) {
    const applyProgress = new ApplyProgress(view => {
        onApplyProgress(view);
        onProgress(progressText(view));
    });
    applyProgress.report("check");
    let mutated = false;
    try {
        return await restoreSteps();
    } catch (error) {
        // Nothing reached the keyboard before the upload; after it, the
        // inner handler has already said whether anything was saved.
        throw withFailure(error, applyProgress, !mutated || error.code === "RESTORE_NOT_SAVED" ? "none" : "unknown");
    }

    async function restoreSteps() {
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
        const rollForwardLocal = operations.rollForwardLocal || rollForwardLocalStorage;
        if (typeof saveRecovery !== "function") throw fail("RECOVERY_REQUIRED", "Save a recovery copy before restoring this keyboard.");
        let before;
        if (baseSnapshot?.document && baseSnapshot.identity && (!expectedFingerprint || baseSnapshot.fingerprint === expectedFingerprint)) {
            if (!baseSnapshot.incomplete) validateSnapshot(baseSnapshot.document, capabilities);
            applyProgress.report("check", {detail: "Comparing with the reviewed profile"});
            const liveIdentity = await currentIdentity(connection, ids, {allowCandidate: false});
            if (identityKey(liveIdentity) !== identityKey(baseSnapshot.identity)) throw fail("PROFILE_CHANGED", "The keyboard changed since the restore was reviewed. Review it again.");
            before = baseSnapshot;
        } else {
            before = await capture(connection, ids, capabilities, message => applyProgress.report("check", {detail: message}), false, true);
        }
        if (expectedFingerprint && before.fingerprint !== expectedFingerprint) throw fail("PROFILE_CHANGED", "The keyboard changed since the restore was reviewed. Review it again.");
        applyProgress.report("backup");
        const recovery = await saveRecovery(before.document);
        if (!recovery) throw fail("RECOVERY_REQUIRED", "A recovery copy could not be saved. The keyboard has not been changed.");
        const options = {nextRequestId: () => ids.next()};
        const base = capturedBase(before, capabilities);
        const beforeIdentity = before.identity;
        if (!beforeIdentity) throw fail("PROFILE_IDENTITY_UNAVAILABLE", "The keyboard read did not include a stable save identity.");
        const candidate = await readCandidate(connection, options);
        if (candidate.state !== CANDIDATE_STATE.IDLE) throw fail("KEYBOARD_BUSY", "The keyboard has an unfinished profile transaction. Finish or recover it before restoring.");
        const coordinator = createCoordinator(connection, {chunkSize: capabilities.candidateChunkMax, requestIds: ids,
            readPeerStatus: operations.readPeerStatus === undefined ? readCandidatePeerStatus : operations.readPeerStatus,
            onProgress: progress => coordinatorReport(progress, applyProgress)});
        const viaCoordinator = createViaCoordinator(connection, {requestIds: ids, onProgress: progress => applyProgress.report("stage", {completed: progress.completed, total: progress.total})});
        const expectedStorageDigest = viaStorageDigest(target);
        if (beforeIdentity.storageGeneration >= 0xffffffff) throw fail("STORAGE_GENERATION_EXHAUSTED", "The keyboard storage generation cannot advance safely.");
        const targetStorageGeneration = beforeIdentity.storageGeneration + 1;
        const macroRanges = changedRanges(base.macros, target.macros, {end: target.macros.length - 1});
        const layoutRanges = changedRanges(base.layout, target.layout);
        let prepared, decisionObserved = false, commitRequested = false;
        try {
            applyProgress.report("upload");
            prepared = await coordinator.upload(target.profile, {metadata: candidateMetadataForBlob(target.profile, {actionAbiDigest: capabilities.actionAbiDigest, viaGeneration: targetStorageGeneration, viaDigest: expectedStorageDigest}), verifyBase: async () => {
                const identity = await currentIdentity(connection, ids, {allowCandidate: true});
                if (identityKey(identity) !== identityKey(beforeIdentity)) throw fail("PROFILE_CHANGED", "The keyboard changed before restore could start.");
            }});
            mutated = true;
            applyProgress.report("stage");
            await viaCoordinator.stage({transactionId: prepared.transactionId, generation: targetStorageGeneration, digest: expectedStorageDigest, target, current: base});
            applyProgress.report("peer", peerReport(null));
            commitRequested = true;
            await coordinator.commit(prepared.transactionId, {digest: prepared.metadata.digest, afterDecision: async () => {
                decisionObserved = true;
                applyProgress.report("converge", {detail: "Waiting for the other half's copy of keys and macros"});
                await viaCoordinator.waitUntilAccepted({transactionId: prepared.transactionId, generation: targetStorageGeneration, digest: expectedStorageDigest});
                applyProgress.report("local");
                await rollForwardLocal(connection, target, base, layoutRanges, macroRanges, counted => applyProgress.report("local", counted));
            }});
            const macroBytes = macroRanges.reduce((sum, range) => sum + range.bytes.length, 0);
            const layoutBytes = layoutRanges.reduce((sum, range) => sum + range.bytes.length, 0);
            const macroWriteNeeded = macroRanges.length > 0 || base.macros.at(-1) !== target.macros.at(-1);
            applyProgress.report("verify", {detail: "Reading back both halves"});
            const storage = await waitStorage(connection, ids);
            await verifyRanges(connection, readStored, VIA_STORAGE.LAYOUT_READ, target.layout, layoutRanges);
            await verifyRanges(connection, readStored, VIA_STORAGE.MACRO_READ, target.macros, macroRanges, {verifyFinalByte: macroRanges.length > 0});
            const status = await readProfile(connection, options);
            const storageAfter = await readStorage(connection, ids);
            // The keyboard activates only after the other half confirmed its
            // copy, so an active target is saved on both halves even when the
            // cable between them came out after that confirmation.
            const peerUnseen = !(status.stateFlags & PROFILE_STATE_FLAGS.PEER_CONVERGED);
            if (status.activeKind !== PROFILE_ACTIVE_KIND.COMMITTED || status.activeDigest !== fnv1a32(target.profile) || status.committedDigest !== fnv1a32(target.profile) || status.conflictCount || !storage.ready || !storageAfter.ready || storage.generation !== targetStorageGeneration || storageAfter.generation !== targetStorageGeneration || storage.generation !== storageAfter.generation || storage.digest !== storageAfter.digest || storageAfter.digest !== expectedStorageDigest) throw fail("RESTORE_VERIFY_FAILED", "The keyboard did not confirm the imported profile on both halves.");
            const resultFingerprint = fingerprint(document);
            applyProgress.finish();
            return {document, fingerprint: resultFingerprint, summary: summary(document), status, identity: snapshotIdentity(status, storageAfter, encodeSettings(target.settings)), recovery, peerUnseen,
                performance: {elapsedMs: Date.now() - startedAt, baseSource: before === baseSnapshot ? "verified-cache" : "device-read", layoutBytes, macroBytes, viaConfigReports: 1, layoutReports: layoutRanges.reduce((sum, range) => sum + Math.ceil(range.bytes.length / 12), 0), macroReports: macroWriteNeeded ? macroRanges.reduce((sum, range) => sum + Math.ceil(range.bytes.length / 12), 0) : 0}};
        } catch (error) {
            let cancelled = false;
            if (prepared && !decisionObserved) {
                try { await viaCoordinator.abort({transactionId: prepared.transactionId, generation: targetStorageGeneration, digest: expectedStorageDigest}); } catch {}
                // The keyboard returns to idle on ABORT only while no commit
                // marker exists, so a completed abort proves nothing was saved.
                try { await coordinator.abort(prepared.transactionId); cancelled = true; } catch {}
            }
            if (!mutated) throw error;
            const restart = await peerRestartGuidance(connection, readProfile, options);
            // Before the commit was sent, or once the keyboard confirmed the
            // cancel, the profile it had is still the one it runs.
            if (!decisionObserved && (!commitRequested || cancelled)) {
                throw Object.assign(fail("RESTORE_NOT_SAVED", `Nothing was saved: the keyboard kept the profile it had. ${error.message}${restart}`), {cause: error});
            }
            // After the decision the keyboard owns the outcome: once this app
            // stops writing, it copies the other half's complete target itself.
            const finishing = decisionObserved ? "The keyboard had already decided to save it and finishes from the other half's copy by itself, about 15 seconds after both halves are connected; read the keyboard again then. Only if it still shows the old profile: " : "";
            throw Object.assign(fail("RESTORE_INCOMPLETE", `Restore was interrupted. Keep both halves connected. ${finishing}${before.incomplete ? `Import your original complete backup again. Interrupted data was saved for diagnosis at ${recovery}.` : `Import the recovery file ${recovery}.`} ${error.message}${restart}`), {cause: error});
        }
    }
}
module.exports = {upgradePdSnapshot, captureProfile, readIdentity, restoreProfile, validateSnapshot, summary, fingerprint, reorderLayers, peerReport};
