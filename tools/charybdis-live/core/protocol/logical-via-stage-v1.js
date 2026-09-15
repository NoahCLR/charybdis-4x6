"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("../transport/device-adapter");
const {PROFILE_WIRE_V1, buildProfileGetRequest, decodeProfileResponse, profileResponseMatcher} = require("./profile-wire-v1");

const LOGICAL_VIA_STAGE_V1 = Object.freeze({
    COMMAND_SET: 0x07,
    VALUE_BEGIN: 0x15,
    VALUE_CHUNK: 0x16,
    VALUE_VERIFY: 0x17,
    VALUE_STATUS: 0x19,
    VALUE_ABORT: 0x1a,
    CHUNK_MAX: 12,
    STATUS_LAYOUT_VERSION: 1,
    STATUS_PAYLOAD_SIZE: 18,
});

const LOGICAL_VIA_STATE = Object.freeze({IDLE: 0, STAGING: 1, STAGED: 2, ACCEPTED: 3, ABORTED: 4, ERROR: 5});
const ADMISSION = Object.freeze({QUEUED: 0, MALFORMED: 1, BUSY: 2, UNSUPPORTED: 3});

class LogicalViaStageError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "LogicalViaStageError";
        this.code = code;
        Object.assign(this, details);
    }
}

function integer(value, label, maximum, nonzero = false) {
    if (!Number.isInteger(value) || value < (nonzero ? 1 : 0) || value > maximum) throw new RangeError(`${label} is out of range.`);
    return value;
}

function header(value, transactionId) {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = LOGICAL_VIA_STAGE_V1.COMMAND_SET;
    report[1] = PROFILE_WIRE_V1.CHANNEL;
    report[2] = value;
    report.writeUInt16LE(integer(transactionId, "Logical transaction id", 0xffff, true), 3);
    return report;
}

function identityRequest(value, transactionId, generation, digest) {
    const report = header(value, transactionId);
    report.writeUInt32LE(integer(generation, "VIA generation", 0xffffffff, true), 5);
    report.writeUInt32LE(integer(digest, "VIA digest", 0xffffffff, true), 9);
    return report;
}

function buildLogicalViaBeginRequest(transactionId, generation, digest) {
    return identityRequest(LOGICAL_VIA_STAGE_V1.VALUE_BEGIN, transactionId, generation, digest);
}

function buildLogicalViaVerifyRequest(transactionId, generation, digest) {
    return identityRequest(LOGICAL_VIA_STAGE_V1.VALUE_VERIFY, transactionId, generation, digest);
}

function buildLogicalViaAbortRequest(transactionId, generation, digest) {
    return identityRequest(LOGICAL_VIA_STAGE_V1.VALUE_ABORT, transactionId, generation, digest);
}

function buildLogicalViaChunkRequest(transactionId, {region, offset, regionLength, bytes, generation, digest}) {
    const payload = Buffer.from(bytes);
    if (payload.length < 1 || payload.length > LOGICAL_VIA_STAGE_V1.CHUNK_MAX) throw new RangeError("Logical VIA chunk must contain 1 through 12 bytes.");
    integer(region, "VIA region", 4, true);
    integer(offset, "VIA offset", 0xffff);
    integer(regionLength, "VIA region length", 0xffff, true);
    if (offset >= regionLength || offset + payload.length > regionLength) throw new RangeError("Logical VIA chunk exceeds its region.");
    const report = header(LOGICAL_VIA_STAGE_V1.VALUE_CHUNK, transactionId);
    report[5] = region;
    report.writeUInt16LE(offset, 6);
    report.writeUInt16LE(regionLength, 8);
    report[10] = payload.length;
    payload.copy(report, 11);
    report.writeUInt32LE(integer(generation, "VIA generation", 0xffffffff, true), 23);
    report.writeUInt32LE(integer(digest, "VIA digest", 0xffffffff, true), 27);
    return report;
}

