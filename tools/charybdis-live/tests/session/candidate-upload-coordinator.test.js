"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../../core/schema/profile-blob-v1");
const {
    CANDIDATE_ADMISSION,
    CANDIDATE_ERROR,
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    CandidateRequestIdSequence,
    CandidateTransactionIdSequence,
    PROFILE_CANDIDATE_V1,
    candidateMetadataForBlob,
} = require("../../core/protocol/profile-candidate-v1");
const {
    CandidateUploadCoordinator,
    CandidateUploadError,
    FIRMWARE_HOST_PRECOMMIT_TIMEOUT_MS,
    FIRMWARE_PREPARING_PEER_NO_PROGRESS_TIMEOUT_MS,
    candidateStatusStallTimeoutMs,
} = require("../../core/session/candidate-upload-coordinator");

function representativeBlob(payloadSize = 41) {
    return encodeProfileBlob({
        domains: [{
            id: PROFILE_DOMAIN_IDS.RGB,
            version: 1,
            payload: Buffer.from(Array.from({length: payloadSize}, (_, index) => index)),
        }],
    });
}

class CandidateFirmwareHarness {
    constructor(options = {}) {
        this.options = options;
        this.writes = [];
        this.requestOptions = [];
        this.mailbox = undefined;
        this.pendingStatusReads = 0;
        this.busied = new Set();
        this.validationReads = 0;
        this.commitReads = 0;
        this.activationReads = 0;
        this.prepareReads = 0;
        this.convergenceReads = 0;
        this.reportedStates = [];
        this.status = noErrorStatus();
    }

    async request(value, requestOptions) {
        const report = Buffer.from(value);
        this.writes.push(report);
        this.requestOptions.push(requestOptions);
        if (this.options.processThenThrowOnOperation !== undefined
            && this.options.processThenThrowOnOperation === operationForValue(report[2])) {
            this.options.processThenThrowOnOperation = undefined;
            this.process(report);
            const error = new Error("simulated lost admitted response");
            error.code = "TIMEOUT";
            throw error;
        }
        if (this.options.throwOnOperation !== undefined
            && this.options.throwOnOperation === operationForValue(report[2])) {
            this.options.throwOnOperation = undefined;
            const error = new Error("simulated transport loss");
            error.code = this.options.transportErrorCode || "TIMEOUT";
            throw error;
        }
        if (report[0] === 0x08 && report[2] === PROFILE_CANDIDATE_V1.VALUE_STATUS) {
            if (this.options.beforeFirstStatusResponse && !this.firstStatusDelayed) {
                this.firstStatusDelayed = true;
                await this.options.beforeFirstStatusResponse();
            }
            const response = this.statusReport(report);
            this.reportedStates.push(response[8]);
            assert.equal(requestOptions.matchResponse(response, report), true);
            return response;
        }
        assert.equal(
            report[0],
            report[2] === PROFILE_CANDIDATE_V1.VALUE_COMMIT
                ? PROFILE_CANDIDATE_V1.COMMAND_SAVE
                : PROFILE_CANDIDATE_V1.COMMAND_SET
        );
        const operation = operationForValue(report[2]);
        if (this.options.busyOnce?.has(operation) && !this.busied.has(operation)) {
            this.busied.add(operation);
            if (operation === CANDIDATE_OPERATION.VALIDATE && this.options.processBusyValidate) {
                this.process(report);
                this.status.state = CANDIDATE_STATE.VALIDATED;
            }
            if (operation === CANDIDATE_OPERATION.CHUNK && this.options.makeBusyChunkUnsafe) {
                this.status.nextOffset = report.readUInt16LE(5) + 1;
            }
            if (operation === CANDIDATE_OPERATION.ABORT && this.options.poisonOnBusyAbort) {
                this.status.state = CANDIDATE_STATE.REJECTED;
                this.status.flags = 2;
                this.status.error = {...noError(), id: CANDIDATE_ERROR.POISONED};
            }
            const response = acknowledgement(report, CANDIDATE_ADMISSION.BUSY, CANDIDATE_ERROR.MAILBOX_BUSY);
            assert.equal(requestOptions.matchResponse(response, report), true);
            return response;
        }
        if (this.mailbox) {
            return acknowledgement(report, CANDIDATE_ADMISSION.BUSY, CANDIDATE_ERROR.MAILBOX_BUSY);
        }
        this.mailbox = report;
        this.pendingStatusReads = this.options.neverProcess ? Number.POSITIVE_INFINITY : 1;
        const response = acknowledgement(report, CANDIDATE_ADMISSION.QUEUED, CANDIDATE_ERROR.NONE);
        assert.equal(requestOptions.matchResponse(response, report), true);
        return response;
    }

