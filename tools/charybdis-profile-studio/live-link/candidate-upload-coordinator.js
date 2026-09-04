"use strict";

const {
    CANDIDATE_ADMISSION,
    CANDIDATE_ERROR,
    CANDIDATE_ERROR_NAMES,
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    CANDIDATE_STATE_NAMES,
    CandidateRequestIdSequence,
    CandidateTransactionIdSequence,
    PROFILE_CANDIDATE_V1,
    buildCandidateAbortRequest,
    buildCandidateBeginRequest,
    buildCandidateChunkRequest,
    buildCandidateCommitRequest,
    buildCandidateValidateRequest,
    candidateMetadataForBlob,
    candidateMutationResponseMatcher,
    decodeCandidateAcknowledgement,
    normalizeCandidateMetadata,
    readCandidateStatus,
} = require("./profile-candidate-v1");

const DEFAULT_POLL_INTERVAL_MS = 10;
const DEFAULT_MAX_BUSY_RESUBMISSIONS = 3;
const FIRMWARE_HOST_PRECOMMIT_TIMEOUT_MS = 15000;
const FIRMWARE_PREPARING_PEER_NO_PROGRESS_TIMEOUT_MS = 60000;
const FIRMWARE_SPLIT_PEER_TIMEOUT_MS = 3000;
const DEFAULT_PREPARING_PEER_TRANSFER_ALLOWANCE_MS = 20000;
const DEFAULT_STATUS_SCHEDULING_MARGIN_MS = 1000;
const DEFAULT_STATUS_SCAN_STEP_ALLOWANCE_MS = 45;
const DEFAULT_STATUS_TIMEOUT_SAFETY_MS = 1000;

const COMMIT_IN_PROGRESS_STATES = Object.freeze([
    CANDIDATE_STATE.PREPARING_PEER,
    CANDIDATE_STATE.COMMITTING,
    CANDIDATE_STATE.CONVERGING_PEER,
    CANDIDATE_STATE.ACTIVATING,
]);

const UPLOAD_PHASE = Object.freeze({
    PREFLIGHT: "preflight",
    BEGIN: "begin",
    WRITING: "writing",
    VALIDATE: "validate",
    VALIDATING: "validating",
    COMMIT_PREFLIGHT: "commit-preflight",
    COMMIT: "commit",
    COMMITTING: "committing",
    ABORTING: "aborting",
    COMPLETE: "complete",
});

class CandidateUploadError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "CandidateUploadError";
        this.code = code;
        this.phase = details.phase || UPLOAD_PHASE.PREFLIGHT;
        this.transactionId = details.transactionId;
        this.operation = details.operation;
        this.progress = details.progress ? cloneProgress(details.progress) : undefined;
        this.status = details.status ? cloneStatus(details.status) : undefined;
        this.deviceError = details.deviceError ? {...details.deviceError} : undefined;
        this.ambiguous = Boolean(details.ambiguous);
        this.safeToRetry = details.safeToRetry === true;
        this.abortAttempted = Boolean(details.abortAttempted);
        this.abortSucceeded = details.abortSucceeded;
        if (details.cause !== undefined) this.cause = details.cause;
    }
}

class CandidateUploadCoordinator {
    constructor(connection, options = {}) {
        if (!connection || typeof connection.request !== "function") {
            throw new TypeError("connection must provide request(report, options).");
        }
        this.connection = connection;
        this.chunkSize = normalizeInteger(
            options.chunkSize === undefined ? PROFILE_CANDIDATE_V1.CHUNK_MAX : options.chunkSize,
            "Candidate chunk size",
            1,
            PROFILE_CANDIDATE_V1.CHUNK_MAX
        );
        this.pollIntervalMs = normalizeInteger(
            options.pollIntervalMs === undefined ? DEFAULT_POLL_INTERVAL_MS : options.pollIntervalMs,
            "Candidate poll interval",
            0,
            60000
        );
        this.maxStatusPolls = options.maxStatusPolls === undefined
            ? undefined
            : normalizeInteger(options.maxStatusPolls, "Maximum candidate status polls", 1, 100000);
        this.statusStallTimeoutMs = options.statusStallTimeoutMs === undefined
            ? undefined
            : normalizeInteger(options.statusStallTimeoutMs, "Candidate status stall timeout", 1, 300000);
        this.maxBusyResubmissions = normalizeInteger(
            options.maxBusyResubmissions === undefined ? DEFAULT_MAX_BUSY_RESUBMISSIONS : options.maxBusyResubmissions,
            "Maximum busy resubmissions",
            0,
            100
        );
        this.requestTimeoutMs = options.requestTimeoutMs;
        this.onProgress = typeof options.onProgress === "function" ? options.onProgress : undefined;
        this.sleep = typeof options.sleep === "function" ? options.sleep : defaultSleep;
        this.now = typeof options.now === "function" ? options.now : Date.now;
        this.transactionIds = options.transactionIds || new CandidateTransactionIdSequence(options.transactionIdStart);
        this.requestIds = options.requestIds || new CandidateRequestIdSequence(options.requestIdStart);
        assertIdSequence(this.transactionIds, "transactionIds");
        assertIdSequence(this.requestIds, "requestIds");
        this.uploading = false;
    }