function buildLogicalViaStatusRequest(requestId) {
    return buildProfileGetRequest(LOGICAL_VIA_STAGE_V1.VALUE_STATUS, 0, integer(requestId, "Logical VIA status request id", 0xff, true));
}

function mutationMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "Logical VIA response");
    return actual.subarray(0, 5).equals(Buffer.from(request).subarray(0, 5));
}

function decodeLogicalViaAcknowledgement(response, request) {
    const report = Buffer.from(normalizeRawHidReport(response, "Logical VIA response"));
    if (!mutationMatcher(report, request)) throw new LogicalViaStageError("CORRELATION_MISMATCH", "Logical VIA acknowledgement does not match its request.");
    if (report.subarray(8).some(Boolean) || !Object.values(ADMISSION).includes(report[5])) throw new LogicalViaStageError("MALFORMED_RESPONSE", "Logical VIA acknowledgement is malformed.");
    if (report[5] !== ADMISSION.QUEUED) throw new LogicalViaStageError(report[5] === ADMISSION.BUSY ? "BUSY" : "REJECTED", "The keyboard did not admit the logical VIA operation.", {admission: report[5], errorId: report[6], frameOffset: report[7]});
    if (report[6] !== 0 || report[7] !== 0xff) throw new LogicalViaStageError("NONCANONICAL_RESPONSE", "Logical VIA acknowledgement contains an unexpected error.");
    return {admission: report[5]};
}

function decodeLogicalViaStatus(response, request) {
    const payload = Buffer.from(decodeProfileResponse(response, request, {allowShortPayload: true}));
    if (payload.length !== LOGICAL_VIA_STAGE_V1.STATUS_PAYLOAD_SIZE || payload[0] !== LOGICAL_VIA_STAGE_V1.STATUS_LAYOUT_VERSION || (payload[3] & ~1)) throw new LogicalViaStageError("MALFORMED_STATUS", "Logical VIA status is malformed.");
    const state = payload[1];
    if (!Object.values(LOGICAL_VIA_STATE).includes(state)) throw new LogicalViaStageError("MALFORMED_STATUS", "Logical VIA status contains an unknown state.");
    if (payload.subarray(16).some(Boolean)) throw new LogicalViaStageError("MALFORMED_STATUS", "Logical VIA status contains nonzero reserved bytes.");
    return {state, lastStatus: payload[2], pending: Boolean(payload[3]), transactionId: payload.readUInt16LE(4), operationSequence: payload.readUInt16LE(6), generation: payload.readUInt32LE(8), digest: payload.readUInt32LE(12)};
}

async function sendLogicalViaMutation(connection, request) {
    const response = await connection.request(request, {matchResponse: data => data[0] === 0xff || mutationMatcher(data, request)});
    if (response[0] === 0xff) throw new LogicalViaStageError("UNSUPPORTED", "The keyboard does not support atomic profile staging.");
    return decodeLogicalViaAcknowledgement(response, request);
}

async function readLogicalViaStatus(connection, options = {}) {
    if (typeof options.nextRequestId !== "function") throw new TypeError("nextRequestId must be a function.");
    const request = buildLogicalViaStatusRequest(options.nextRequestId());
    const response = await connection.request(request, {matchResponse: data => data[0] === 0xff || profileResponseMatcher(data, request)});
    if (response[0] === 0xff) throw new LogicalViaStageError("UNSUPPORTED", "The keyboard does not support atomic profile staging.");
    return decodeLogicalViaStatus(response, request);
}

module.exports = {ADMISSION, LOGICAL_VIA_STAGE_V1, LOGICAL_VIA_STATE, LogicalViaStageError, buildLogicalViaAbortRequest, buildLogicalViaBeginRequest, buildLogicalViaChunkRequest, buildLogicalViaStatusRequest, buildLogicalViaVerifyRequest, decodeLogicalViaAcknowledgement, decodeLogicalViaStatus, mutationMatcher, readLogicalViaStatus, sendLogicalViaMutation};