    statusReport(request) {
        if (this.mailbox && this.pendingStatusReads > 0) {
            this.pendingStatusReads -= 1;
            return encodeStatus(request, {...this.status, flags: 1});
        }
        if (this.mailbox) {
            const report = this.mailbox;
            this.mailbox = undefined;
            this.process(report);
        } else if (this.status.state === CANDIDATE_STATE.VALIDATING) {
            this.validationReads += 1;
            if (this.validationReads >= (this.options.validationReads || 1)) {
                this.status.state = this.options.validationError
                    ? CANDIDATE_STATE.REJECTED
                    : CANDIDATE_STATE.VALIDATED;
                if (this.options.validationError) {
                    this.status.flags = 2;
                    this.status.error = {
                        id: CANDIDATE_ERROR.VALIDATION_REJECTED,
                        domainId: 0x20,
                        tableId: 2,
                        rowIndex: 5,
                        tapIndex: 3,
                        fieldId: 4,
                        byteOffset: 6,
                    };
                }
            }
        } else if (this.status.state === CANDIDATE_STATE.PREPARING_PEER && this.status.lastOperation === CANDIDATE_OPERATION.COMMIT) {
            this.prepareReads += 1;
            if (this.prepareReads >= (this.options.prepareReads || 1)) {
                this.status.state = CANDIDATE_STATE.COMMITTING;
            }
        } else if (this.status.state === CANDIDATE_STATE.COMMITTING && this.status.lastOperation === CANDIDATE_OPERATION.COMMIT) {
            this.commitReads += 1;
            if (this.commitReads >= (this.options.commitReads || 1)) {
                this.status.state = this.options.splitBarrier
                    ? CANDIDATE_STATE.CONVERGING_PEER
                    : CANDIDATE_STATE.ACTIVATING;
            }
        } else if (this.status.state === CANDIDATE_STATE.CONVERGING_PEER && this.status.lastOperation === CANDIDATE_OPERATION.COMMIT) {
            this.convergenceReads += 1;
            if (this.convergenceReads >= (this.options.convergenceReads || 1)) {
                if (this.options.authorityErrorId !== undefined) {
                    this.status.state = CANDIDATE_STATE.AUTHORITY_FAILED;
                    this.status.flags = 2;
                    this.status.error = {...noError(), id: this.options.authorityErrorId};
                    this.status.operationSequence = (this.status.operationSequence + 1) & 0xffff;
                } else {
                    this.status.state = CANDIDATE_STATE.ACTIVATING;
                }
            }
        } else if (this.status.state === CANDIDATE_STATE.ACTIVATING && this.status.lastOperation === CANDIDATE_OPERATION.COMMIT) {
            this.activationReads += 1;
            if (this.activationReads >= (this.options.activationReads || 1)) {
                this.status.state = this.options.activationError
                    ? CANDIDATE_STATE.REJECTED
                    : CANDIDATE_STATE.IDLE;
                if (this.options.activationError) {
                    this.status.flags = 2;
                    this.status.error = {...noError(), id: this.options.activationErrorId || CANDIDATE_ERROR.ACTIVATION_FAILED};
                }
            }
        }
        return encodeStatus(request, this.status);
    }

