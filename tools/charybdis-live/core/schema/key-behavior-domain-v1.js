"use strict";

const {
    PROFILE_ACTION_KINDS,
    PROFILE_BLOB_V1,
    PROFILE_DOMAIN_IDS,
    ProfileBlobProtocolError,
    decodeDomainEnvelope,
    decodeSemanticAction,
    encodeDomainEnvelope,
    encodeSemanticAction,
    readSemanticAction,
} = require("./profile-blob-v1");

const KEY_BEHAVIOR_DOMAIN_V1 = Object.freeze({
    VERSION: 1,
    HEADER_SIZE: 4,
    ROW_LENGTH_SIZE: 2,
    ROW_FIXED_SIZE: 12,
    STEP_HEADER_SIZE: 2,
    HOLD_SIZE: 6,
    ROW_FLAG_KEEPS_AUTO_MOUSE_ANCHORED: 1 << 0,
    KNOWN_ROW_FLAGS: 1 << 0,
    STEP_HAS_TAP: 1 << 0,
    STEP_HAS_HOLD: 1 << 1,
    STEP_HAS_LONG_HOLD: 1 << 2,
    KNOWN_STEP_MASK: 0x07,
});

const KEY_BEHAVIOR_HOLD_MODES = Object.freeze({
    PRESS_AND_HOLD_UNTIL_RELEASE: 1,
    TAP_AT_HOLD_THRESHOLD: 2,
    REPEAT_WHILE_HELD: 3,
    TAP_ON_RELEASE_AFTER_HOLD: 4,
});

const KEY_BEHAVIOR_LIMITS = Object.freeze({
    maxRows: 64,
    maxPopulatedSteps: 128,
    maxTapStepsPerBehavior: 5,
    maxRepeatHz: 100,
    maxPayloadSize: PROFILE_BLOB_V1.MAX_SIZE - PROFILE_BLOB_V1.HEADER_SIZE - PROFILE_BLOB_V1.DOMAIN_HEADER_SIZE,
});

function encodeKeyBehaviorDomain(value = {}, options = {}) {
    const limits = normalizeLimits(options.limits || options);
    if (!value || typeof value !== "object" || !Array.isArray(value.rows)) {
        throw new TypeError("Key-behavior domain must provide a rows array.");
    }
    if (value.rows.length > limits.maxRows) {
        throw behaviorError("CAPACITY_EXCEEDED", `Key-behavior row count exceeds ${limits.maxRows}.`);
    }

    const rows = value.rows.map((row, rowIndex) => normalizeRow(row, limits, options.actionLimits, rowIndex));
    rows.sort((left, right) => Buffer.compare(left.targetBytes, right.targetBytes));
    for (let index = 1; index < rows.length; index += 1) {
        if (rows[index - 1].targetBytes.equals(rows[index].targetBytes)) {
            throw behaviorError("DUPLICATE_TARGET", "Key-behavior target actions must be unique.", {row: index});
        }
    }

    const populatedStepCount = rows.reduce((total, row) => total + row.steps.length, 0);
    if (populatedStepCount > limits.maxPopulatedSteps || populatedStepCount > 0xff) {
        throw behaviorError("CAPACITY_EXCEEDED", `Populated key-behavior step count exceeds ${limits.maxPopulatedSteps}.`);
    }
    const encodedRows = rows.map((row) => encodeRow(row));
    const size = KEY_BEHAVIOR_DOMAIN_V1.HEADER_SIZE + encodedRows.reduce((total, row) => total + row.length, 0);
    if (size > limits.maxPayloadSize) {
        throw behaviorError("CAPACITY_EXCEEDED", `Key-behavior payload is ${size} bytes; maximum is ${limits.maxPayloadSize}.`);
    }

    const output = Buffer.alloc(size);
    output[0] = rows.length;
    output[1] = populatedStepCount;
    let offset = KEY_BEHAVIOR_DOMAIN_V1.HEADER_SIZE;
    for (const row of encodedRows) {
        row.copy(output, offset);
        offset += row.length;
    }
    return output;
}