    async upload(value, options = {}) {
        if (this.uploading) {
            throw new CandidateUploadError("UPLOAD_BUSY", "A candidate upload is already active.", {
                phase: UPLOAD_PHASE.PREFLIGHT,
                safeToRetry: true,
            });
        }
        const context = createUploadContext(options.signal);
        this.uploading = true;
        context.onProgress = typeof options.onProgress === "function" ? options.onProgress : undefined;
        try {
            context.blob = copyBytes(value, "Candidate profile blob");
            context.totalBytes = context.blob.length;
            context.chunkCount = Math.ceil(context.totalBytes / this.chunkSize);
            if (options.metadata === undefined) {
                context.metadata = candidateMetadataForBlob(context.blob, {
                    actionAbiDigest: options.actionAbiDigest,
                    requestedDomains: options.requestedDomains,
                });
            } else {
                const supplied = normalizeCandidateMetadata(options.metadata);
                const derived = candidateMetadataForBlob(context.blob, {
                    actionAbiDigest: supplied.actionAbiDigest,
                    requestedDomains: supplied.requestedDomains,
                });
                for (const field of ["schemaMajor", "schemaMinor", "requestedDomains", "flags", "payloadLength", "crc32", "digest", "actionAbiDigest"]) {
                    if (supplied[field] !== derived[field]) {
                        throw new RangeError(`Candidate metadata ${field} does not match the exact candidate blob.`);
                    }
                }
                context.metadata = supplied;
            }

            throwIfCancelled(context);
            context.phase = UPLOAD_PHASE.PREFLIGHT;
            context.status = await this.readStatus(context, {ambiguous: false});
            throwIfCancelled(context);
            assertNoActiveCandidate(context.status);
            context.transactionId = options.transactionId === undefined
                ? allocateTransactionId(this.transactionIds)
                : normalizeTransactionId(options.transactionId);
            this.emitProgress(context);

            context.phase = UPLOAD_PHASE.BEGIN;
            context.operation = CANDIDATE_OPERATION.BEGIN;
            context.abortNeeded = true;
            context.status = await this.performMutation(
                buildCandidateBeginRequest(context.transactionId, context.metadata),
                context,
                {metadata: context.metadata}
            );
            this.emitProgress(context);

            context.phase = UPLOAD_PHASE.WRITING;
            for (let offset = 0, chunkIndex = 0; offset < context.blob.length; chunkIndex += 1) {
                throwIfCancelled(context);
                const bytes = context.blob.subarray(offset, Math.min(offset + this.chunkSize, context.blob.length));
                context.operation = CANDIDATE_OPERATION.CHUNK;
                context.chunkIndex = chunkIndex;
                context.status = await this.performMutation(
                    buildCandidateChunkRequest(context.transactionId, offset, bytes),
                    context,
                    {offset, length: bytes.length}
                );
                offset += bytes.length;
                context.bytesSent = offset;
                this.emitProgress(context);
            }

            throwIfCancelled(context);
            context.phase = UPLOAD_PHASE.VALIDATE;
            context.operation = CANDIDATE_OPERATION.VALIDATE;
            context.status = await this.performMutation(
                buildCandidateValidateRequest(context.transactionId),
                context
            );
            this.emitProgress(context);

            context.phase = UPLOAD_PHASE.VALIDATING;
            context.status = await this.waitForValidation(context);
            context.phase = UPLOAD_PHASE.COMPLETE;
            context.operation = CANDIDATE_OPERATION.NONE;
            context.abortNeeded = false;
            this.emitProgress(context);
            return {
                transactionId: context.transactionId,
                metadata: {...context.metadata},
                status: cloneStatus(context.status),
                progress: progressFor(context),
            };
        } catch (cause) {
            if (isCancellation(cause, context)) {
                const abortResult = context.abortNeeded
                    ? await this.bestEffortAbort(context)
                    : {attempted: false, succeeded: undefined};
                throw uploadError("CANCELLED", "Candidate upload was cancelled.", context, {
                    abortAttempted: abortResult.attempted,
                    abortSucceeded: abortResult.succeeded,
                    ambiguous: abortResult.attempted && abortResult.succeeded !== true,
                    cause: cause instanceof Error ? cause : undefined,
                    safeToRetry: abortResult.succeeded === true,
                });
            }
            if (cause instanceof CandidateUploadError) {
                if (context.abortNeeded && !cause.ambiguous) {
                    const abortResult = await this.bestEffortAbort(context);
                    cause.abortAttempted = abortResult.attempted;
                    cause.abortSucceeded = abortResult.succeeded;
                    cause.safeToRetry = abortResult.succeeded === true;
                    if (abortResult.succeeded !== true) cause.ambiguous = true;
                }
                throw cause;
            }
            const abortResult = context.abortNeeded
                ? await this.bestEffortAbort(context)
                : {attempted: false, succeeded: undefined};
            throw uploadError(context.abortNeeded ? "UPLOAD_FAILED" : "INVALID_CANDIDATE", cause?.message || "Candidate upload failed.", context, {
                abortAttempted: abortResult.attempted,
                abortSucceeded: abortResult.succeeded,
                ambiguous: abortResult.attempted && abortResult.succeeded !== true,
                cause,
                safeToRetry: !context.abortNeeded || abortResult.succeeded === true,
            });
        } finally {
            this.uploading = false;
        }
    }