    process(report) {
        const operation = operationForValue(report[2]);
        this.status.operationSequence = (this.status.operationSequence + 1) & 0xffff;
        this.status.lastOperation = operation;
        const transactionId = report.readUInt16LE(3);
        if (operation === CANDIDATE_OPERATION.BEGIN) {
            this.status = {
                ...noErrorStatus(),
                state: CANDIDATE_STATE.RECEIVING,
                lastOperation: operation,
                transactionId,
                payloadLength: report.readUInt16LE(9),
                digest: report.readUInt32LE(15),
                operationSequence: this.status.operationSequence,
            };
        } else if (operation === CANDIDATE_OPERATION.CHUNK) {
            this.status.transactionId = transactionId;
            this.status.nextOffset = report.readUInt16LE(5) + report[7];
            this.status.state = this.status.nextOffset === this.status.payloadLength
                ? CANDIDATE_STATE.COMPLETE
                : CANDIDATE_STATE.RECEIVING;
            this.status.error = noError();
        } else if (operation === CANDIDATE_OPERATION.VALIDATE) {
            this.status.transactionId = transactionId;
            this.status.state = CANDIDATE_STATE.VALIDATING;
            this.status.error = noError();
            this.validationReads = 0;
        } else if (operation === CANDIDATE_OPERATION.ABORT) {
            this.status = {
                ...noErrorStatus(),
                state: CANDIDATE_STATE.IDLE,
                lastOperation: operation,
                transactionId,
                operationSequence: this.status.operationSequence,
            };
        } else if (operation === CANDIDATE_OPERATION.COMMIT) {
            this.status.transactionId = transactionId;
            this.status.state = this.options.splitBarrier
                ? CANDIDATE_STATE.PREPARING_PEER
                : CANDIDATE_STATE.COMMITTING;
            this.status.error = noError();
            this.prepareReads = 0;
            this.commitReads = 0;
            this.convergenceReads = 0;
            this.activationReads = 0;
        }
    }
}

function noError() {
    return {
        id: CANDIDATE_ERROR.NONE,
        domainId: 0xff,
        tableId: 0xff,
        rowIndex: 0xffff,
        tapIndex: 0xff,
        fieldId: 0xff,
        byteOffset: 0xffff,
    };
}

function noErrorStatus() {
    return {
        state: CANDIDATE_STATE.IDLE,
        lastOperation: CANDIDATE_OPERATION.NONE,
        flags: 0,
        transactionId: 0,
        nextOffset: 0,
        payloadLength: 0,
        digest: 0,
        error: noError(),
        operationSequence: 0,
    };
}

function acknowledgement(request, admission, errorId) {
    const response = Buffer.from(request);
    response.fill(0, 5);
    response[5] = admission;
    response[6] = errorId;
    response[7] = 0xff;
    return response;
}

function encodeStatus(request, status) {
    const response = Buffer.from(request);
    response.fill(0, 5);
    response[5] = 0;
    response[6] = 25;
    const payload = response.subarray(7);
    payload[0] = 1;
    payload[1] = status.state;
    payload[2] = status.lastOperation;
    payload[3] = status.flags || 0;
    payload.writeUInt16LE(status.transactionId, 4);
    payload.writeUInt16LE(status.nextOffset, 6);
    payload.writeUInt16LE(status.payloadLength, 8);
    payload.writeUInt32LE(status.digest, 10);
    payload[14] = status.error.id;
    payload[15] = status.error.domainId;
    payload[16] = status.error.tableId;
    payload.writeUInt16LE(status.error.rowIndex, 17);
    payload[19] = status.error.tapIndex;
    payload[20] = status.error.fieldId;
    payload.writeUInt16LE(status.error.byteOffset, 21);
    payload.writeUInt16LE(status.operationSequence, 23);
    return response;
}