function decodeKeyBehaviorDomain(value, options = {}) {
    const bytes = copyBytes(value, "Key-behavior payload");
    const limits = normalizeLimits(options.limits || options);
    if (bytes.length < KEY_BEHAVIOR_DOMAIN_V1.HEADER_SIZE) {
        throw behaviorError("TRUNCATED", "Key-behavior payload is missing its four-byte header.");
    }
    if (bytes.length > limits.maxPayloadSize) {
        throw behaviorError("CAPACITY_EXCEEDED", `Key-behavior payload is ${bytes.length} bytes; maximum is ${limits.maxPayloadSize}.`);
    }
    const rowCount = bytes[0];
    const declaredStepCount = bytes[1];
    if (bytes[2] !== 0 || bytes[3] !== 0) {
        throw behaviorError("RESERVED_FIELDS", "Key-behavior header reserved bytes must be zero.");
    }
    if (rowCount > limits.maxRows || declaredStepCount > limits.maxPopulatedSteps) {
        throw behaviorError("CAPACITY_EXCEEDED", "Key-behavior header exceeds advertised capacities.");
    }

    const rows = [];
    let offset = KEY_BEHAVIOR_DOMAIN_V1.HEADER_SIZE;
    let actualStepCount = 0;
    let previousTargetBytes;
    for (let rowIndex = 0; rowIndex < rowCount; rowIndex += 1) {
        if (bytes.length - offset < KEY_BEHAVIOR_DOMAIN_V1.ROW_LENGTH_SIZE) {
            throw behaviorError("TRUNCATED", "Key-behavior row is missing its length.", {row: rowIndex});
        }
        const bodyLength = bytes.readUInt16LE(offset);
        const bodyStart = offset + KEY_BEHAVIOR_DOMAIN_V1.ROW_LENGTH_SIZE;
        const rowEnd = bodyStart + bodyLength;
        if (bodyLength < KEY_BEHAVIOR_DOMAIN_V1.ROW_FIXED_SIZE || rowEnd > bytes.length) {
            throw behaviorError("TRUNCATED", "Key-behavior row length exceeds the payload.", {row: rowIndex});
        }

        const targetBytes = Buffer.from(bytes.subarray(bodyStart, bodyStart + PROFILE_BLOB_V1.ACTION_SIZE));
        if (previousTargetBytes) {
            const order = Buffer.compare(previousTargetBytes, targetBytes);
            if (order === 0) {
                throw behaviorError("DUPLICATE_TARGET", "Key-behavior target actions must be unique.", {row: rowIndex});
            }
            if (order > 0) {
                throw behaviorError("ROW_ORDER", "Key-behavior rows must be sorted by canonical target action bytes.", {row: rowIndex});
            }
        }
        const target = decodeSemanticAction(targetBytes, {actionLimits: options.actionLimits});
        if (target.kind === PROFILE_ACTION_KINDS.NONE) {
            throw behaviorError("INVALID_TARGET", "A key-behavior row cannot target the none action.", {row: rowIndex});
        }

        const tapHoldTerm = bytes.readUInt16LE(bodyStart + 4);
        const longerHoldTerm = bytes.readUInt16LE(bodyStart + 6);
        const multiTapTerm = bytes.readUInt16LE(bodyStart + 8);
        const flags = bytes[bodyStart + 10];
        const stepCount = bytes[bodyStart + 11];
        if ((flags & ~KEY_BEHAVIOR_DOMAIN_V1.KNOWN_ROW_FLAGS) !== 0) {
            throw behaviorError("RESERVED_FLAGS", "Key-behavior row contains unknown flags.", {row: rowIndex});
        }
        if (stepCount > limits.maxTapStepsPerBehavior) {
            throw behaviorError("CAPACITY_EXCEEDED", "Key-behavior row exceeds the tap-step capacity.", {row: rowIndex});
        }

        const steps = [];
        let stepOffset = bodyStart + KEY_BEHAVIOR_DOMAIN_V1.ROW_FIXED_SIZE;
        let previousTapIndex = -1;
        for (let stepIndex = 0; stepIndex < stepCount; stepIndex += 1) {
            const decoded = decodeStep(bytes, stepOffset, rowEnd, limits, options.actionLimits, rowIndex, stepIndex);
            if (decoded.step.tapIndex <= previousTapIndex) {
                throw behaviorError(
                    decoded.step.tapIndex === previousTapIndex ? "DUPLICATE_STEP" : "STEP_ORDER",
                    "Key-behavior tap indexes must be strictly ascending.",
                    {row: rowIndex, step: stepIndex}
                );
            }
            previousTapIndex = decoded.step.tapIndex;
            steps.push(decoded.step);
            stepOffset = decoded.nextOffset;
        }
        if (stepOffset !== rowEnd) {
            throw behaviorError("ROW_LENGTH", "Key-behavior row has trailing or unaccounted bytes.", {row: rowIndex});
        }
        actualStepCount += steps.length;
        if (actualStepCount > limits.maxPopulatedSteps) {
            throw behaviorError("CAPACITY_EXCEEDED", "Key-behavior payload exceeds the populated-step capacity.");
        }
        rows.push({
            target,
            tapHoldTerm,
            longerHoldTerm,
            multiTapTerm,
            keepsAutoMouseAnchored: (flags & KEY_BEHAVIOR_DOMAIN_V1.ROW_FLAG_KEEPS_AUTO_MOUSE_ANCHORED) !== 0,
            steps,
        });
        previousTargetBytes = targetBytes;
        offset = rowEnd;
    }
    if (offset !== bytes.length) {
        throw behaviorError("TRAILING_BYTES", "Key-behavior payload has bytes after its declared rows.");
    }
    if (actualStepCount !== declaredStepCount) {
        throw behaviorError("COUNT_MISMATCH", "Key-behavior populated-step count does not match its rows.");
    }
    return {rows, rowCount, populatedStepCount: actualStepCount, byteLength: bytes.length};
}