    async commit(transactionId, options = {}) {
        if (this.uploading) {
            throw new CandidateUploadError("UPLOAD_BUSY", "A candidate operation is already active.", {
                phase: UPLOAD_PHASE.COMMIT_PREFLIGHT,
                safeToRetry: true,
            });
        }
        const context = createUploadContext(options.signal);
        context.transactionId = normalizeTransactionId(transactionId);
        context.onProgress = typeof options.onProgress === "function" ? options.onProgress : undefined;
        context.expectedDigest = options.digest === undefined ? undefined : normalizeInteger(options.digest, "Candidate digest", 0, 0xffffffff);
        this.uploading = true;
        try {
            throwIfCancelled(context);
            context.phase = UPLOAD_PHASE.COMMIT_PREFLIGHT;
            context.status = await this.readStatus(context, {ambiguous: false});
            assertCandidateIdentity(context.status, context);
            if (context.expectedDigest !== undefined && context.status.digest !== context.expectedDigest) {
                throw uploadError("CANDIDATE_IDENTITY_MISMATCH", "Prepared candidate digest does not match the requested commit.", context, {safeToRetry: false});
            }
            context.expectedDigest = context.status.digest;
            if (commitIsComplete(context.status, context)) {
                assertNoDeviceError(context.status, context);
                context.phase = UPLOAD_PHASE.COMPLETE;
                this.emitProgress(context);
                return commitResult(context);
            }
            if (context.status.state === CANDIDATE_STATE.AUTHORITY_FAILED) {
                throw deviceRejection(context.status, context);
            }
            if (COMMIT_IN_PROGRESS_STATES.includes(context.status.state)) {
                context.phase = UPLOAD_PHASE.COMMITTING;
                context.operation = CANDIDATE_OPERATION.COMMIT;
                context.status = await this.waitForCommit(context);
                context.phase = UPLOAD_PHASE.COMPLETE;
                context.operation = CANDIDATE_OPERATION.NONE;
                this.emitProgress(context);
                return commitResult(context);
            }
            if (context.status.state !== CANDIDATE_STATE.VALIDATED || context.status.error.id !== CANDIDATE_ERROR.NONE) {
                throw uploadError("CANDIDATE_NOT_VALIDATED", "Only the matching validated candidate can be committed.", context, {safeToRetry: false});
            }

            throwIfCancelled(context);
            context.phase = UPLOAD_PHASE.COMMIT;
            context.operation = CANDIDATE_OPERATION.COMMIT;
            context.status = await this.performMutation(
                buildCandidateCommitRequest(context.transactionId),
                context
            );
            context.phase = UPLOAD_PHASE.COMMITTING;
            context.status = await this.waitForCommit(context);
            context.phase = UPLOAD_PHASE.COMPLETE;
            context.operation = CANDIDATE_OPERATION.NONE;
            this.emitProgress(context);
            return commitResult(context);
        } catch (cause) {
            if (cause instanceof CandidateUploadError) throw cause;
            throw uploadError("COMMIT_FAILED", cause?.message || "Candidate commit failed.", context, {
                ambiguous: context.phase !== UPLOAD_PHASE.COMMIT_PREFLIGHT,
                cause,
                safeToRetry: context.operation === CANDIDATE_OPERATION.COMMIT,
            });
        } finally {
            this.uploading = false;
        }
    }

