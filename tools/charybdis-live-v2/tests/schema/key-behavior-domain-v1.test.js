"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    KEY_BEHAVIOR_DOMAIN_V1,
    KEY_BEHAVIOR_HOLD_MODES,
    KEY_BEHAVIOR_LIMITS,
    decodeKeyBehaviorDomain,
    decodeKeyBehaviorDomainEnvelope,
    encodeKeyBehaviorDomain,
    encodeKeyBehaviorDomainEnvelope,
} = require("../../core/schema/key-behavior-domain-v1");
const {
    PROFILE_ACTION_KINDS,
    PROFILE_DOMAIN_IDS,
    crc32,
    decodeProfileBlob,
    encodeProfileBlob,
    fnv1a32,
} = require("../../core/schema/profile-blob-v1");

function fixtures() {
    const values = new Map(fs.readFileSync(path.join(__dirname, "../../../../tests/fixtures/key_behavior_domain_v1.fixture"), "utf8")
        .split(/\r?\n/)
        .filter((line) => line && !line.startsWith("#"))
        .map((line) => {
            const separator = line.indexOf("=");
            assert.notEqual(separator, -1, `Malformed shared fixture line: ${line}`);
            return [line.slice(0, separator), line.slice(separator + 1)];
        }));
    return {
        emptyHex: values.get("payload.empty.hex"),
        representativeHex: values.get("payload.representative.hex"),
        representativeEnvelopeHex: values.get("envelope.representative.hex"),
        representativeBlobHex: values.get("blob.representative.hex"),
        representativeFnv1a32: Number.parseInt(values.get("blob.representative.fnv1a32"), 16),
        representativeCrc32: Number.parseInt(values.get("blob.representative.crc32"), 16),
    };
}

function representative() {
    return {rows: [
        {
            target: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, operand: 2},
            steps: [{tapIndex: 1, tap: {kind: PROFILE_ACTION_KINDS.HARDCODED_MACRO, operand: 2}}],
        },
        {
            target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 0x1234},
            tapHoldTerm: 150,
            longerHoldTerm: 400,
            multiTapTerm: 175,
            keepsAutoMouseAnchored: true,
            steps: [
                {
                    tapIndex: 2,
                    tap: {kind: PROFILE_ACTION_KINDS.VIA_MACRO, operand: 10},
                    longHold: {
                        mode: KEY_BEHAVIOR_HOLD_MODES.TAP_AT_HOLD_THRESHOLD,
                        action: {kind: PROFILE_ACTION_KINDS.LAYER_LOCK, operand: 3},
                    },
                },
                {
                    tapIndex: 0,
                    hold: {
                        mode: KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD,
                        repeatHz: 25,
                        action: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 0x28},
                    },
                },
            ],
        },
    ]};
}

test("empty and representative key-behavior payloads match exact golden bytes", () => {
    const golden = fixtures();
    assert.equal(encodeKeyBehaviorDomain({rows: []}).toString("hex"), golden.emptyHex);
    const payload = encodeKeyBehaviorDomain(representative());
    assert.equal(payload.toString("hex"), golden.representativeHex);
    assert.deepEqual(decodeKeyBehaviorDomain(payload), {
        rows: [
            {
                target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, flags: 0, operand: 0x1234},
                tapHoldTerm: 150,
                longerHoldTerm: 400,
                multiTapTerm: 175,
                keepsAutoMouseAnchored: true,
                steps: [
                    {
                        tapIndex: 0,
                        hold: {
                            mode: KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD,
                            repeatHz: 25,
                            action: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, flags: 0, operand: 0x28},
                        },
                    },
                    {
                        tapIndex: 2,
                        tap: {kind: PROFILE_ACTION_KINDS.VIA_MACRO, flags: 0, operand: 10},
                        longHold: {
                            mode: KEY_BEHAVIOR_HOLD_MODES.TAP_AT_HOLD_THRESHOLD,
                            repeatHz: 0,
                            action: {kind: PROFILE_ACTION_KINDS.LAYER_LOCK, flags: 0, operand: 3},
                        },
                    },
                ],
            },
            {
                target: {kind: PROFILE_ACTION_KINDS.PD_MODE_MOMENTARY, flags: 0, operand: 2},
                tapHoldTerm: 0,
                longerHoldTerm: 0,
                multiTapTerm: 0,
                keepsAutoMouseAnchored: false,
                steps: [{tapIndex: 1, tap: {kind: PROFILE_ACTION_KINDS.HARDCODED_MACRO, flags: 0, operand: 2}}],
            },
        ],
        rowCount: 2,
        populatedStepCount: 3,
        byteLength: 58,
    });
});

