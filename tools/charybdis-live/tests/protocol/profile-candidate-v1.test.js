"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../../core/schema/profile-blob-v1");
const {
    CANDIDATE_ADMISSION,
    CANDIDATE_ERROR,
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    CandidateRequestIdSequence,
    CandidateTransactionIdSequence,
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
    readCandidateStatus,
} = require("../../core/protocol/profile-candidate-v1");

function goldenFixtures() {
    const fixturePath = path.resolve(__dirname, "../../../../tests/fixtures/profile_candidate_v1.fixture");
    const entries = {};
    for (const line of fs.readFileSync(fixturePath, "utf8").split(/\r?\n/)) {
        if (!line || line.startsWith("#")) continue;
        const [name, hex] = line.split("=");
        entries[name] = Buffer.from(hex, "hex");
    }
    return entries;
}

test("JavaScript emits the exact candidate frames consumed by the firmware fixture", () => {
    assert.equal(CANDIDATE_ERROR.TIMEOUT, 19);
    assert.equal(CANDIDATE_ERROR.PEER_SUPERSEDED, 20);
    assert.equal(CANDIDATE_ERROR.PEER_PREPARE_YIELDED, 21);
    assert.equal(CANDIDATE_ERROR.POSTCOMMIT_AUTHORITY_LOST, 22);
    assert.equal(CANDIDATE_ERROR.PEER_COMMIT_CONFLICT, 23);
    assert.equal(CANDIDATE_STATE.PREPARING_PEER, 8);
    assert.equal(CANDIDATE_STATE.CONVERGING_PEER, 9);
    const fixtures = goldenFixtures();
    const metadata = {
        schemaMajor: 1,
        schemaMinor: 0,
        requestedDomains: 3,
        flags: 0,
        payloadLength: 8,
        crc32: 0x11223344,
        digest: 0x88776655,
        actionAbiDigest: 0xccbbaa99,
    };
    assert.deepEqual(buildCandidateBeginRequest(0x1234, metadata), fixtures["begin-request"]);
    assert.deepEqual(
        buildCandidateChunkRequest(0x1234, 0, Buffer.from("4e4c503101000001", "hex")),
        fixtures["chunk-request"]
    );
    assert.deepEqual(buildCandidateValidateRequest(0x1234), fixtures["validate-request"]);
    assert.deepEqual(buildCandidateCommitRequest(0x1234), fixtures["commit-request"]);
    assert.deepEqual(buildCandidateAbortRequest(0x1234), fixtures["abort-request"]);
    assert.deepEqual(buildCandidateStatusRequest(0x41), fixtures["operation-status-request"]);
    for (const report of [
        buildCandidateBeginRequest(0x1234, metadata),
        buildCandidateChunkRequest(0x1234, 0, Buffer.alloc(20)),
        buildCandidateValidateRequest(0x1234),
        buildCandidateCommitRequest(0x1234),
        buildCandidateAbortRequest(0x1234),
        buildCandidateStatusRequest(0x41),
    ]) {
        assert.equal(report.length, 32);
    }
});

test("commit uses custom-save framing and the shared acknowledgement contract", () => {
    const fixtures = goldenFixtures();
    const request = buildCandidateCommitRequest(0x1234);
    assert.equal(request[0], 0x09);
    assert.equal(request[2], 0x13);
    assert.deepEqual(request, fixtures["commit-request"]);
    assert.deepEqual(decodeCandidateAcknowledgement(fixtures["commit-queued-ack"], request), {
        admission: CANDIDATE_ADMISSION.QUEUED,
        admissionName: "QUEUED",
        errorId: CANDIDATE_ERROR.NONE,
        errorName: "NONE",
        frameOffset: 0xff,
        transactionId: 0x1234,
        valueId: 0x13,
    });
});

test("mutation acknowledgment correlation includes operation and both transaction bytes", () => {
    const fixtures = goldenFixtures();
    const request = fixtures["begin-request"];
    const queued = decodeCandidateAcknowledgement(fixtures["begin-queued-ack"], request);
    assert.deepEqual(queued, {
        admission: CANDIDATE_ADMISSION.QUEUED,
        admissionName: "QUEUED",
        errorId: CANDIDATE_ERROR.NONE,
        errorName: "NONE",
        frameOffset: 0xff,
        transactionId: 0x1234,
        valueId: 0x10,
    });
    const malformed = decodeCandidateAcknowledgement(fixtures["begin-malformed-ack"], request);
    assert.equal(malformed.admission, CANDIDATE_ADMISSION.MALFORMED);
    assert.equal(malformed.errorId, CANDIDATE_ERROR.MALFORMED_FRAME);
    assert.equal(malformed.frameOffset, 8);

    for (const offset of [2, 3, 4]) {
        const mismatch = Buffer.from(fixtures["begin-queued-ack"]);
        mismatch[offset] ^= 1;
        assert.equal(candidateMutationResponseMatcher(mismatch, request), false);
        assert.throws(
            () => decodeCandidateAcknowledgement(mismatch, request),
            (error) => error.code === "CORRELATION_MISMATCH"
        );
    }
});

