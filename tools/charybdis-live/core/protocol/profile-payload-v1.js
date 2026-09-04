"use strict";

// Reads the committed profile payload off the keyboard.
//
// The device serves page 0 as metadata and pages 1..N as raw payload, a full
// report each. It deliberately does not stamp a sequence number on every chunk;
// generation only ever increases, so re-reading the metadata after the chunks
// proves nothing was committed in between. That check lives here, because the
// device cannot know when the host's read started.
//
// Everything arriving here is untrusted. The reassembled payload is verified
// against both the CRC and the digest the device reported before any caller
// sees it, so a truncated or corrupted transfer fails loudly instead of
// decoding into a plausible-looking profile.

const {
    PROFILE_WIRE_V1,
    ProfileWireProtocolError,
    buildProfileGetRequest,
    decodeProfileResponse,
    profileResponseMatcher,
} = require("./profile-wire-v1");
const {crc32, fnv1a32} = require("../schema/profile-blob-v1");

const PROFILE_PAYLOAD_V1 = Object.freeze({
    VALUE: 0x04,
    // The compiled defaults the firmware was built with, served through the
    // same page layout. A keyboard with nothing committed is still running
    // something, and this is it.
    COMPILED_VALUE: 0x05,
    LAYOUT_VERSION: 1,
    METADATA_PAGE: 0,
    METADATA_SIZE: 21,
    DEFAULT_GENERATION_RETRIES: 3,
    // A page index is one byte, so page 255 is the last addressable chunk.
    MAX_CHUNKS: 0xff,
});

function decodePayloadMetadata(payload) {
    const page = normalizePayload(payload, "profile payload metadata page");
    const decoded = {
        layoutVersion: page[0],
        chunkSize: page[1],
        payloadLength: readU16(page, 2),
        generation: readU32(page, 4),
        digest: readU32(page, 8),
        crc32: readU32(page, 12),
        schema: {major: page[16], minor: page[17]},
        domainMask: page[18],
        originHalf: page[19],
        flags: page[20],
    };

    assertZeroRange(page, PROFILE_PAYLOAD_V1.METADATA_SIZE, PROFILE_WIRE_V1.PAYLOAD_SIZE, "profile payload metadata page");
    if (decoded.layoutVersion !== PROFILE_PAYLOAD_V1.LAYOUT_VERSION) {
        throw protocolError("INCOMPATIBLE_RESPONSE", `Unsupported profile payload layout ${decoded.layoutVersion}.`);
    }
    if (decoded.chunkSize === 0 || decoded.chunkSize > PROFILE_WIRE_V1.PAYLOAD_SIZE) {
        throw protocolError("MALFORMED_RESPONSE", `Profile payload chunk size ${decoded.chunkSize} does not fit one report.`);
    }
    if (decoded.payloadLength === 0) {
        throw protocolError("MALFORMED_RESPONSE", "The keyboard reports a committed profile of zero bytes.");
    }
    if (chunkCount(decoded) > PROFILE_PAYLOAD_V1.MAX_CHUNKS) {
        throw protocolError(
            "INCOMPATIBLE_RESPONSE",
            `A ${decoded.payloadLength}-byte payload needs more than ${PROFILE_PAYLOAD_V1.MAX_CHUNKS} pages.`
        );
    }
    return decoded;
}

function chunkCount(metadata) {
    return Math.ceil(metadata.payloadLength / metadata.chunkSize);
}

async function readCommittedPayload(connection, options = {}) {
    return readProfilePayload(connection, PROFILE_PAYLOAD_V1.VALUE, options);
}

// The compiled defaults carry no generation, so there is nothing for the
// coherence check to compare; they cannot change while the firmware runs.
async function readCompiledPayload(connection, options = {}) {
    return readProfilePayload(connection, PROFILE_PAYLOAD_V1.COMPILED_VALUE, options);
}