function encodeKeyBehaviorDomainEnvelope(value, options = {}) {
    return encodeDomainEnvelope({
        id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS,
        version: KEY_BEHAVIOR_DOMAIN_V1.VERSION,
        payload: encodeKeyBehaviorDomain(value, options),
    });
}

function decodeKeyBehaviorDomainEnvelope(value, options = {}) {
    const envelope = decodeDomainEnvelope(value);
    if (envelope.id !== PROFILE_DOMAIN_IDS.KEY_BEHAVIORS || envelope.version !== KEY_BEHAVIOR_DOMAIN_V1.VERSION) {
        throw behaviorError("WRONG_DOMAIN", "Expected a key-behavior domain v1 envelope.");
    }
    return decodeKeyBehaviorDomain(envelope.payload, options);
}

function normalizeRow(row, limits, actionLimits, rowIndex) {
    if (!row || typeof row !== "object" || !Array.isArray(row.steps)) {
        throw new TypeError(`Key-behavior row ${rowIndex} must provide a steps array.`);
    }
    const targetBytes = encodeSemanticAction(row.target, {actionLimits});
    const target = decodeSemanticAction(targetBytes, {actionLimits});
    if (target.kind === PROFILE_ACTION_KINDS.NONE) {
        throw behaviorError("INVALID_TARGET", "A key-behavior row cannot target the none action.", {row: rowIndex});
    }
    if (row.steps.length > limits.maxTapStepsPerBehavior) {
        throw behaviorError("CAPACITY_EXCEEDED", "Key-behavior row exceeds the tap-step capacity.", {row: rowIndex});
    }
    const steps = row.steps.map((step, stepIndex) => normalizeStep(step, limits, actionLimits, rowIndex, stepIndex));
    steps.sort((left, right) => left.tapIndex - right.tapIndex);
    for (let index = 1; index < steps.length; index += 1) {
        if (steps[index - 1].tapIndex === steps[index].tapIndex) {
            throw behaviorError("DUPLICATE_STEP", "A key-behavior row cannot repeat a tap index.", {row: rowIndex, step: index});
        }
    }
    return {
        target,
        targetBytes,
        tapHoldTerm: assertU16(row.tapHoldTerm ?? 0, "tapHoldTerm"),
        longerHoldTerm: assertU16(row.longerHoldTerm ?? 0, "longerHoldTerm"),
        multiTapTerm: assertU16(row.multiTapTerm ?? 0, "multiTapTerm"),
        keepsAutoMouseAnchored: assertBoolean(row.keepsAutoMouseAnchored ?? false, "keepsAutoMouseAnchored"),
        steps,
    };
}

function encodeRow(row) {
    const encodedSteps = row.steps.map(encodeStep);
    const bodyLength = KEY_BEHAVIOR_DOMAIN_V1.ROW_FIXED_SIZE + encodedSteps.reduce((total, step) => total + step.length, 0);
    const output = Buffer.alloc(KEY_BEHAVIOR_DOMAIN_V1.ROW_LENGTH_SIZE + bodyLength);
    output.writeUInt16LE(bodyLength, 0);
    row.targetBytes.copy(output, 2);
    output.writeUInt16LE(row.tapHoldTerm, 6);
    output.writeUInt16LE(row.longerHoldTerm, 8);
    output.writeUInt16LE(row.multiTapTerm, 10);
    output[12] = row.keepsAutoMouseAnchored ? KEY_BEHAVIOR_DOMAIN_V1.ROW_FLAG_KEEPS_AUTO_MOUSE_ANCHORED : 0;
    output[13] = row.steps.length;
    let offset = 14;
    for (const step of encodedSteps) {
        step.copy(output, offset);
        offset += step.length;
    }
    return output;
}