    async performMutation(frame, context, operationDetails = {}) {
        let baseline = context.status;
        let busyResubmissions = 0;
        while (true) {
            throwIfCancelled(context);
            let response;
            try {
                response = await this.connection.request(frame, {
                    matchResponse: candidateMutationResponseMatcher,
                    signal: requestSignal(context),
                    timeoutMs: this.requestTimeoutMs,
                });
            } catch (cause) {
                throw uploadError(
                    "TRANSPORT_OUTCOME_AMBIGUOUS",
                    "The candidate mutation response was lost; the keyboard may or may not have admitted it.",
                    context,
                    {ambiguous: true, cause, safeToRetry: context.operation === CANDIDATE_OPERATION.COMMIT}
                );
            }
            let acknowledgment;
            try {
                acknowledgment = decodeCandidateAcknowledgement(response, frame);
            } catch (cause) {
                throw uploadError("PROTOCOL_ERROR", cause.message, context, {
                    ambiguous: true,
                    cause,
                    safeToRetry: false,
                });
            }
            throwIfCancelled(context);

            if (acknowledgment.admission === CANDIDATE_ADMISSION.QUEUED) {
                return this.waitForProcessedOperation(context, baseline, operationDetails);
            }
            if (acknowledgment.admission !== CANDIDATE_ADMISSION.BUSY) {
                throw uploadError(
                    "ADMISSION_REJECTED",
                    `Firmware rejected the candidate ${operationLabel(context.operation)} frame at admission.`,
                    context,
                    {
                        deviceError: {
                            id: acknowledgment.errorId,
                            name: acknowledgment.errorName,
                            frameOffset: acknowledgment.frameOffset,
                        },
                        safeToRetry: false,
                    }
                );
            }

            const resolution = await this.waitUntilBusyCanResolve(context, baseline, operationDetails);
            if (resolution.processed) {
                return resolution.status;
            }
            if (!resolution.safeToResubmit) {
                throw uploadError(
                    "UNSAFE_RESUBMISSION",
                    `Firmware was busy and the ${operationLabel(context.operation)} frame cannot be resubmitted safely.`,
                    context,
                    {status: resolution.status, safeToRetry: false}
                );
            }
            if (busyResubmissions >= this.maxBusyResubmissions) {
                throw uploadError("MAILBOX_BUSY", "Firmware remained busy after the bounded resubmission limit.", context, {
                    status: resolution.status,
                    safeToRetry: true,
                });
            }
            busyResubmissions += 1;
            context.status = resolution.status;
            baseline = resolution.status;
        }
    }

    async waitForProcessedOperation(context, baseline, operationDetails) {
        const budget = this.createStatusBudget(context, baseline);
        while (true) {
            throwIfCancelled(context);
            const status = await this.readStatus(context, {ambiguous: true});
            context.status = status;
            this.emitProgress(context);
            if (status.operationSequence !== baseline.operationSequence) {
                if (status.lastOperation !== context.operation) {
                    throw uploadError(
                        "OPERATION_CORRELATION_MISMATCH",
                        `Candidate operation sequence advanced for ${operationLabel(status.lastOperation)}, not ${operationLabel(context.operation)}.`,
                        context,
                        {ambiguous: true, status, safeToRetry: false}
                    );
                }
                assertSuccessfulOperationStatus(status, context, operationDetails);
                return status;
            }
            if (!this.statusBudgetAllowsAnotherPoll(budget, status)) break;
            await this.waitBeforePoll(context);
        }
        throw uploadError(
            "OPERATION_OUTCOME_AMBIGUOUS",
            `Candidate ${operationLabel(context.operation)} made no observable progress within its firmware-aware wait window.`,
            context,
            {ambiguous: true, safeToRetry: false}
        );
    }

    async waitUntilBusyCanResolve(context, baseline, operationDetails) {
        const budget = this.createStatusBudget(context, baseline);
        while (true) {
            throwIfCancelled(context);
            const status = await this.readStatus(context, {ambiguous: false});
            context.status = status;
            this.emitProgress(context);

            if (status.operationSequence !== baseline.operationSequence
                && status.lastOperation === context.operation
                && operationStatusCouldBeOurs(status, context, operationDetails)) {
                assertSuccessfulOperationStatus(status, context, operationDetails);
                return {processed: true, safeToResubmit: false, status};
            }
            if (!status.mailboxPending) {
                return {
                    processed: false,
                    safeToResubmit: canSafelyResubmit(status, context, operationDetails),
                    status,
                };
            }
            if (!this.statusBudgetAllowsAnotherPoll(budget, status)) break;
            await this.waitBeforePoll(context);
        }
        throw uploadError("MAILBOX_BUSY", "Firmware mailbox made no observable progress within its firmware-aware wait window.", context, {
            safeToRetry: true,
        });
    }