function operationForValue(valueId) {
    return {
        [PROFILE_CANDIDATE_V1.VALUE_BEGIN]: CANDIDATE_OPERATION.BEGIN,
        [PROFILE_CANDIDATE_V1.VALUE_CHUNK]: CANDIDATE_OPERATION.CHUNK,
        [PROFILE_CANDIDATE_V1.VALUE_VALIDATE]: CANDIDATE_OPERATION.VALIDATE,
        [PROFILE_CANDIDATE_V1.VALUE_COMMIT]: CANDIDATE_OPERATION.COMMIT,
        [PROFILE_CANDIDATE_V1.VALUE_ABORT]: CANDIDATE_OPERATION.ABORT,
    }[valueId];
}

function operationWrites(harness, valueId) {
    return harness.writes.filter((report) => (report[0] === PROFILE_CANDIDATE_V1.COMMAND_SET || report[0] === PROFILE_CANDIDATE_V1.COMMAND_SAVE) && report[2] === valueId);
}

function coordinator(harness, options = {}) {
    return new CandidateUploadCoordinator(harness, {
        pollIntervalMs: 0,
        maxStatusPolls: 20,
        ...options,
    });
}

test("upload stages strictly sequential 20-byte chunks and validates only after completion", async () => {
    const harness = new CandidateFirmwareHarness({validationReads: 2});
    const progress = [];
    const blob = representativeBlob();
    assert.equal(blob.length, 53);
    const result = await coordinator(harness, {
        transactionIds: new CandidateTransactionIdSequence(0xffff),
        requestIds: new CandidateRequestIdSequence(0xff),
        onProgress(value) {
            progress.push(value);
        },
    }).upload(blob, {actionAbiDigest: 0x12345678});

    assert.equal(result.transactionId, 0xffff);
    assert.equal(result.status.state, CANDIDATE_STATE.VALIDATED);
    const chunks = operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_CHUNK);
    assert.deepEqual(chunks.map((report) => report.readUInt16LE(5)), [0, 20, 40]);
    assert.deepEqual(chunks.map((report) => report[7]), [20, 20, 13]);
    assert.deepEqual(Buffer.concat(chunks.map((report) => report.subarray(8, 8 + report[7]))), blob);
    assert.equal(harness.writes.every((report) => report.length === 32), true);
    assert.equal(progress.at(-1).phase, "complete");
    assert.equal(progress.at(-1).bytesSent, blob.length);
    const requestIds = harness.writes.filter((report) => report[0] === 0x08).map((report) => report[3]);
    assert.deepEqual(requestIds.slice(0, 3), [0xff, 1, 2]);
    assert.equal(requestIds.includes(0), false);
});

test("a prepared candidate commits through custom-save and waits for activation", async () => {
    const harness = new CandidateFirmwareHarness({commitReads: 2, activationReads: 2});
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    const result = await client.commit(prepared.transactionId, {digest: prepared.metadata.digest});

    assert.equal(result.status.state, CANDIDATE_STATE.IDLE);
    assert.equal(result.status.lastOperation, CANDIDATE_OPERATION.COMMIT);
    assert.equal(result.digest, prepared.metadata.digest);
    const commits = operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT);
    assert.equal(commits.length, 1);
    assert.equal(commits[0][0], PROFILE_CANDIDATE_V1.COMMAND_SAVE);
});

test("a split commit polls through peer preparation and convergence before activation", async () => {
    const harness = new CandidateFirmwareHarness({
        splitBarrier: true,
        prepareReads: 2,
        commitReads: 2,
        convergenceReads: 2,
        activationReads: 2,
    });
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    const decisions = [];
    const result = await client.commit(prepared.transactionId, {digest: prepared.metadata.digest, afterDecision: async decision => {
        decisions.push(decision);
        assert.equal(decision.status.state, CANDIDATE_STATE.CONVERGING_PEER);
    }});

    assert.equal(result.status.state, CANDIDATE_STATE.IDLE);
    assert.equal(result.status.lastOperation, CANDIDATE_OPERATION.COMMIT);
    for (const state of [
        CANDIDATE_STATE.PREPARING_PEER,
        CANDIDATE_STATE.COMMITTING,
        CANDIDATE_STATE.CONVERGING_PEER,
        CANDIDATE_STATE.ACTIVATING,
    ]) {
        assert.equal(harness.reportedStates.includes(state), true, `missing reported candidate state ${state}`);
    }
    assert.equal(decisions.length, 1);
    assert.equal(decisions[0].transactionId, prepared.transactionId);
    assert.equal(decisions[0].digest, prepared.metadata.digest);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT).length, 1);
});