function normalizeStep(step, limits, actionLimits, rowIndex, stepIndex) {
    if (!step || typeof step !== "object") {
        throw new TypeError(`Key-behavior row ${rowIndex} step ${stepIndex} must be an object.`);
    }
    const tapIndex = assertU8(step.tapIndex, "tapIndex");
    if (tapIndex >= limits.maxTapStepsPerBehavior) {
        throw behaviorError("INVALID_TAP_INDEX", `Tap index must be below ${limits.maxTapStepsPerBehavior}.`, {row: rowIndex, step: stepIndex});
    }
    const normalized = {tapIndex};
    if (step.tap !== undefined) normalized.tap = normalizeAction(step.tap, actionLimits, rowIndex, stepIndex, "tap");
    if (step.hold !== undefined) normalized.hold = normalizeHold(step.hold, limits, actionLimits, rowIndex, stepIndex, "hold");
    if (step.longHold !== undefined) normalized.longHold = normalizeHold(step.longHold, limits, actionLimits, rowIndex, stepIndex, "longHold");
    if (!normalized.tap && !normalized.hold && !normalized.longHold) {
        throw behaviorError("EMPTY_STEP", "A populated key-behavior step must contain at least one branch.", {row: rowIndex, step: stepIndex});
    }
    return normalized;
}

function encodeStep(step) {
    let mask = 0;
    const parts = [];
    if (step.tap) {
        mask |= KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_TAP;
        parts.push(step.tap.bytes);
    }
    if (step.hold) {
        mask |= KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_HOLD;
        parts.push(encodeHold(step.hold));
    }
    if (step.longHold) {
        mask |= KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_LONG_HOLD;
        parts.push(encodeHold(step.longHold));
    }
    return Buffer.concat([Buffer.from([step.tapIndex, mask]), ...parts]);
}

function decodeStep(bytes, offset, rowEnd, limits, actionLimits, rowIndex, stepIndex) {
    if (rowEnd - offset < KEY_BEHAVIOR_DOMAIN_V1.STEP_HEADER_SIZE) {
        throw behaviorError("TRUNCATED", "Key-behavior step is missing its header.", {row: rowIndex, step: stepIndex});
    }
    const tapIndex = bytes[offset];
    const mask = bytes[offset + 1];
    if (tapIndex >= limits.maxTapStepsPerBehavior) {
        throw behaviorError("INVALID_TAP_INDEX", `Tap index must be below ${limits.maxTapStepsPerBehavior}.`, {row: rowIndex, step: stepIndex});
    }
    if (mask === 0 || (mask & ~KEY_BEHAVIOR_DOMAIN_V1.KNOWN_STEP_MASK) !== 0) {
        throw behaviorError(mask === 0 ? "EMPTY_STEP" : "RESERVED_FLAGS", "Key-behavior step presence mask is invalid.", {row: rowIndex, step: stepIndex});
    }
    const step = {tapIndex};
    let nextOffset = offset + KEY_BEHAVIOR_DOMAIN_V1.STEP_HEADER_SIZE;
    if ((mask & KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_TAP) !== 0) {
        const decoded = readBoundedAction(bytes, nextOffset, rowEnd, actionLimits, rowIndex, stepIndex);
        step.tap = decoded.action;
        nextOffset = decoded.nextOffset;
    }
    if ((mask & KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_HOLD) !== 0) {
        const decoded = decodeHold(bytes, nextOffset, rowEnd, limits, actionLimits, rowIndex, stepIndex, "hold");
        step.hold = decoded.hold;
        nextOffset = decoded.nextOffset;
    }
    if ((mask & KEY_BEHAVIOR_DOMAIN_V1.STEP_HAS_LONG_HOLD) !== 0) {
        const decoded = decodeHold(bytes, nextOffset, rowEnd, limits, actionLimits, rowIndex, stepIndex, "longHold");
        step.longHold = decoded.hold;
        nextOffset = decoded.nextOffset;
    }
    return {step, nextOffset};
}