    async waitForValidation(context) {
        const budget = this.createStatusBudget(context, context.status);
        while (true) {
            throwIfCancelled(context);
            const status = context.status?.state === CANDIDATE_STATE.VALIDATED
                || context.status?.state === CANDIDATE_STATE.REJECTED
                ? context.status
                : await this.readStatus(context, {ambiguous: true});
            context.status = status;
            this.emitProgress(context);
            assertCandidateIdentity(status, context);
            if (status.state === CANDIDATE_STATE.VALIDATED) {
                assertNoDeviceError(status, context);
                return status;
            }
            if (status.state === CANDIDATE_STATE.REJECTED || status.error.id !== CANDIDATE_ERROR.NONE) {
                throw deviceRejection(status, context);
            }
            if (status.state !== CANDIDATE_STATE.VALIDATING) {
                throw uploadError("INVALID_VALIDATION_STATE", `Firmware entered candidate state ${status.state} during validation.`, context, {
                    status,
                    safeToRetry: false,
                });
            }
            if (!this.statusBudgetAllowsAnotherPoll(budget, status)) break;
            await this.waitBeforePoll(context);
        }
        throw uploadError("VALIDATION_OUTCOME_AMBIGUOUS", "Candidate validation made no observable progress within its firmware-aware wait window.", context, {
            ambiguous: true,
            safeToRetry: false,
        });
    }

    async waitForCommit(context) {
        const budget = this.createStatusBudget(context, context.status);
        let first = true;
        while (true) {
            throwIfCancelled(context);
            const status = first
                ? context.status
                : await this.readStatus(context, {ambiguous: true});
            first = false;
            context.status = status;
            this.emitProgress(context);
            assertCandidateIdentity(status, context);
            if (status.digest !== context.expectedDigest) {
                throw uploadError("CANDIDATE_IDENTITY_MISMATCH", "Commit status digest changed during persistence.", context, {status, safeToRetry: false});
            }
            if (commitIsComplete(status, context)) {
                assertNoDeviceError(status, context);
                return status;
            }
            if (status.state === CANDIDATE_STATE.AUTHORITY_FAILED || status.state === CANDIDATE_STATE.REJECTED || status.error.id !== CANDIDATE_ERROR.NONE) {
                throw deviceRejection(status, context);
            }
            if (!COMMIT_IN_PROGRESS_STATES.includes(status.state)) {
                throw uploadError("INVALID_COMMIT_STATE", `Firmware entered candidate state ${status.state} during commit.`, context, {status, safeToRetry: false});
            }
            if (!this.statusBudgetAllowsAnotherPoll(budget, status)) break;
            await this.waitBeforePoll(context);
        }
        throw uploadError("COMMIT_OUTCOME_AMBIGUOUS", "Candidate commit made no observable progress within its firmware-aware wait window.", context, {
            ambiguous: true,
            safeToRetry: true,
        });
    }

    async readStatus(context, options) {
        try {
            return await readCandidateStatus(this.connection, {
                nextRequestId: () => this.requestIds.next(),
                signal: requestSignal(context),
                timeoutMs: this.requestTimeoutMs,
            });
        } catch (cause) {
            throw uploadError(
                options.ambiguous ? "TRANSPORT_OUTCOME_AMBIGUOUS" : "TRANSPORT_FAILURE",
                options.ambiguous
                    ? "Candidate status could not be read after a mutation; its outcome is ambiguous."
                    : "Candidate status could not be read.",
                context,
                {ambiguous: options.ambiguous, cause, safeToRetry: !options.ambiguous}
            );
        }
    }

    createStatusBudget(context, baseline) {
        const timeoutMs = this.statusStallTimeoutMs === undefined
            ? candidateStatusStallTimeoutMs(candidatePayloadLength(context), baseline?.state)
            : this.statusStallTimeoutMs;
        return {
            configuredTimeoutMs: this.statusStallTimeoutMs,
            deadline: this.now() + timeoutMs,
            fingerprint: statusProgressFingerprint(baseline),
            maxPolls: this.maxStatusPolls,
            payloadLength: candidatePayloadLength(context),
            polls: 0,
            timeoutMs,
        };
    }

    statusBudgetAllowsAnotherPoll(budget, status) {
        const now = this.now();
        const fingerprint = statusProgressFingerprint(status);
        budget.polls += 1;
        if (fingerprint !== budget.fingerprint) {
            budget.fingerprint = fingerprint;
            budget.timeoutMs = budget.configuredTimeoutMs === undefined
                ? candidateStatusStallTimeoutMs(budget.payloadLength, status?.state)
                : budget.configuredTimeoutMs;
            budget.deadline = now + budget.timeoutMs;
        }
        if (budget.maxPolls !== undefined && budget.polls >= budget.maxPolls) return false;
        return now < budget.deadline;
    }

    async waitBeforePoll(context) {
        throwIfCancelled(context);
        if (this.pollIntervalMs > 0) {
            await this.sleep(this.pollIntervalMs);
        }
        throwIfCancelled(context);
    }

