"use strict";
const {encodeSemanticAction, decodeSemanticAction} = require("./profile-blob-v1");
const fail = message => Object.assign(new Error(message), {code: "INVALID_COMBO_DOMAIN"});
function encodeComboDomainV1(rows, options = {}) {
    if (!Array.isArray(rows) || rows.length > 32) throw fail("At most 32 combos are supported.");
    const bytes = Buffer.alloc(4 + rows.length * 28);
    bytes[0] = rows.length;
    rows.forEach((row, index) => {
        if (!Array.isArray(row.inputs) || row.inputs.length < 2 || row.inputs.length > 4) throw fail("A combo needs two to four inputs.");
        for (const field of ["termMs", "holdTermMs"]) if (!Number.isInteger(row[field]) || row[field] < 0 || row[field] > 65535) throw fail("Combo timing must be between 0 and 65535 ms.");
        if (index && row.holdTermMs !== rows[0].holdTermMs) throw fail("All combos share one hold threshold.");
        if (row.mustHold && row.mustTap) throw fail("A combo cannot require both a hold and a tap.");
        const offset = 4 + 28 * index;
        bytes[offset] = row.inputs.length;
        bytes[offset + 1] = (row.mustHold ? 1 : 0) | (row.mustTap ? 2 : 0) | (row.ordered ? 4 : 0);
        bytes.writeUInt16LE(row.termMs, offset + 2); bytes.writeUInt16LE(row.holdTermMs, offset + 4);
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
function decodeComboDomainV1(value, options = {}) {
    const bytes = Buffer.from(value);
    if (bytes.length < 4 || bytes[0] > 32 || bytes[1] || bytes[2] || bytes[3] || bytes.length !== 4 + 28 * bytes[0]) throw fail("Malformed combo header.");
    const rows = Array.from({length: bytes[0]}, (_, id) => {
        const offset = 4 + 28 * id;
        if (bytes[offset] < 2 || bytes[offset] > 4 || bytes[offset + 1] & ~7 || bytes[offset + 6] || bytes[offset + 7] || bytes.subarray(offset + 12 + bytes[offset] * 4, offset + 28).some(Boolean)) throw fail("Malformed combo row.");
        const readAction = start => decodeSemanticAction(bytes.subarray(start, start + 4), options);
        return {id, inputs: Array.from({length: bytes[offset]}, (_, slot) => readAction(offset + 12 + slot * 4)), output: readAction(offset + 8), termMs: bytes.readUInt16LE(offset + 2), holdTermMs: bytes.readUInt16LE(offset + 4), mustHold: Boolean(bytes[offset + 1] & 1), mustTap: Boolean(bytes[offset + 1] & 2), ordered: Boolean(bytes[offset + 1] & 4)};
    });
    if (!encodeComboDomainV1(rows, options).equals(bytes)) throw fail("Noncanonical combo data.");
    return rows;
}
module.exports = {encodeComboDomainV1, decodeComboDomainV1};