async function readProfilePayload(connection, value, options = {}) {
    assertConnection(connection);
    const retries = normalizeRetryCount(options.generationRetries);
    const requestIds = createRequestIdSource(options);
    let lastMismatch;

    for (let attempt = 0; attempt <= retries; attempt += 1) {
        const metadata = decodePayloadMetadata(await requestPage(connection, value, PROFILE_PAYLOAD_V1.METADATA_PAGE, requestIds, options));
        const total = chunkCount(metadata);
        const bytes = Buffer.alloc(metadata.payloadLength);
        let written = 0;

        for (let chunk = 0; chunk < total; chunk += 1) {
            const page = await requestPage(connection, value, chunk + 1, requestIds, options);
            const expected = Math.min(metadata.chunkSize, metadata.payloadLength - written);
            if (page.length < expected) {
                throw protocolError(
                    "MALFORMED_RESPONSE",
                    `Chunk ${chunk} returned ${page.length} bytes where ${expected} were expected.`
                );
            }
            page.copy(bytes, written, 0, expected);
            written += expected;
            if (typeof options.onProgress === "function") {
                options.onProgress({done: written, total: metadata.payloadLength});
            }
        }

        // Re-read the metadata. A generation that has not moved means no commit
        // landed while the chunks were in flight.
        const after = decodePayloadMetadata(await requestPage(connection, value, PROFILE_PAYLOAD_V1.METADATA_PAGE, requestIds, options));
        if (after.generation !== metadata.generation || after.digest !== metadata.digest) {
            lastMismatch = {expectedGeneration: metadata.generation, actualGeneration: after.generation};
            continue;
        }

        verify(bytes, metadata);
        return {metadata, bytes};
    }

    throw protocolError(
        "GENERATION_UNSTABLE",
        `The committed profile changed during ${retries + 1} consecutive read attempts.`,
        {...lastMismatch, attempts: retries + 1}
    );
}

// Both checks, not one. The CRC catches transport damage; the digest is what
// the rest of the system identifies a generation by, so a mismatch there means
// this payload is not the generation the device claimed.
function verify(bytes, metadata) {
    const actualCrc = crc32(bytes);
    if (actualCrc !== metadata.crc32) {
        throw protocolError("PAYLOAD_CORRUPT", `Committed payload CRC ${hex(actualCrc)} does not match the reported ${hex(metadata.crc32)}.`, {
            expected: metadata.crc32,
            actual: actualCrc,
        });
    }
    const actualDigest = fnv1a32(bytes);
    if (actualDigest !== metadata.digest) {
        throw protocolError("PAYLOAD_CORRUPT", `Committed payload digest ${hex(actualDigest)} does not match the reported ${hex(metadata.digest)}.`, {
            expected: metadata.digest,
            actual: actualDigest,
        });
    }
}

async function requestPage(connection, value, page, requestIds, options) {
    const request = buildProfileGetRequest(value, page, requestIds.next());
    const response = await connection.request(request, {
        matchResponse: profileResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    });
    return Buffer.from(decodeProfileResponse(response, request, {allowShortPayload: true}));
}

function normalizePayload(value, label) {
    if (!(value instanceof Uint8Array) || value.byteLength < PROFILE_PAYLOAD_V1.METADATA_SIZE) {
        throw protocolError("MALFORMED_RESPONSE", `${label} is shorter than its ${PROFILE_PAYLOAD_V1.METADATA_SIZE}-byte layout.`);
    }
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function assertZeroRange(buffer, start, end, label) {
    for (let index = start; index < Math.min(end, buffer.length); index += 1) {
        if (buffer[index] !== 0) {
            throw protocolError("NONCANONICAL_RESPONSE", `${label} has nonzero reserved bytes.`);
        }
    }
}

function createRequestIdSource(options) {
    if (options.nextRequestId !== undefined) {
        if (typeof options.nextRequestId !== "function") {
            throw new TypeError("nextRequestId must be a function.");
        }
        return {next: options.nextRequestId};
    }
    let requestId = 1;
    return {
        next() {
            const current = requestId;
            requestId = requestId === 0xff ? 1 : requestId + 1;
            return current;
        },
    };
}

function normalizeRetryCount(value) {
    if (value === undefined) {
        return PROFILE_PAYLOAD_V1.DEFAULT_GENERATION_RETRIES;
    }
    if (!Number.isSafeInteger(value) || value < 0 || value > 10) {
        throw new TypeError("generationRetries must be an integer from 0 through 10.");
    }
    return value;
}

function assertConnection(connection) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
}

function readU16(buffer, offset) {
    return buffer[offset] | (buffer[offset + 1] << 8);
}

function readU32(buffer, offset) {
    return (buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24)) >>> 0;
}

function hex(value) {
    return `0x${(value >>> 0).toString(16).toUpperCase().padStart(8, "0")}`;
}

function protocolError(code, message, details = {}) {
    return new ProfileWireProtocolError(code, message, details);
}

module.exports = {
    PROFILE_PAYLOAD_V1,
    chunkCount,
    decodePayloadMetadata,
    readCommittedPayload,
    readCompiledPayload,
};
