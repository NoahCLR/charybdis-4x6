"use strict";

const catalog = require("../data/keycode-catalog");
const {validateMacroIr} = require("./settings-domain-v1");
const fail = message => Object.assign(new Error(message), {code: "INVALID_MACRO"});
const validKey = key => Number.isInteger(key) && ((key >= 4 && key <= 0xa4) || (key >= 0xe0 && key <= 0xe7));
const escapeText = text => text.replace(/[{}]/g, brace => brace + brace);
const keyName = key => catalog.resolve(key).name;

function macroKeycodes() {
    return catalog.entries().filter(entry => validKey(entry.value)).flatMap(entry => [entry.name, ...entry.aliases]);
}

// The existing recorder and step builder use text plus {key}, {+key}, {-key}
// and {milliseconds}. Doubled braces preserve literal text from device bytes.
function parsePayload(payload) {
    if (typeof payload !== "string" || payload.length > 32768) throw fail("Enter a macro of at most 32,768 characters.");
    const steps = [], held = new Set();
    let text = "";
    const flush = () => {if (text) steps.push({kind: "text", text}); text = "";};
    for (let index = 0; index < payload.length;) {
        const char = payload[index++];
        if ((char === "{" || char === "}") && payload[index] === char) {text += char; index++; continue;}
        if (char === "}") throw fail("Unexpected }. Use }} for a literal closing brace.");
        if (char !== "{") {
            const code = char.charCodeAt(0);
            if (!(code === 9 || code === 10 || (code >= 32 && code <= 126))) throw fail("Macros support ASCII text, tabs and newlines.");
            text += char; continue;
        }
        flush();
        const end = payload.indexOf("}", index);
        if (end < 0) throw fail("Missing } for a macro command.");
        const command = payload.slice(index, end).trim(); index = end + 1;
        if (/^\d+$/.test(command)) {
            const delay = Number(command);
            if (delay > 65535) throw fail("A macro delay must be between 0 and 65,535 milliseconds.");
            steps.push({kind: "delay", delay}); continue;
        }
        const kind = command[0] === "+" ? "down" : command[0] === "-" ? "up" : "tap";
        const names = (kind === "tap" ? command : command.slice(1)).split(",").map(name => name.trim());
        const keys = names.map(name => catalog.encode(name));
        if (!keys.length || keys.length > 16 || keys.some(key => !validKey(key)) || new Set(keys).size !== keys.length || (kind !== "tap" && keys.length !== 1)) throw fail("Choose up to 16 distinct basic keys for a chord, or one key for a press or release.");
        if (kind === "up") {
            if (!held.delete(keys[0])) throw fail("A macro releases a key that is not held.");
        } else {
            if (keys.some(key => held.has(key)) || held.size + keys.length > 16) throw fail("A macro presses a held key or holds more than 16 keys.");
            if (kind === "down") held.add(keys[0]);
        }
        steps.push({kind, keys});
    }
    flush();
    if (held.size) throw fail("Release every held key before the macro ends.");
    return steps;
}

function encodeMacroPayload(payload, kind) {
    if (!["via", "user"].includes(kind)) throw fail("Unknown macro bank.");
    const chunks = [];
    for (const step of parsePayload(payload)) {
        if (step.kind === "text") {
            const bytes = Buffer.from(step.text, "ascii");
            if (kind === "via") chunks.push(bytes);
            else for (let offset = 0; offset < bytes.length; offset += 255) {
                const part = bytes.subarray(offset, offset + 255);
                chunks.push(Buffer.from([1, part.length]), part);
            }
        } else if (step.kind === "delay") {
            chunks.push(kind === "via" ? Buffer.concat([Buffer.from([1, 4]), Buffer.from(`${step.delay}|`)]) : Buffer.from([2, step.delay & 255, step.delay >> 8]));
        } else if (kind === "user") {
            chunks.push(Buffer.from(step.kind === "tap" ? [5, step.keys.length, ...step.keys] : [step.kind === "down" ? 3 : 4, step.keys[0]]));
        } else if (step.kind === "tap" && step.keys.length > 1) {
            for (const key of step.keys) chunks.push(Buffer.from([1, 2, key]));
            for (const key of [...step.keys].reverse()) chunks.push(Buffer.from([1, 3, key]));
        } else {
            chunks.push(Buffer.from([1, {tap: 1, down: 2, up: 3}[step.kind], step.keys[0]]));
        }
    }
    const bytes = Buffer.concat(chunks);
    if (kind === "user") validateMacroIr(bytes);
    return bytes;
}

function decodeMacroPayload(bytes, kind) {
    if (!Buffer.isBuffer(bytes) || !["via", "user"].includes(kind)) throw fail("Invalid macro bytes or bank.");
    if (kind === "user") validateMacroIr(bytes);
    let output = "", offset = 0;
    while (offset < bytes.length) {
        let op = bytes[offset++];
        if (kind === "via") {
            if (op !== 1) {output += escapeText(String.fromCharCode(op)); continue;}
            op = bytes[offset++];
            if (op === 4) {
                const end = bytes.indexOf(124, offset);
                if (end < 0) throw fail("Truncated macro delay.");
                output += `{${bytes.subarray(offset, end).toString("ascii")}}`; offset = end + 1;
            } else {
                if (![1, 2, 3].includes(op) || !validKey(bytes[offset])) throw fail("Invalid macro key instruction.");
                output += `{${op === 2 ? "+" : op === 3 ? "-" : ""}${keyName(bytes[offset++])}}`;
            }
        } else if (op === 1) {
            const length = bytes[offset++];
            output += escapeText(bytes.subarray(offset, offset + length).toString("ascii")); offset += length;
        } else if (op === 2) {
            output += `{${bytes.readUInt16LE(offset)}}`; offset += 2;
        } else if (op === 3 || op === 4) {
            output += `{${op === 3 ? "+" : "-"}${keyName(bytes[offset++])}}`;
        } else {
            const length = bytes[offset++];
            output += `{${[...bytes.subarray(offset, offset + length)].map(keyName).join(",")}}`; offset += length;
        }
    }
    parsePayload(output);
    return output;
}

module.exports = {macroKeycodes, parsePayload, encodeMacroPayload, decodeMacroPayload};