    async bestEffortAbort(context) {
        const result = {attempted: true, succeeded: false};
        const previousPhase = context.phase;
        const previousOperation = context.operation;
        try {
            context.phase = UPLOAD_PHASE.ABORTING;
            context.operation = CANDIDATE_OPERATION.ABORT;
            context.cancellationSuppressed = true;
            if (!context.status) {
                context.status = await this.readStatus(context, {ambiguous: false});
            }
            context.status = await this.performMutation(
                buildCandidateAbortRequest(context.transactionId),
                context
            );
            result.succeeded = context.status.state === CANDIDATE_STATE.IDLE;
        } catch {
            result.succeeded = false;
        } finally {
            context.cancellationSuppressed = false;
            context.phase = previousPhase;
            context.operation = previousOperation;
        }
        return result;
    }

    emitProgress(context) {
        const listener = typeof context.onProgress === "function" ? context.onProgress : this.onProgress;
        if (!listener) return;
        try {
            listener(progressFor(context));
        } catch {
            // Progress observers must not affect the transport transaction.
        }
    }
}

function assertSuccessfulOperationStatus(status, context, operationDetails) {
    if (status.error.id !== CANDIDATE_ERROR.NONE || status.state === CANDIDATE_STATE.REJECTED || status.state === CANDIDATE_STATE.AUTHORITY_FAILED) {
        throw deviceRejection(status, context);
    }
    assertCandidateIdentity(status, context);
    switch (context.operation) {
        case CANDIDATE_OPERATION.BEGIN:
            if (status.state !== CANDIDATE_STATE.RECEIVING
                || status.nextOffset !== 0
                || status.payloadLength !== operationDetails.metadata.payloadLength
                || status.digest !== operationDetails.metadata.digest) {
                throw operationStatusMismatch(status, context, "begin metadata/state did not match");
            }
            break;
        case CANDIDATE_OPERATION.CHUNK: {
            const expectedOffset = operationDetails.offset + operationDetails.length;
            const expectedState = expectedOffset === status.payloadLength
                ? CANDIDATE_STATE.COMPLETE
                : CANDIDATE_STATE.RECEIVING;
            if (status.nextOffset !== expectedOffset || status.state !== expectedState) {
                throw operationStatusMismatch(status, context, `next offset ${status.nextOffset} did not equal ${expectedOffset}`);
            }
            break;
        }
        case CANDIDATE_OPERATION.VALIDATE:
            if (![CANDIDATE_STATE.VALIDATING, CANDIDATE_STATE.VALIDATED].includes(status.state)) {
                throw operationStatusMismatch(status, context, `validation entered state ${status.state}`);
            }
            break;
        case CANDIDATE_OPERATION.COMMIT:
            if (![...COMMIT_IN_PROGRESS_STATES, CANDIDATE_STATE.IDLE].includes(status.state)) {
                throw operationStatusMismatch(status, context, `commit entered state ${status.state}`);
            }
            break;
        case CANDIDATE_OPERATION.ABORT:
            if (status.state !== CANDIDATE_STATE.IDLE) {
                throw operationStatusMismatch(status, context, `abort entered state ${status.state}`);
            }
            break;
        default:
            throw operationStatusMismatch(status, context, "unknown operation");
    }
}

function operationStatusCouldBeOurs(status, context, operationDetails) {
    if (status.transactionId !== context.transactionId) return false;
    if (status.error.id !== CANDIDATE_ERROR.NONE) return true;
    switch (context.operation) {
        case CANDIDATE_OPERATION.BEGIN:
            return status.payloadLength === operationDetails.metadata.payloadLength
                && status.digest === operationDetails.metadata.digest;
        case CANDIDATE_OPERATION.CHUNK:
            return status.nextOffset === operationDetails.offset + operationDetails.length;
        case CANDIDATE_OPERATION.VALIDATE:
            return [CANDIDATE_STATE.VALIDATING, CANDIDATE_STATE.VALIDATED, CANDIDATE_STATE.REJECTED].includes(status.state);
        case CANDIDATE_OPERATION.COMMIT:
            return [...COMMIT_IN_PROGRESS_STATES, CANDIDATE_STATE.IDLE, CANDIDATE_STATE.REJECTED, CANDIDATE_STATE.AUTHORITY_FAILED].includes(status.state);
        case CANDIDATE_OPERATION.ABORT:
            return status.state === CANDIDATE_STATE.IDLE;
        default:
            return false;
    }
}

