"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("../transport/device-adapter");
const {crc32, decodeProfileBlob, fnv1a32, PROFILE_DOMAIN_IDS} = require("../schema/profile-blob-v1");
const {
    PROFILE_WIRE_DOMAINS,
    PROFILE_WIRE_V1,
    buildProfileGetRequest,
    decodeProfileResponse,
    profileResponseMatcher,
} = require("./profile-wire-v1");

const PROFILE_CANDIDATE_V1 = Object.freeze({
    COMMAND_SET: 0x07,
    COMMAND_SAVE: 0x09,
    CHANNEL: PROFILE_WIRE_V1.CHANNEL,
    VALUE_BEGIN: 0x10,
    VALUE_CHUNK: 0x11,
    VALUE_VALIDATE: 0x12,
    VALUE_COMMIT: 0x13,
    VALUE_ABORT: 0x14,
    VALUE_STATUS: 0x18,
    CHUNK_MAX: 20,
    MIN_BLOB_SIZE: 8,
    MAX_BLOB_SIZE: 4064,
    STATUS_LAYOUT_VERSION: 1,
    STATUS_PAYLOAD_SIZE: 25,
    LOGICAL_STORE_FORMAT: 2,
    KNOWN_DOMAIN_MASK: PROFILE_WIRE_DOMAINS.RGB | PROFILE_WIRE_DOMAINS.KEY_BEHAVIORS | PROFILE_WIRE_DOMAINS.COMBOS | PROFILE_WIRE_DOMAINS.SETTINGS,
});

const CANDIDATE_ADMISSION = Object.freeze({
    QUEUED: 0,
    MALFORMED: 1,
    BUSY: 2,
    UNSUPPORTED: 3,
});

const CANDIDATE_STATE = Object.freeze({
    IDLE: 0,
    RECEIVING: 1,
    COMPLETE: 2,
    VALIDATING: 3,
    VALIDATED: 4,
    REJECTED: 5,
    COMMITTING: 6,
    ACTIVATING: 7,
    PREPARING_PEER: 8,
    CONVERGING_PEER: 9,
    AUTHORITY_FAILED: 10,
});

const CANDIDATE_STATE_NAMES = Object.freeze(Object.fromEntries(
    Object.entries(CANDIDATE_STATE).map(([name, value]) => [value, name])
));

const CANDIDATE_OPERATION = Object.freeze({
    NONE: 0,
    BEGIN: 1,
    CHUNK: 2,
    VALIDATE: 3,
    ABORT: 4,
    COMMIT: 5,
});

const CANDIDATE_STATUS_FLAGS = Object.freeze({
    MAILBOX_PENDING: 1 << 0,
    POISONED: 1 << 1,
    KNOWN_MASK: (1 << 0) | (1 << 1),
});

const CANDIDATE_ERROR = Object.freeze({
    NONE: 0,
    MALFORMED_FRAME: 1,
    MAILBOX_BUSY: 2,
    INVALID_TRANSACTION: 3,
    WRONG_TRANSACTION: 4,
    INVALID_STATE: 5,
    INCOMPATIBLE_SCHEMA: 6,
    UNSUPPORTED_DOMAIN: 7,
    INCOMPATIBLE_ACTION_ABI: 8,
    CAPACITY_EXCEEDED: 9,
    OUT_OF_ORDER: 10,
    CONFLICTING_RETRY: 11,
    CHECKSUM_MISMATCH: 12,
    STORAGE_FAILURE: 13,
    VALIDATION_REJECTED: 14,
    UNSUPPORTED_OPERATION: 15,
    POISONED: 16,
    ACTIVATION_FAILED: 17,
    DURABILITY_UNKNOWN: 18,
    TIMEOUT: 19,
    PEER_SUPERSEDED: 20,
    PEER_PREPARE_YIELDED: 21,
    POSTCOMMIT_AUTHORITY_LOST: 22,
    PEER_COMMIT_CONFLICT: 23,
});

const CANDIDATE_ERROR_NAMES = Object.freeze(Object.fromEntries(
    Object.entries(CANDIDATE_ERROR).map(([name, value]) => [value, name])
));

const CANDIDATE_ADMISSION_NAMES = Object.freeze(Object.fromEntries(
    Object.entries(CANDIDATE_ADMISSION).map(([name, value]) => [value, name])
));

class ProfileCandidateProtocolError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "ProfileCandidateProtocolError";
        this.code = code;
        Object.assign(this, details);
    }
}

