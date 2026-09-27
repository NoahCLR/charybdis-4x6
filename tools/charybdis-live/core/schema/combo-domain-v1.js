"use strict";
const {encodeSemanticAction, decodeSemanticAction} = require("./profile-blob-v1");
const fail = message => Object.assign(new Error(message), {code: "INVALID_COMBO_DOMAIN"});

// Domain 0x30. A combo table is its rows and the two values QMK keeps for all
// combos: the default window, which a row without its own follows (termMs
// null), and the hold threshold. Version 2 stores both once in its header;
// version 1 stores every window on its row, has no default, and repeats the
// hold threshold on every row, so a version 1 table without rows has none.
const COMBO_DOMAIN_VERSION = 2;
const HEADER = {1: 4, 2: 8};
const ms = (value, min) => Number.isInteger(value) && value >= min && value <= 65535;

function encodeComboDomain(table, options = {}) {
    const {version = COMBO_DOMAIN_VERSION, defaultTermMs = null, holdTermMs = null, rows} = table || {};
    if (!HEADER[version]) throw fail(`Unknown combo format version ${version}.`);
    if (!Array.isArray(rows) || rows.length > 32) throw fail("At most 32 combos are supported.");
    const header = HEADER[version];
    const bytes = Buffer.alloc(header + rows.length * 28);
    bytes[0] = rows.length;
    if (version === 2) {
        // QMK fires a combo only inside its window: a zero default would
        // silently stop every combo that follows it.
        if (!ms(defaultTermMs, 1)) throw fail("The default combo window must be between 1 and 65535 ms.");
        if (!ms(holdTermMs, 0)) throw fail("The combo hold threshold must be between 0 and 65535 ms.");
        bytes.writeUInt16LE(defaultTermMs, 4); bytes.writeUInt16LE(holdTermMs, 6);
    } else {
        if (defaultTermMs !== null) throw fail("This combo format has no default window.");
        if (rows.length && !ms(holdTermMs, 0)) throw fail("The combo hold threshold must be between 0 and 65535 ms.");
    }
    rows.forEach((row, index) => {
        if (!Array.isArray(row.inputs) || row.inputs.length < 2 || row.inputs.length > 4) throw fail("A combo needs two to four inputs.");
        if (row.termMs === null) {
            if (version === 1) throw fail("This combo format stores every combo's own window.");
        } else if (!ms(row.termMs, version === 2 ? 1 : 0)) throw fail("A combo's window must be between 1 and 65535 ms.");
        if (row.mustHold && row.mustTap) throw fail("A combo cannot require both a hold and a tap.");
        const offset = header + 28 * index;
        bytes[offset] = row.inputs.length;
        bytes[offset + 1] = (row.mustHold ? 1 : 0) | (row.mustTap ? 2 : 0) | (row.ordered ? 4 : 0);
        bytes.writeUInt16LE(row.termMs ?? 0, offset + 2);
        if (version === 1) bytes.writeUInt16LE(holdTermMs, offset + 4);
        const output = encodeSemanticAction(row.output, options);
        if (output[0] === 0 || (output[0] === 1 && output.readUInt16LE(2) === 0)) throw fail("Choose a key or action output. Firmware callback outputs cannot be edited.");
        output.copy(bytes, offset + 8);
        const seen = new Set();
        row.inputs.forEach((input, slot) => {
            const action = encodeSemanticAction(input, options);
            if (action[0] === 0 || (action[0] === 1 && action.readUInt16LE(2) <= 1)) throw fail("Combo inputs must be assigned keys.");
            const identity = action.toString("hex");
            if (seen.has(identity)) throw fail("Combo inputs must be distinct.");
            seen.add(identity); action.copy(bytes, offset + 12 + slot * 4);
        });
    });
    return bytes;
}

function decodeComboDomain(value, version, options = {}) {
    const bytes = Buffer.from(value), header = HEADER[version];
    if (!header) throw fail(`Unknown combo format version ${version}.`);
    if (bytes.length < header || bytes[0] > 32 || bytes[1] || bytes[2] || bytes[3] || bytes.length !== header + 28 * bytes[0]) throw fail("Malformed combo header.");
    let holdTermMs = version === 2 ? bytes.readUInt16LE(6) : null;
    const rows = Array.from({length: bytes[0]}, (_, id) => {
        const offset = header + 28 * id;
        if (bytes[offset] < 2 || bytes[offset] > 4 || bytes[offset + 1] & ~7 || bytes[offset + 6] || bytes[offset + 7] || bytes.subarray(offset + 12 + bytes[offset] * 4, offset + 28).some(Boolean)) throw fail("Malformed combo row.");
        if (version === 1) {
            const hold = bytes.readUInt16LE(offset + 4);
            if (id && hold !== holdTermMs) throw fail("All combos share one hold threshold.");
            holdTermMs = hold;
        }
        const readAction = start => decodeSemanticAction(bytes.subarray(start, start + 4), options);
        const termMs = bytes.readUInt16LE(offset + 2);
        return {id, inputs: Array.from({length: bytes[offset]}, (_, slot) => readAction(offset + 12 + slot * 4)), output: readAction(offset + 8),
            termMs: version === 2 && termMs === 0 ? null : termMs, mustHold: Boolean(bytes[offset + 1] & 1), mustTap: Boolean(bytes[offset + 1] & 2), ordered: Boolean(bytes[offset + 1] & 4)};
    });
    const table = {version, defaultTermMs: version === 2 ? bytes.readUInt16LE(4) : null, holdTermMs, rows};
    if (!encodeComboDomain(table, options).equals(bytes)) throw fail("Noncanonical combo data.");
    return table;
}

// A version 1 table as version 2: its windows stay its own, and it takes the
// keyboard's default and, when it has no rows to carry one, the hold threshold.
function upgradeComboTable(table, {defaultTermMs, holdTermMs}) {
    if (table.version === COMBO_DOMAIN_VERSION) return table;
    return {...table, version: COMBO_DOMAIN_VERSION, defaultTermMs, holdTermMs: table.holdTermMs ?? holdTermMs};
}

// The window a row runs with.
const effectiveComboTerm = (table, row) => row.termMs ?? table.defaultTermMs;

module.exports = {COMBO_DOMAIN_VERSION, encodeComboDomain, decodeComboDomain, upgradeComboTable, effectiveComboTerm};