test("resuming a post-decision commit invokes local roll-forward once", async () => {
    const harness = new CandidateFirmwareHarness({splitBarrier: true, convergenceReads: 2, activationReads: 2});
    const blob = representativeBlob(1);
    const metadata = candidateMetadataForBlob(blob, {actionAbiDigest: 1});
    harness.status = {
        ...harness.status,
        state: CANDIDATE_STATE.CONVERGING_PEER,
        lastOperation: CANDIDATE_OPERATION.COMMIT,
        transactionId: 0x1234,
        payloadLength: blob.length,
        nextOffset: blob.length,
        digest: metadata.digest,
    };
    harness.commitCountdown = 0;
    harness.convergenceCountdown = 2;
    harness.activationCountdown = 2;
    let decisions = 0;
    const result = await coordinator(harness).commit(0x1234, {digest: metadata.digest, afterDecision: async () => { decisions++; }});
    assert.equal(result.status.state, CANDIDATE_STATE.IDLE);
    assert.equal(decisions, 1);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT).length, 0);
});

test("lost commit acknowledgement is safely resolved from progressing status", async () => {
    const harness = new CandidateFirmwareHarness({
        processThenThrowOnOperation: CANDIDATE_OPERATION.COMMIT,
        splitBarrier: true,
        commitReads: 2,
        prepareReads: 2,
        convergenceReads: 2,
        activationReads: 2,
    });
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});

    await assert.rejects(
        client.commit(prepared.transactionId, {digest: prepared.metadata.digest}),
        (error) => error.code === "TRANSPORT_OUTCOME_AMBIGUOUS"
            && error.ambiguous === true
            && error.safeToRetry === true
    );
    const recovered = await client.commit(prepared.transactionId, {digest: prepared.metadata.digest});
    assert.equal(recovered.status.state, CANDIDATE_STATE.IDLE);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT).length, 1);
});

test("terminal postcommit authority failure is surfaced without retrying or aborting", async () => {
    const harness = new CandidateFirmwareHarness({
        splitBarrier: true,
        authorityErrorId: CANDIDATE_ERROR.POSTCOMMIT_AUTHORITY_LOST,
    });
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});

    await assert.rejects(
        client.commit(prepared.transactionId, {digest: prepared.metadata.digest}),
        (error) => error.code === "AUTHORITY_FAILED"
            && error.phase === "committing"
            && error.ambiguous === false
            && error.safeToRetry === false
            && error.status.state === CANDIDATE_STATE.AUTHORITY_FAILED
            && error.deviceError.id === CANDIDATE_ERROR.POSTCOMMIT_AUTHORITY_LOST
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT).length, 1);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 0);
});

test("commit preflight refuses a mismatched digest before custom-save", async () => {
    const harness = new CandidateFirmwareHarness();
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    await assert.rejects(
        client.commit(prepared.transactionId, {digest: (prepared.metadata.digest ^ 1) >>> 0}),
        (error) => error.code === "CANDIDATE_IDENTITY_MISMATCH" && error.safeToRetry === false
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_COMMIT).length, 0);
});

test("activation failure remains visible after durable commit", async () => {
    const harness = new CandidateFirmwareHarness({activationError: true});
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    await assert.rejects(
        client.commit(prepared.transactionId, {digest: prepared.metadata.digest}),
        (error) => error.code === "DEVICE_REJECTED"
            && error.phase === "committing"
            && error.deviceError.id === CANDIDATE_ERROR.ACTIVATION_FAILED
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 0);
});