class CandidateTransactionIdSequence {
    constructor(start = 1) {
        this.value = assertU16(start, "Candidate transaction id", {nonzero: true});
    }

    next() {
        const current = this.value;
        this.value = current === 0xffff ? 1 : current + 1;
        return current;
    }
}

class CandidateRequestIdSequence {
    constructor(start = 1) {
        this.value = assertU8(start, "Candidate status request id", {nonzero: true});
    }

    next() {
        const current = this.value;
        this.value = current === 0xff ? 1 : current + 1;
        return current;
    }
}

function buildCandidateBeginRequest(transactionId, metadata) {
    const normalizedTransactionId = assertU16(transactionId, "Candidate transaction id", {nonzero: true});
    const normalized = normalizeCandidateMetadata(metadata);
    const report = buildMutationHeader(PROFILE_CANDIDATE_V1.VALUE_BEGIN, normalizedTransactionId);
    report[5] = normalized.schemaMajor;
    report[6] = normalized.schemaMinor;
    report[7] = normalized.requestedDomains;
    report[8] = normalized.flags;
    report.writeUInt16LE(normalized.payloadLength, 9);
    report.writeUInt32LE(normalized.crc32, 11);
    report.writeUInt32LE(normalized.digest, 15);
    report.writeUInt32LE(normalized.actionAbiDigest, 19);
    report[23] = normalized.storeFormatVersion;
    if (normalized.storeFormatVersion === PROFILE_CANDIDATE_V1.LOGICAL_STORE_FORMAT) {
        report.writeUInt32LE(normalized.viaGeneration, 24);
        report.writeUInt32LE(normalized.viaDigest, 28);
    }
    return report;
}

function buildCandidateChunkRequest(transactionId, offset, value) {
    const normalizedTransactionId = assertU16(transactionId, "Candidate transaction id", {nonzero: true});
    const normalizedOffset = assertU16(offset, "Candidate chunk offset");
    const bytes = copyBytes(value, "Candidate chunk");
    if (bytes.length < 1 || bytes.length > PROFILE_CANDIDATE_V1.CHUNK_MAX) {
        throw new RangeError(`Candidate chunk must contain 1 through ${PROFILE_CANDIDATE_V1.CHUNK_MAX} bytes.`);
    }
    if (normalizedOffset >= PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE || normalizedOffset + bytes.length > PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE) {
        throw new RangeError(`Candidate chunk must end at or before byte ${PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE}.`);
    }
    const report = buildMutationHeader(PROFILE_CANDIDATE_V1.VALUE_CHUNK, normalizedTransactionId);
    report.writeUInt16LE(normalizedOffset, 5);
    report[7] = bytes.length;
    bytes.copy(report, 8);
    return report;
}

function buildCandidateValidateRequest(transactionId) {
    return buildMutationHeader(
        PROFILE_CANDIDATE_V1.VALUE_VALIDATE,
        assertU16(transactionId, "Candidate transaction id", {nonzero: true})
    );
}

function buildCandidateCommitRequest(transactionId) {
    return buildMutationHeader(
        PROFILE_CANDIDATE_V1.VALUE_COMMIT,
        assertU16(transactionId, "Candidate transaction id", {nonzero: true}),
        PROFILE_CANDIDATE_V1.COMMAND_SAVE
    );
}

function buildCandidateAbortRequest(transactionId) {
    return buildMutationHeader(
        PROFILE_CANDIDATE_V1.VALUE_ABORT,
        assertU16(transactionId, "Candidate transaction id", {nonzero: true})
    );
}

function buildCandidateStatusRequest(requestId) {
    return buildProfileGetRequest(
        PROFILE_CANDIDATE_V1.VALUE_STATUS,
        0,
        assertU8(requestId, "Candidate status request id", {nonzero: true})
    );
}

function candidateMutationResponseMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "Candidate mutation response");
    const expected = normalizeRawHidReport(request, "Candidate mutation request");
    return actual[0] === expected[0]
        && actual[1] === expected[1]
        && actual[2] === expected[2]
        && actual[3] === expected[3]
        && actual[4] === expected[4];
}