function canSafelyResubmit(status, context, operationDetails) {
    if (status.mailboxPending) return false;
    if (context.operation === CANDIDATE_OPERATION.ABORT) {
        return status.state === CANDIDATE_STATE.IDLE || status.transactionId === context.transactionId;
    }
    if (status.poisoned) return false;
    switch (context.operation) {
        case CANDIDATE_OPERATION.BEGIN:
            return status.state === CANDIDATE_STATE.IDLE;
        case CANDIDATE_OPERATION.CHUNK: {
            if (status.transactionId !== context.transactionId
                || ![CANDIDATE_STATE.RECEIVING, CANDIDATE_STATE.COMPLETE].includes(status.state)) {
                return false;
            }
            const end = operationDetails.offset + operationDetails.length;
            return status.nextOffset === operationDetails.offset || status.nextOffset === end;
        }
        case CANDIDATE_OPERATION.VALIDATE:
            return status.transactionId === context.transactionId && status.state === CANDIDATE_STATE.COMPLETE;
        case CANDIDATE_OPERATION.COMMIT:
            return status.transactionId === context.transactionId
                && ([CANDIDATE_STATE.VALIDATED, ...COMMIT_IN_PROGRESS_STATES].includes(status.state)
                    || (status.state === CANDIDATE_STATE.IDLE && status.lastOperation === CANDIDATE_OPERATION.COMMIT));
        default:
            return false;
    }
}

function assertCandidateIdentity(status, context) {
    if (status.transactionId !== context.transactionId) {
        throw uploadError(
            "TRANSACTION_CORRELATION_MISMATCH",
            `Candidate status transaction ${status.transactionId} does not match ${context.transactionId}.`,
            context,
            {status, safeToRetry: false}
        );
    }
}

function commitIsComplete(status, context) {
    return status.state === CANDIDATE_STATE.IDLE
        && status.lastOperation === CANDIDATE_OPERATION.COMMIT
        && status.transactionId === context.transactionId
        && status.digest === context.expectedDigest;
}

function commitResult(context) {
    return {
        transactionId: context.transactionId,
        digest: context.expectedDigest,
        status: cloneStatus(context.status),
        progress: progressFor(context),
    };
}

function assertNoDeviceError(status, context) {
    if (status.error.id !== CANDIDATE_ERROR.NONE) {
        throw deviceRejection(status, context);
    }
}

function deviceRejection(status, context) {
    if (status.error.id === CANDIDATE_ERROR.DURABILITY_UNKNOWN) {
        return uploadError(
            "DURABILITY_UNKNOWN",
            "Firmware could not confirm whether the final commit marker became durable; reconcile device status before retrying.",
            context,
            {ambiguous: true, deviceError: status.error, status, safeToRetry: false}
        );
    }
    if (status.state === CANDIDATE_STATE.AUTHORITY_FAILED) {
        return uploadError(
            "AUTHORITY_FAILED",
            `The committed profile could not establish split authority (${CANDIDATE_ERROR_NAMES[status.error.id] || `error ${status.error.id}`}); it was not activated.`,
            context,
            {deviceError: status.error, status, safeToRetry: false}
        );
    }
    return uploadError(
        "DEVICE_REJECTED",
        `Firmware rejected the candidate with ${CANDIDATE_ERROR_NAMES[status.error.id] || `error ${status.error.id}`}.`,
        context,
        {deviceError: status.error, status, safeToRetry: false}
    );
}

function operationStatusMismatch(status, context, reason) {
    return uploadError(
        "OPERATION_STATUS_MISMATCH",
        `Processed candidate ${operationLabel(context.operation)} status was inconsistent: ${reason}.`,
        context,
        {status, safeToRetry: false}
    );
}

function createUploadContext(signal) {
    if (signal !== undefined && !isAbortSignal(signal)) {
        throw new TypeError("upload signal must be an AbortSignal.");
    }
    return {
        abortNeeded: false,
        blob: undefined,
        bytesSent: 0,
        cancellationSuppressed: false,
        chunkCount: 0,
        chunkIndex: -1,
        metadata: undefined,
        onProgress: undefined,
        operation: CANDIDATE_OPERATION.NONE,
        phase: UPLOAD_PHASE.PREFLIGHT,
        signal,
        status: undefined,
        totalBytes: 0,
        transactionId: undefined,
    };
}

function candidatePayloadLength(context) {
    return context.metadata?.payloadLength
        ?? context.status?.payloadLength
        ?? PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE;
}

// Firmware performs at most one <=20-byte candidate/split work item per scan.
// Ordinary staging therefore scales with the actual candidate, leaves room
// for the split peer's 3-second liveness window, and expires before the 15-
// second precommit inactivity cleanup. PREPARING_PEER is different: candidate
// status cannot expose its internal acknowledged split offset, so the desktop
// allows the firmware's full 60-second no-progress window plus enough time for
// the audited maximum transfer and scheduling overhead. Observable status
// transitions renew the deadline for the newly visible phase.
function candidateStatusStallTimeoutMs(payloadLength, state) {
    if (state === CANDIDATE_STATE.PREPARING_PEER) {
        return FIRMWARE_PREPARING_PEER_NO_PROGRESS_TIMEOUT_MS
            + DEFAULT_PREPARING_PEER_TRANSFER_ALLOWANCE_MS;
    }
    const boundedLength = Math.max(
        PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE,
        Math.min(PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE, Number(payloadLength) || PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE)
    );
    const scanSteps = Math.ceil(boundedLength / PROFILE_CANDIDATE_V1.CHUNK_MAX);
    const workAllowance = FIRMWARE_SPLIT_PEER_TIMEOUT_MS
        + DEFAULT_STATUS_SCHEDULING_MARGIN_MS
        + scanSteps * DEFAULT_STATUS_SCAN_STEP_ALLOWANCE_MS;
    return Math.min(
        FIRMWARE_HOST_PRECOMMIT_TIMEOUT_MS - DEFAULT_STATUS_TIMEOUT_SAFETY_MS,
        workAllowance
    );
}