test("unknown marker durability is an ambiguous outcome that requires reconciliation", async () => {
    const harness = new CandidateFirmwareHarness({
        activationError: true,
        activationErrorId: CANDIDATE_ERROR.DURABILITY_UNKNOWN,
    });
    const client = coordinator(harness);
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    await assert.rejects(
        client.commit(prepared.transactionId, {digest: prepared.metadata.digest}),
        (error) => error.code === "DURABILITY_UNKNOWN"
            && error.ambiguous === true
            && error.safeToRetry === false
    );
});

test("preflight refuses every non-idle firmware candidate without sending a mutation", async () => {
    for (const state of [
        CANDIDATE_STATE.RECEIVING,
        CANDIDATE_STATE.COMPLETE,
        CANDIDATE_STATE.VALIDATING,
        CANDIDATE_STATE.VALIDATED,
        CANDIDATE_STATE.REJECTED,
        CANDIDATE_STATE.COMMITTING,
        CANDIDATE_STATE.ACTIVATING,
        CANDIDATE_STATE.PREPARING_PEER,
        CANDIDATE_STATE.CONVERGING_PEER,
        CANDIDATE_STATE.AUTHORITY_FAILED,
    ]) {
        const harness = new CandidateFirmwareHarness();
        harness.status.state = state;
        harness.status.transactionId = 0x4321;
        await assert.rejects(
            coordinator(harness).upload(representativeBlob(1), {actionAbiDigest: 1}),
            (error) => error.code === "ACTIVE_CANDIDATE"
                && error.phase === "preflight"
                && error.transactionId === 0x4321
                && error.safeToRetry === false
        );
        assert.equal(harness.writes.filter((report) => report[0] === PROFILE_CANDIDATE_V1.COMMAND_SET).length, 0);
    }
});

test("upload snapshots caller-owned bytes before asynchronous preflight", async () => {
    let releasePreflight;
    let preflightReached;
    const reached = new Promise((resolve) => {
        preflightReached = resolve;
    });
    const harness = new CandidateFirmwareHarness({
        beforeFirstStatusResponse() {
            preflightReached();
            return new Promise((resolve) => {
                releasePreflight = resolve;
            });
        },
    });
    const blob = representativeBlob(21);
    const expected = Buffer.from(blob);
    const upload = coordinator(harness).upload(blob, {actionAbiDigest: 1});
    await reached;
    blob.fill(0xee);
    releasePreflight();
    await upload;

    const chunks = operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_CHUNK);
    assert.deepEqual(Buffer.concat(chunks.map((report) => report.subarray(8, 8 + report[7]))), expected);
});

test("busy frames are resubmitted only from a safe status and exact bytes are preserved", async () => {
    const harness = new CandidateFirmwareHarness({
        busyOnce: new Set([CANDIDATE_OPERATION.CHUNK, CANDIDATE_OPERATION.VALIDATE]),
        processBusyValidate: true,
    });
    const blob = representativeBlob(1);
    const result = await coordinator(harness).upload(blob, {actionAbiDigest: 1, transactionId: 7});
    assert.equal(result.status.state, CANDIDATE_STATE.VALIDATED);
    const chunks = operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_CHUNK);
    assert.equal(chunks.length, 2);
    assert.deepEqual(chunks[0], chunks[1]);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_VALIDATE).length, 1);
});

test("busy polling refuses a partial-overlap chunk resubmission", async () => {
    const harness = new CandidateFirmwareHarness({
        busyOnce: new Set([CANDIDATE_OPERATION.CHUNK]),
        makeBusyChunkUnsafe: true,
    });
    await assert.rejects(
        coordinator(harness).upload(representativeBlob(1), {actionAbiDigest: 1}),
        (error) => error.code === "UNSAFE_RESUBMISSION"
            && error.phase === "writing"
            && error.abortAttempted === true
            && error.abortSucceeded === true
            && error.safeToRetry === true
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_CHUNK).length, 1);
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 1);
    assert.equal(harness.status.state, CANDIDATE_STATE.IDLE);
});