function decodeCandidateAcknowledgement(response, request) {
    const report = Buffer.from(normalizeRawHidReport(response, "Candidate mutation response"));
    if (!candidateMutationResponseMatcher(report, request)) {
        throw candidateProtocolError(
            "CORRELATION_MISMATCH",
            "Candidate acknowledgment does not match the operation and transaction id."
        );
    }
    assertZeroRange(report, 8, RAW_HID_REPORT_SIZE, "Candidate acknowledgment");
    const admission = report[5];
    const errorId = report[6];
    const frameOffset = report[7];
    if (!Object.values(CANDIDATE_ADMISSION).includes(admission)) {
        throw candidateProtocolError("MALFORMED_RESPONSE", `Unknown candidate admission id ${admission}.`);
    }
    assertKnownCandidateError(errorId);
    if (admission === CANDIDATE_ADMISSION.QUEUED && (errorId !== CANDIDATE_ERROR.NONE || frameOffset !== 0xff)) {
        throw candidateProtocolError("NONCANONICAL_RESPONSE", "A queued acknowledgment must not contain an error location.");
    }
    if (admission === CANDIDATE_ADMISSION.BUSY && (errorId !== CANDIDATE_ERROR.MAILBOX_BUSY || frameOffset !== 0xff)) {
        throw candidateProtocolError("NONCANONICAL_RESPONSE", "A busy acknowledgment must report only the mailbox-busy error.");
    }
    if (admission === CANDIDATE_ADMISSION.MALFORMED
        && (errorId !== CANDIDATE_ERROR.MALFORMED_FRAME || frameOffset >= RAW_HID_REPORT_SIZE)) {
        throw candidateProtocolError("NONCANONICAL_RESPONSE", "A malformed acknowledgment must identify a malformed frame byte.");
    }
    if (admission === CANDIDATE_ADMISSION.UNSUPPORTED
        && (errorId !== CANDIDATE_ERROR.UNSUPPORTED_OPERATION || frameOffset !== 0xff)) {
        throw candidateProtocolError("NONCANONICAL_RESPONSE", "An unsupported acknowledgment must report only the unsupported-operation error.");
    }
    return {
        admission,
        admissionName: CANDIDATE_ADMISSION_NAMES[admission],
        errorId,
        errorName: CANDIDATE_ERROR_NAMES[errorId],
        frameOffset,
        transactionId: report.readUInt16LE(3),
        valueId: report[2],
    };
}

function decodeCandidateStatusResponse(response, request) {
    const payload = decodeProfileResponse(response, request);
    if (payload.length !== PROFILE_CANDIDATE_V1.STATUS_PAYLOAD_SIZE) {
        throw candidateProtocolError("MALFORMED_RESPONSE", "Candidate status must contain exactly 25 payload bytes.");
    }
    if (payload[0] !== PROFILE_CANDIDATE_V1.STATUS_LAYOUT_VERSION) {
        throw candidateProtocolError("INCOMPATIBLE_RESPONSE", `Unsupported candidate status layout ${payload[0]}.`);
    }
    const state = payload[1];
    const lastOperation = payload[2];
    const flags = payload[3];
    const errorId = payload[14];
    if (!Object.values(CANDIDATE_STATE).includes(state)) {
        throw candidateProtocolError("INCOMPATIBLE_RESPONSE", `Unknown candidate state ${state}.`);
    }
    if (!Object.values(CANDIDATE_OPERATION).includes(lastOperation)) {
        throw candidateProtocolError("INCOMPATIBLE_RESPONSE", `Unknown candidate operation ${lastOperation}.`);
    }
    if ((flags & ~CANDIDATE_STATUS_FLAGS.KNOWN_MASK) !== 0) {
        throw candidateProtocolError("INCOMPATIBLE_RESPONSE", `Candidate status contains unknown flag bits 0x${(flags & ~CANDIDATE_STATUS_FLAGS.KNOWN_MASK).toString(16)}.`);
    }
    assertKnownCandidateError(errorId);
    const error = {
        id: errorId,
        name: CANDIDATE_ERROR_NAMES[errorId],
        domainId: payload[15],
        tableId: payload[16],
        rowIndex: payload.readUInt16LE(17),
        tapIndex: payload[19],
        fieldId: payload[20],
        byteOffset: payload.readUInt16LE(21),
    };
    if (errorId === CANDIDATE_ERROR.NONE && (
        error.domainId !== 0xff
        || error.tableId !== 0xff
        || error.rowIndex !== 0xffff
        || error.tapIndex !== 0xff
        || error.fieldId !== 0xff
        || error.byteOffset !== 0xffff
    )) {
        throw candidateProtocolError("NONCANONICAL_RESPONSE", "Candidate status without an error must use all location sentinels.");
    }
    return {
        layoutVersion: payload[0],
        state,
        lastOperation,
        flags,
        mailboxPending: (flags & CANDIDATE_STATUS_FLAGS.MAILBOX_PENDING) !== 0,
        poisoned: (flags & CANDIDATE_STATUS_FLAGS.POISONED) !== 0,
        transactionId: payload.readUInt16LE(4),
        nextOffset: payload.readUInt16LE(6),
        payloadLength: payload.readUInt16LE(8),
        digest: payload.readUInt32LE(10),
        error,
        operationSequence: payload.readUInt16LE(23),
    };
}