function normalizeHold(hold, limits, actionLimits, rowIndex, stepIndex, field) {
    if (!hold || typeof hold !== "object") {
        throw new TypeError(`${field} must be an object.`);
    }
    const mode = assertU8(hold.mode, `${field}.mode`);
    if (!Object.values(KEY_BEHAVIOR_HOLD_MODES).includes(mode)) {
        throw behaviorError("INVALID_HOLD_MODE", `Unsupported ${field} mode ${mode}.`, {row: rowIndex, step: stepIndex});
    }
    const repeatHz = assertU8(hold.repeatHz ?? 0, `${field}.repeatHz`);
    if (mode === KEY_BEHAVIOR_HOLD_MODES.REPEAT_WHILE_HELD) {
        if (repeatHz < 1 || repeatHz > limits.maxRepeatHz) {
            throw behaviorError("INVALID_REPEAT_RATE", `Repeat frequency must be 1 through ${limits.maxRepeatHz} Hz.`, {row: rowIndex, step: stepIndex});
        }
    } else if (repeatHz !== 0) {
        throw behaviorError("INVALID_REPEAT_RATE", "Non-repeat hold modes require repeat frequency zero.", {row: rowIndex, step: stepIndex});
    }
    return {mode, repeatHz, ...normalizeAction(hold.action, actionLimits, rowIndex, stepIndex, field)};
}

function encodeHold(hold) {
    return Buffer.concat([Buffer.from([hold.mode, hold.repeatHz]), hold.bytes]);
}

function decodeHold(bytes, offset, rowEnd, limits, actionLimits, rowIndex, stepIndex, field) {
    if (rowEnd - offset < KEY_BEHAVIOR_DOMAIN_V1.HOLD_SIZE) {
        throw behaviorError("TRUNCATED", `Key-behavior ${field} branch is truncated.`, {row: rowIndex, step: stepIndex});
    }
    const normalized = normalizeHold({
        mode: bytes[offset],
        repeatHz: bytes[offset + 1],
        action: readBoundedAction(bytes, offset + 2, rowEnd, actionLimits, rowIndex, stepIndex).action,
    }, limits, actionLimits, rowIndex, stepIndex, field);
    delete normalized.bytes;
    return {hold: normalized, nextOffset: offset + KEY_BEHAVIOR_DOMAIN_V1.HOLD_SIZE};
}

function normalizeAction(action, actionLimits, rowIndex, stepIndex, field) {
    const bytes = encodeSemanticAction(action, {actionLimits});
    const normalized = decodeSemanticAction(bytes, {actionLimits});
    if (normalized.kind === PROFILE_ACTION_KINDS.NONE) {
        throw behaviorError("INVALID_ACTION", `Key-behavior ${field} branch cannot use the none action.`, {row: rowIndex, step: stepIndex});
    }
    return {action: normalized, bytes};
}

function readBoundedAction(bytes, offset, rowEnd, actionLimits, rowIndex, stepIndex) {
    if (rowEnd - offset < PROFILE_BLOB_V1.ACTION_SIZE) {
        throw behaviorError("TRUNCATED", "Key-behavior semantic action is truncated.", {row: rowIndex, step: stepIndex});
    }
    const decoded = readSemanticAction(bytes, offset, {actionLimits});
    if (decoded.action.kind === PROFILE_ACTION_KINDS.NONE) {
        throw behaviorError("INVALID_ACTION", "A populated key-behavior branch cannot use the none action.", {row: rowIndex, step: stepIndex});
    }
    return decoded;
}

function normalizeLimits(value) {
    const limits = {};
    for (const [field, fallback] of Object.entries(KEY_BEHAVIOR_LIMITS)) {
        const candidate = value?.[field] ?? fallback;
        if (!Number.isInteger(candidate) || candidate < 1 || candidate > fallback) {
            throw new RangeError(`${field} must be a positive integer no greater than the frozen v1 ceiling ${fallback}.`);
        }
        limits[field] = candidate;
    }
    return limits;
}

function assertU8(value, label) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > 0xff) throw new RangeError(`${label} must be an 8-bit integer.`);
    return number;
}

function assertU16(value, label) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > 0xffff) throw new RangeError(`${label} must be a 16-bit integer.`);
    return number;
}

function assertBoolean(value, label) {
    if (typeof value !== "boolean") throw new TypeError(`${label} must be boolean.`);
    return value;
}

function copyBytes(value, label) {
    if (!(value instanceof Uint8Array)) throw new TypeError(`${label} must be a Buffer or Uint8Array.`);
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function behaviorError(code, message, details = {}) {
    return new ProfileBlobProtocolError(code, message, details);
}

module.exports = {
    KEY_BEHAVIOR_DOMAIN_V1,
    KEY_BEHAVIOR_HOLD_MODES,
    KEY_BEHAVIOR_LIMITS,
    decodeKeyBehaviorDomain,
    decodeKeyBehaviorDomainEnvelope,
    encodeKeyBehaviorDomain,
    encodeKeyBehaviorDomainEnvelope,
};