test("cancellation after staging starts performs a best-effort idempotent abort", async () => {
    const harness = new CandidateFirmwareHarness();
    const controller = new AbortController();
    const upload = coordinator(harness).upload(representativeBlob(), {
        actionAbiDigest: 1,
        signal: controller.signal,
        onProgress(progress) {
            if (progress.phase === "writing" && progress.bytesSent === 20) controller.abort();
        },
    });
    await assert.rejects(upload, (error) => {
        assert.ok(error instanceof CandidateUploadError);
        assert.equal(error.code, "CANCELLED");
        assert.equal(error.phase, "writing");
        assert.equal(error.progress.bytesSent, 20);
        assert.equal(error.abortAttempted, true);
        assert.equal(error.abortSucceeded, true);
        return true;
    });
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 1);
    assert.equal(harness.status.state, CANDIDATE_STATE.IDLE);
    const abortIndex = harness.writes.findIndex((report) => report[0] === PROFILE_CANDIDATE_V1.COMMAND_SET
        && report[2] === PROFILE_CANDIDATE_V1.VALUE_ABORT);
    assert.equal(harness.requestOptions.slice(0, abortIndex).some((options) => options.signal === controller.signal), true);
    assert.equal(harness.requestOptions[abortIndex].signal, undefined);
});

for (const transportErrorCode of ["TIMEOUT", "DISCONNECTED"]) {
    test(`${transportErrorCode.toLowerCase()} during a mutation is ambiguous and is never silently retried`, async () => {
        const harness = new CandidateFirmwareHarness({
            throwOnOperation: CANDIDATE_OPERATION.CHUNK,
            transportErrorCode,
        });
        await assert.rejects(
            coordinator(harness).upload(representativeBlob(), {actionAbiDigest: 1}),
            (error) => error.code === "TRANSPORT_OUTCOME_AMBIGUOUS"
                && error.phase === "writing"
                && error.ambiguous === true
                && error.safeToRetry === false
                && error.progress.bytesSent === 0
        );
        assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_CHUNK).length, 1);
        assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 0);
    });
}

test("a queued operation that never advances returns bounded structured ambiguity", async () => {
    const harness = new CandidateFirmwareHarness({neverProcess: true});
    await assert.rejects(
        coordinator(harness, {maxStatusPolls: 2}).upload(representativeBlob(1), {actionAbiDigest: 1}),
        (error) => error.code === "OPERATION_OUTCOME_AMBIGUOUS"
            && error.phase === "begin"
            && error.operation === CANDIDATE_OPERATION.BEGIN
            && error.ambiguous === true
            && error.safeToRetry === false
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_BEGIN).length, 1);
});

test("default status stall timing scales and gives peer preparation its unobservable-progress window", () => {
    const minimum = candidateStatusStallTimeoutMs(PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE);
    const maximum = candidateStatusStallTimeoutMs(PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE);
    const preparing = candidateStatusStallTimeoutMs(
        PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE,
        CANDIDATE_STATE.PREPARING_PEER
    );
    assert.equal(maximum > minimum, true);
    assert.equal(maximum < FIRMWARE_HOST_PRECOMMIT_TIMEOUT_MS, true);
    assert.equal(maximum, 13180);
    assert.equal(preparing, 80000);
    assert.equal(preparing > FIRMWARE_PREPARING_PEER_NO_PROGRESS_TIMEOUT_MS, true);
});

test("observable split state progress renews the stall deadline", async () => {
    let now = 0;
    const harness = new CandidateFirmwareHarness({
        splitBarrier: true,
        prepareReads: 2,
        commitReads: 2,
        convergenceReads: 2,
        activationReads: 2,
    });
    const client = new CandidateUploadCoordinator(harness, {
        pollIntervalMs: 1,
        statusStallTimeoutMs: 2,
        now: () => now,
        async sleep(milliseconds) {
            now += milliseconds;
        },
    });
    const prepared = await client.upload(representativeBlob(1), {actionAbiDigest: 1, transactionId: 0x1234});
    const result = await client.commit(prepared.transactionId, {digest: prepared.metadata.digest});
    assert.equal(result.status.state, CANDIDATE_STATE.IDLE);
    assert.equal(now > 2, true);
});