async function readCandidateStatus(connection, options = {}) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
    const requestId = options.nextRequestId
        ? assertNextRequestId(options.nextRequestId)
        : assertU8(options.requestId === undefined ? 1 : options.requestId, "Candidate status request id", {nonzero: true});
    const request = buildCandidateStatusRequest(requestId);
    const response = await connection.request(request, {
        matchResponse: profileResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    });
    return decodeCandidateStatusResponse(response, request);
}

function candidateMetadataForBlob(value, options = {}) {
    const blob = copyBytes(value, "Candidate profile blob");
    if (blob.length < PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE || blob.length > PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE) {
        throw new RangeError(`Candidate profile blob must contain ${PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE} through ${PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE} bytes.`);
    }
    const decoded = decodeProfileBlob(blob);
    let derivedDomains = 0;
    for (const domain of decoded.domains) {
        if (domain.id === PROFILE_DOMAIN_IDS.RGB) derivedDomains |= PROFILE_WIRE_DOMAINS.RGB;
        if (domain.id === PROFILE_DOMAIN_IDS.KEY_BEHAVIORS) derivedDomains |= PROFILE_WIRE_DOMAINS.KEY_BEHAVIORS;
        if (domain.id === PROFILE_DOMAIN_IDS.SETTINGS) derivedDomains |= PROFILE_WIRE_DOMAINS.SETTINGS;
        if (domain.id === PROFILE_DOMAIN_IDS.COMBOS) derivedDomains |= PROFILE_WIRE_DOMAINS.COMBOS;
    }
    const requestedDomains = options.requestedDomains === undefined
        ? derivedDomains
        : assertDomainMask(options.requestedDomains);
    if (requestedDomains !== derivedDomains) {
        throw candidateProtocolError(
            "DOMAIN_MASK_MISMATCH",
            `Requested domain mask 0x${requestedDomains.toString(16)} does not match blob domains 0x${derivedDomains.toString(16)}.`,
            {derivedDomains, requestedDomains}
        );
    }
    return {
        schemaMajor: decoded.schema.major,
        schemaMinor: decoded.schema.minor,
        requestedDomains,
        flags: 0,
        payloadLength: blob.length,
        crc32: crc32(blob),
        digest: fnv1a32(blob),
        actionAbiDigest: assertU32(options.actionAbiDigest, "Action-ABI digest"),
        storeFormatVersion: options.viaGeneration === undefined && options.viaDigest === undefined ? 0 : PROFILE_CANDIDATE_V1.LOGICAL_STORE_FORMAT,
        viaGeneration: options.viaGeneration === undefined ? 0 : assertU32(options.viaGeneration, "VIA generation"),
        viaDigest: options.viaDigest === undefined ? 0 : assertU32(options.viaDigest, "VIA digest"),
    };
}

function normalizeCandidateMetadata(metadata) {
    if (!metadata || typeof metadata !== "object") {
        throw new TypeError("Candidate metadata must be an object.");
    }
    const flags = assertU8(metadata.flags === undefined ? 0 : metadata.flags, "Candidate flags");
    if (flags !== 0) {
        throw new RangeError("Candidate flags must be zero for Profile Wire v1.");
    }
    const payloadLength = assertU16(metadata.payloadLength, "Candidate payload length");
    if (payloadLength < PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE || payloadLength > PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE) {
        throw new RangeError(`Candidate payload length must be ${PROFILE_CANDIDATE_V1.MIN_BLOB_SIZE} through ${PROFILE_CANDIDATE_V1.MAX_BLOB_SIZE}.`);
    }
    const storeFormatVersion = metadata.storeFormatVersion === undefined ? 0 : assertU8(metadata.storeFormatVersion, "Candidate store format");
    if (storeFormatVersion !== 0 && storeFormatVersion !== PROFILE_CANDIDATE_V1.LOGICAL_STORE_FORMAT) {
        throw new RangeError(`Candidate store format must be zero or ${PROFILE_CANDIDATE_V1.LOGICAL_STORE_FORMAT}.`);
    }
    const viaGeneration = metadata.viaGeneration === undefined ? 0 : assertU32(metadata.viaGeneration, "VIA generation");
    const viaDigest = metadata.viaDigest === undefined ? 0 : assertU32(metadata.viaDigest, "VIA digest");
    if (storeFormatVersion === 0 ? (viaGeneration !== 0 || viaDigest !== 0) : (viaGeneration === 0 || viaDigest === 0)) {
        throw new RangeError("Logical candidates require a nonzero VIA generation and digest; legacy candidates require both to be zero.");
    }
    return {
        schemaMajor: assertU8(metadata.schemaMajor, "Candidate schema major"),
        schemaMinor: assertU8(metadata.schemaMinor, "Candidate schema minor"),
        requestedDomains: assertDomainMask(metadata.requestedDomains),
        flags,
        payloadLength,
        crc32: assertU32(metadata.crc32, "Candidate CRC32"),
        digest: assertU32(metadata.digest, "Candidate digest"),
        actionAbiDigest: assertU32(metadata.actionAbiDigest, "Candidate action-ABI digest"),
        storeFormatVersion,
        viaGeneration,
        viaDigest,
    };
}