test("domain envelope composes with the canonical whole-profile blob", () => {
    const golden = fixtures();
    const envelope = encodeKeyBehaviorDomainEnvelope(representative());
    assert.equal(envelope.toString("hex"), golden.representativeEnvelopeHex);
    assert.equal(envelope[0], PROFILE_DOMAIN_IDS.KEY_BEHAVIORS);
    assert.deepEqual(decodeKeyBehaviorDomainEnvelope(envelope), decodeKeyBehaviorDomain(encodeKeyBehaviorDomain(representative())));
    const blob = encodeProfileBlob({domains: [{
        id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS,
        version: 1,
        payload: encodeKeyBehaviorDomain(representative()),
    }]});
    assert.equal(blob.toString("hex"), golden.representativeBlobHex);
    assert.equal(fnv1a32(blob), golden.representativeFnv1a32);
    assert.equal(crc32(blob), golden.representativeCrc32);
    assert.equal(decodeProfileBlob(blob).domains[0].payload.toString("hex"), golden.representativeHex);
});

test("encoder canonicalizes row and step order and rejects duplicate identities", () => {
    const canonical = encodeKeyBehaviorDomain(representative());
    const reordered = representative();
    reordered.rows.reverse();
    reordered.rows[1].steps.reverse();
    assert.deepEqual(encodeKeyBehaviorDomain(reordered), canonical);

    const duplicateTarget = representative();
    duplicateTarget.rows[1].target = {...duplicateTarget.rows[0].target};
    assert.throws(() => encodeKeyBehaviorDomain(duplicateTarget), (error) => error.code === "DUPLICATE_TARGET");
    const duplicateStep = representative();
    duplicateStep.rows[1].steps[1].tapIndex = duplicateStep.rows[1].steps[0].tapIndex;
    assert.throws(() => encodeKeyBehaviorDomain(duplicateStep), (error) => error.code === "DUPLICATE_STEP");
});