test("an unexpected polling failure cleans up the admitted candidate before retry", async () => {
    const harness = new CandidateFirmwareHarness();
    let sleepCalls = 0;
    await assert.rejects(
        coordinator(harness, {
            pollIntervalMs: 1,
            async sleep() {
                sleepCalls += 1;
                if (sleepCalls === 1) throw new Error("poll scheduler failed");
            },
        }).upload(representativeBlob(1), {actionAbiDigest: 1}),
        (error) => error.code === "UPLOAD_FAILED"
            && error.abortAttempted === true
            && error.abortSucceeded === true
            && error.safeToRetry === true
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length >= 1, true);
    assert.equal(harness.status.state, CANDIDATE_STATE.IDLE);
});

test("semantic validation failures retain phase, progress, and firmware locations", async () => {
    const harness = new CandidateFirmwareHarness({validationError: true});
    const blob = representativeBlob(1);
    await assert.rejects(
        coordinator(harness).upload(blob, {actionAbiDigest: 1, transactionId: 0x1234}),
        (error) => {
            assert.equal(error.code, "DEVICE_REJECTED");
            assert.equal(error.phase, "validating");
            assert.equal(error.transactionId, 0x1234);
            assert.equal(error.progress.bytesSent, blob.length);
            assert.equal(error.abortAttempted, true);
            assert.equal(error.abortSucceeded, true);
            assert.equal(error.safeToRetry, true);
            assert.deepEqual(error.deviceError, {
                id: CANDIDATE_ERROR.VALIDATION_REJECTED,
                name: "VALIDATION_REJECTED",
                domainId: 0x20,
                tableId: 2,
                rowIndex: 5,
                tapIndex: 3,
                fieldId: 4,
                byteOffset: 6,
            });
            return true;
        }
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 1);
    assert.equal(harness.status.state, CANDIDATE_STATE.IDLE);

    harness.options.validationError = false;
    const retry = await coordinator(harness).upload(blob, {actionAbiDigest: 1, transactionId: 0x1235});
    assert.equal(retry.status.state, CANDIDATE_STATE.VALIDATED);
});

test("cleanup resubmits an idempotent abort after busy status becomes poisoned", async () => {
    const harness = new CandidateFirmwareHarness({
        busyOnce: new Set([CANDIDATE_OPERATION.ABORT]),
        poisonOnBusyAbort: true,
        validationError: true,
    });
    await assert.rejects(
        coordinator(harness).upload(representativeBlob(1), {actionAbiDigest: 1}),
        (error) => error.code === "DEVICE_REJECTED"
            && error.abortAttempted === true
            && error.abortSucceeded === true
            && error.safeToRetry === true
    );
    assert.equal(operationWrites(harness, PROFILE_CANDIDATE_V1.VALUE_ABORT).length, 2);
    assert.equal(harness.status.state, CANDIDATE_STATE.IDLE);
});

test("a profile changed after BEGIN is aborted before any chunks or commit", async () => {
    const firmware = new CandidateFirmwareHarness();
    const coordinator = new CandidateUploadCoordinator(firmware, {pollIntervalMs: 0});
    let checked = false;
    await assert.rejects(coordinator.upload(representativeBlob(), {actionAbiDigest: 0x12345678, verifyBase: async () => {
        checked = true;
        assert.ok(firmware.writes.some(report => report[2] === PROFILE_CANDIDATE_V1.VALUE_BEGIN));
        throw Object.assign(new Error("Profile changed"), {code: "PROFILE_EDIT_CONFLICT"});
    }}), /Profile changed/);
    assert.equal(checked, true);
    assert.ok(firmware.writes.some(report => report[2] === PROFILE_CANDIDATE_V1.VALUE_ABORT));
    assert.equal(firmware.writes.some(report => [PROFILE_CANDIDATE_V1.VALUE_CHUNK, PROFILE_CANDIDATE_V1.VALUE_COMMIT].includes(report[2])), false);
});