function buildMutationHeader(valueId, transactionId, command = PROFILE_CANDIDATE_V1.COMMAND_SET) {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = command;
    report[1] = PROFILE_CANDIDATE_V1.CHANNEL;
    report[2] = valueId;
    report.writeUInt16LE(transactionId, 3);
    return report;
}

function assertDomainMask(value) {
    const mask = assertU8(value, "Candidate requested-domain mask");
    if ((mask & ~PROFILE_CANDIDATE_V1.KNOWN_DOMAIN_MASK) !== 0) {
        throw new RangeError(`Candidate requested-domain mask contains unknown bits 0x${(mask & ~PROFILE_CANDIDATE_V1.KNOWN_DOMAIN_MASK).toString(16)}.`);
    }
    return mask;
}

function assertKnownCandidateError(value) {
    if (!Object.prototype.hasOwnProperty.call(CANDIDATE_ERROR_NAMES, value)) {
        throw candidateProtocolError("INCOMPATIBLE_RESPONSE", `Unknown candidate error id ${value}.`);
    }
}

function assertNextRequestId(nextRequestId) {
    if (typeof nextRequestId !== "function") {
        throw new TypeError("nextRequestId must be a function.");
    }
    return assertU8(nextRequestId(), "Candidate status request id", {nonzero: true});
}

function assertZeroRange(buffer, start, end, label) {
    for (let index = start; index < end; index += 1) {
        if (buffer[index] !== 0) {
            throw candidateProtocolError("NONCANONICAL_RESPONSE", `${label} has nonzero reserved byte ${index}.`, {frameOffset: index});
        }
    }
}

function copyBytes(value, label) {
    if (!(value instanceof Uint8Array)) {
        throw new TypeError(`${label} must be a Buffer or Uint8Array.`);
    }
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function assertU8(value, label, options = {}) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < (options.nonzero ? 1 : 0) || number > 0xff) {
        throw new RangeError(`${label} must be ${options.nonzero ? "a nonzero " : "an "}8-bit integer.`);
    }
    return number;
}

function assertU16(value, label, options = {}) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < (options.nonzero ? 1 : 0) || number > 0xffff) {
        throw new RangeError(`${label} must be ${options.nonzero ? "a nonzero " : "an "}16-bit integer.`);
    }
    return number;
}

function assertU32(value, label) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > 0xffffffff) {
        throw new RangeError(`${label} must be an unsigned 32-bit integer.`);
    }
    return number >>> 0;
}

function candidateProtocolError(code, message, details) {
    return new ProfileCandidateProtocolError(code, message, details);
}

module.exports = {
    CANDIDATE_ADMISSION,
    CANDIDATE_ADMISSION_NAMES,
    CANDIDATE_ERROR,
    CANDIDATE_ERROR_NAMES,
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    CANDIDATE_STATE_NAMES,
    CANDIDATE_STATUS_FLAGS,
    CandidateRequestIdSequence,
    CandidateTransactionIdSequence,
    PROFILE_CANDIDATE_V1,
    ProfileCandidateProtocolError,
    buildCandidateAbortRequest,
    buildCandidateBeginRequest,
    buildCandidateChunkRequest,
    buildCandidateCommitRequest,
    buildCandidateStatusRequest,
    buildCandidateValidateRequest,
    candidateMetadataForBlob,
    candidateMutationResponseMatcher,
    decodeCandidateAcknowledgement,
    decodeCandidateStatusResponse,
    normalizeCandidateMetadata,
    readCandidateStatus,
};