test("encoder enforces actions, authorable hold modes, repeat rates, and capacities", () => {
    const noneTarget = representative();
    noneTarget.rows[0].target = {kind: PROFILE_ACTION_KINDS.NONE, operand: 0};
    assert.throws(() => encodeKeyBehaviorDomain(noneTarget), (error) => error.code === "INVALID_TARGET");
    assert.throws(() => encodeKeyBehaviorDomain({rows: [{
        target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 1},
        steps: [{tapIndex: 0}],
    }]}), (error) => error.code === "EMPTY_STEP");
    assert.throws(() => encodeKeyBehaviorDomain({rows: [{
        target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 1},
        steps: [{tapIndex: 5, tap: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 2}}],
    }]}), (error) => error.code === "INVALID_TAP_INDEX");

    for (const [mode, repeatHz, code] of [
        [0, 0, "INVALID_HOLD_MODE"],
        [5, 0, "INVALID_HOLD_MODE"],
        [KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD, 0, "INVALID_REPEAT_RATE"],
        [KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD, 101, "INVALID_REPEAT_RATE"],
        [KEY_BEHAVIOR_HOLD_MODES.TAP_AT_HOLD_THRESHOLD, 1, "INVALID_REPEAT_RATE"],
    ]) {
        assert.throws(() => encodeKeyBehaviorDomain({rows: [{
            target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 1},
            steps: [{tapIndex: 0, hold: {mode, repeatHz, action: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 2}}}],
        }]}), (error) => error.code === code);
    }
    assert.throws(() => encodeKeyBehaviorDomain({rows: Array.from({length: 65}, (_, operand) => ({
        target: {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand}, steps: [],
    }))}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => encodeKeyBehaviorDomain(representative(), {maxPopulatedSteps: 2}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => encodeKeyBehaviorDomain(representative(), {maxPayloadSize: 32}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => encodeKeyBehaviorDomain(representative(), {actionLimits: {maxPdModes: 2}}), (error) => error.code === "INVALID_OPERAND");
});

test("decoder rejects noncanonical ordering, counts, flags, lengths, and truncation", () => {
    const valid = encodeKeyBehaviorDomain(representative());
    const reservedHeader = Buffer.from(valid);
    reservedHeader[2] = 1;
    assert.throws(() => decodeKeyBehaviorDomain(reservedHeader), (error) => error.code === "RESERVED_FIELDS");

    const badCount = Buffer.from(valid);
    badCount[1] -= 1;
    assert.throws(() => decodeKeyBehaviorDomain(badCount), (error) => error.code === "COUNT_MISMATCH");
    const badRowFlags = Buffer.from(valid);
    badRowFlags[16] = 0x80;
    assert.throws(() => decodeKeyBehaviorDomain(badRowFlags), (error) => error.code === "RESERVED_FLAGS");
    const emptyStep = Buffer.from(valid);
    emptyStep[19] = 0;
    assert.throws(() => decodeKeyBehaviorDomain(emptyStep), (error) => error.code === "EMPTY_STEP");
    const reservedStep = Buffer.from(valid);
    reservedStep[19] = 0x80;
    assert.throws(() => decodeKeyBehaviorDomain(reservedStep), (error) => error.code === "RESERVED_FLAGS");
    const duplicateStep = Buffer.from(valid);
    duplicateStep[26] = 0;
    assert.throws(() => decodeKeyBehaviorDomain(duplicateStep), (error) => error.code === "DUPLICATE_STEP");
    const internalHoldMode = Buffer.from(valid);
    internalHoldMode[20] = 0;
    assert.throws(() => decodeKeyBehaviorDomain(internalHoldMode), (error) => error.code === "INVALID_HOLD_MODE");

    const firstRowLength = valid.readUInt16LE(4) + KEY_BEHAVIOR_DOMAIN_V1.ROW_LENGTH_SIZE;
    const first = valid.subarray(4, 4 + firstRowLength);
    const second = valid.subarray(4 + firstRowLength);
    const outOfOrder = Buffer.concat([valid.subarray(0, 4), second, first]);
    assert.throws(() => decodeKeyBehaviorDomain(outOfOrder), (error) => error.code === "ROW_ORDER");
    const duplicateTarget = Buffer.from(valid);
    valid.copy(duplicateTarget, 4 + firstRowLength + 2, 6, 10);
    assert.throws(() => decodeKeyBehaviorDomain(duplicateTarget), (error) => error.code === "DUPLICATE_TARGET");

    assert.throws(() => decodeKeyBehaviorDomain(Buffer.concat([valid, Buffer.from([0])])), (error) => error.code === "TRAILING_BYTES");
    assert.throws(() => decodeKeyBehaviorDomain(valid.subarray(0, valid.length - 1)), (error) => error.code === "TRUNCATED");
    const wrongLength = Buffer.from(valid);
    wrongLength.writeUInt16LE(wrongLength.readUInt16LE(4) - 1, 4);
    assert.throws(() => decodeKeyBehaviorDomain(wrongLength), (error) => ["TRUNCATED", "ROW_LENGTH"].includes(error.code));
});

test("decoder honors negotiated capacity ceilings", () => {
    const payload = encodeKeyBehaviorDomain(representative());
    assert.throws(() => decodeKeyBehaviorDomain(payload, {maxRows: 1}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => decodeKeyBehaviorDomain(payload, {maxPopulatedSteps: 2}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => decodeKeyBehaviorDomain(payload, {maxTapStepsPerBehavior: 2}), (error) => error.code === "INVALID_TAP_INDEX" || error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => decodeKeyBehaviorDomain(payload, {maxPayloadSize: 32}), (error) => error.code === "CAPACITY_EXCEEDED");
});

test("negotiated behavior limits can lower but never raise frozen v1 ceilings", () => {
    const lowered = {
        maxRows: 63,
        maxPopulatedSteps: 127,
        maxTapStepsPerBehavior: 4,
        maxRepeatHz: 99,
        maxPayloadSize: KEY_BEHAVIOR_LIMITS.maxPayloadSize - 1,
    };
    assert.doesNotThrow(() => encodeKeyBehaviorDomain({rows: []}, lowered));
    for (const [field, ceiling] of [
        ["maxRows", 64],
        ["maxPopulatedSteps", 128],
        ["maxTapStepsPerBehavior", 5],
        ["maxRepeatHz", 100],
        ["maxPayloadSize", KEY_BEHAVIOR_LIMITS.maxPayloadSize],
    ]) {
        assert.throws(() => encodeKeyBehaviorDomain({rows: []}, {[field]: ceiling + 1}), RangeError);
        assert.doesNotThrow(() => encodeKeyBehaviorDomain({rows: []}, {[field]: ceiling}));
    }
});