function statusProgressFingerprint(status) {
    if (!status) return "none";
    return [
        status.state,
        status.lastOperation,
        status.flags,
        status.operationSequence,
        status.nextOffset,
        status.error?.id,
    ].join(":");
}

function throwIfCancelled(context) {
    if (!context.cancellationSuppressed && context.signal?.aborted) {
        throw uploadError("CANCELLED", "Candidate upload was cancelled.", context, {safeToRetry: false});
    }
}

function isCancellation(cause, context) {
    return cause?.code === "CANCELLED" || (!context.cancellationSuppressed && context.signal?.aborted);
}

function allocateTransactionId(sequence) {
    return normalizeTransactionId(sequence.next());
}

function assertNoActiveCandidate(status) {
    if (status.state !== CANDIDATE_STATE.IDLE) {
        throw new CandidateUploadError(
            "ACTIVE_CANDIDATE",
            `Candidate transaction ${status.transactionId} is already in firmware state ${CANDIDATE_STATE_NAMES[status.state] || status.state}; abort or finish it before starting another upload.`,
            {phase: UPLOAD_PHASE.PREFLIGHT, status, transactionId: status.transactionId, safeToRetry: false}
        );
    }
}

function progressFor(context) {
    return {
        phase: context.phase,
        transactionId: context.transactionId,
        operation: context.operation,
        bytesSent: context.bytesSent,
        totalBytes: context.totalBytes,
        chunkIndex: context.chunkIndex,
        chunkCount: context.chunkCount,
        status: context.status ? cloneStatus(context.status) : null,
    };
}

function cloneProgress(progress) {
    return {
        ...progress,
        status: progress.status ? cloneStatus(progress.status) : progress.status,
    };
}

function cloneStatus(status) {
    return {
        ...status,
        error: status.error ? {...status.error} : status.error,
    };
}

function uploadError(code, message, context, details = {}) {
    return new CandidateUploadError(code, message, {
        phase: context.phase,
        transactionId: context.transactionId,
        operation: context.operation,
        progress: progressFor(context),
        status: details.status || context.status,
        ...details,
    });
}

function operationLabel(operation) {
    return Object.entries(CANDIDATE_OPERATION).find(([, value]) => value === operation)?.[0].toLowerCase() || `operation ${operation}`;
}

function normalizeTransactionId(value) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 1 || number > 0xffff) {
        throw new RangeError("Candidate transaction id must be a nonzero 16-bit integer.");
    }
    return number;
}

function normalizeInteger(value, label, minimum, maximum) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < minimum || number > maximum) {
        throw new RangeError(`${label} must be an integer from ${minimum} through ${maximum}.`);
    }
    return number;
}

function assertIdSequence(value, label) {
    if (!value || typeof value.next !== "function") {
        throw new TypeError(`${label} must provide next().`);
    }
}

function copyBytes(value, label) {
    if (!(value instanceof Uint8Array)) {
        throw new TypeError(`${label} must be a Buffer or Uint8Array.`);
    }
    return Buffer.from(value);
}

function requestSignal(context) {
    return context.cancellationSuppressed ? undefined : context.signal;
}

function isAbortSignal(value) {
    return value && typeof value.aborted === "boolean"
        && typeof value.addEventListener === "function"
        && typeof value.removeEventListener === "function";
}

function defaultSleep(milliseconds) {
    return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function uploadProfileCandidate(connection, blob, options = {}) {
    const coordinator = new CandidateUploadCoordinator(connection, options);
    return coordinator.upload(blob, options);
}

async function commitPreparedCandidate(connection, transactionId, options = {}) {
    const coordinator = new CandidateUploadCoordinator(connection, options);
    return coordinator.commit(transactionId, options);
}

module.exports = {
    DEFAULT_MAX_BUSY_RESUBMISSIONS,
    DEFAULT_POLL_INTERVAL_MS,
    FIRMWARE_HOST_PRECOMMIT_TIMEOUT_MS,
    FIRMWARE_PREPARING_PEER_NO_PROGRESS_TIMEOUT_MS,
    CandidateUploadCoordinator,
    CandidateUploadError,
    UPLOAD_PHASE,
    candidateStatusStallTimeoutMs,
    commitPreparedCandidate,
    uploadProfileCandidate,
};