test("candidate status uses normal request correlation and decodes structured locations", async () => {
    const fixtures = goldenFixtures();
    const request = fixtures["operation-status-request"];
    assert.deepEqual(decodeCandidateStatusResponse(fixtures["operation-status-response"], request), {
        layoutVersion: 1,
        state: CANDIDATE_STATE.REJECTED,
        lastOperation: CANDIDATE_OPERATION.VALIDATE,
        flags: 3,
        mailboxPending: true,
        poisoned: true,
        transactionId: 0x1234,
        nextOffset: 8,
        payloadLength: 8,
        digest: 0x88776655,
        error: {
            id: CANDIDATE_ERROR.VALIDATION_REJECTED,
            name: "VALIDATION_REJECTED",
            domainId: 0x20,
            tableId: 2,
            rowIndex: 5,
            tapIndex: 3,
            fieldId: 4,
            byteOffset: 0x1122,
        },
        operationSequence: 0x3344,
    });

    const connection = {
        async request(actual, options) {
            assert.deepEqual(actual, request);
            assert.equal(options.matchResponse(fixtures["operation-status-response"], actual), true);
            return fixtures["operation-status-response"];
        },
    };
    assert.equal((await readCandidateStatus(connection, {requestId: 0x41})).transactionId, 0x1234);

    const commit = decodeCandidateStatusResponse(fixtures["commit-status-response"], request);
    assert.equal(commit.state, CANDIDATE_STATE.ACTIVATING);
    assert.equal(commit.lastOperation, CANDIDATE_OPERATION.COMMIT);
    assert.equal(commit.transactionId, 0x1234);
    assert.equal(commit.digest, 0x88776655);
    assert.equal(commit.error.id, CANDIDATE_ERROR.NONE);
    assert.equal(commit.operationSequence, 0x3345);
});

test("candidate codecs reject noncanonical padding, invalid bounds, and unknown status values", () => {
    const fixtures = goldenFixtures();
    assert.throws(() => buildCandidateChunkRequest(1, 0, Buffer.alloc(0)), /1 through 20/);
    assert.throws(() => buildCandidateChunkRequest(1, 4050, Buffer.alloc(20)), /end at or before/);
    assert.throws(() => buildCandidateAbortRequest(0), /nonzero/);

    const badAck = Buffer.from(fixtures["begin-queued-ack"]);
    badAck[31] = 1;
    assert.throws(() => decodeCandidateAcknowledgement(badAck, fixtures["begin-request"]), (error) => error.code === "NONCANONICAL_RESPONSE");
    const badQueued = Buffer.from(fixtures["begin-queued-ack"]);
    badQueued[7] = 0;
    assert.throws(() => decodeCandidateAcknowledgement(badQueued, fixtures["begin-request"]), /queued acknowledgment/);

    const badStatus = Buffer.from(fixtures["operation-status-response"]);
    badStatus[8] = 11;
    assert.throws(() => decodeCandidateStatusResponse(badStatus, fixtures["operation-status-request"]), /Unknown candidate state/);

    const preparing = Buffer.from(fixtures["commit-status-response"]);
    preparing[8] = CANDIDATE_STATE.PREPARING_PEER;
    assert.equal(decodeCandidateStatusResponse(preparing, fixtures["operation-status-request"]).state, CANDIDATE_STATE.PREPARING_PEER);
    const converging = Buffer.from(preparing);
    converging[8] = CANDIDATE_STATE.CONVERGING_PEER;
    assert.equal(decodeCandidateStatusResponse(converging, fixtures["operation-status-request"]).state, CANDIDATE_STATE.CONVERGING_PEER);
    const failed = Buffer.from(preparing);
    failed[8] = CANDIDATE_STATE.AUTHORITY_FAILED;
    assert.equal(decodeCandidateStatusResponse(failed, fixtures["operation-status-request"]).state, CANDIDATE_STATE.AUTHORITY_FAILED);
});

test("metadata is derived from the canonical blob and its exact domain mask", () => {
    const blob = encodeProfileBlob({
        domains: [{id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: Buffer.alloc(3)}],
    });
    const metadata = candidateMetadataForBlob(blob, {actionAbiDigest: 0x12345678});
    assert.equal(metadata.schemaMajor, 1);
    assert.equal(metadata.schemaMinor, 0);
    assert.equal(metadata.requestedDomains, 1);
    assert.equal(metadata.payloadLength, blob.length);
    assert.equal(metadata.actionAbiDigest, 0x12345678);
    assert.equal(metadata.storeFormatVersion, 0);
    assert.notEqual(metadata.crc32, metadata.digest);
    assert.throws(
        () => candidateMetadataForBlob(blob, {actionAbiDigest: 1, requestedDomains: 2}),
        (error) => error.code === "DOMAIN_MASK_MISMATCH"
    );
});

test("logical candidate begin binds the target VIA identity", () => {
    const blob = encodeProfileBlob({domains: []});
    const metadata = candidateMetadataForBlob(blob, {
        actionAbiDigest: 0x12345678,
        viaGeneration: 9,
        viaDigest: 0x89abcdef,
    });
    const report = buildCandidateBeginRequest(0x4321, metadata);
    assert.equal(report[23], 2);
    assert.equal(report.readUInt32LE(24), 9);
    assert.equal(report.readUInt32LE(28), 0x89abcdef);
    assert.throws(() => buildCandidateBeginRequest(1, {...metadata, viaDigest: 0}), /nonzero VIA/);
    assert.throws(() => buildCandidateBeginRequest(1, {...metadata, storeFormatVersion: 0}), /legacy candidates/);
});

test("request and transaction id allocators wrap without emitting zero", () => {
    const requests = new CandidateRequestIdSequence(0xfe);
    const transactions = new CandidateTransactionIdSequence(0xfffe);
    assert.deepEqual([requests.next(), requests.next(), requests.next()], [0xfe, 0xff, 1]);
    assert.deepEqual([transactions.next(), transactions.next(), transactions.next()], [0xfffe, 0xffff, 1]);
    assert.throws(() => new CandidateRequestIdSequence(0), /nonzero/);
    assert.throws(() => new CandidateTransactionIdSequence(0), /nonzero/);
});
