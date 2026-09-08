"use strict";

const {validateSnapshot} = require("./portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../schema/profile-blob-v1");
const {encodeSettings} = require("../schema/settings-domain-v1");
const {macroKeycodes, encodeMacroPayload, decodeMacroPayload} = require("../schema/macro-payload");
const fail = message => Object.assign(new Error(message), {code: "MACRO_EDIT_CONFLICT"});

function macroEditorView(snapshot) {
    if (!snapshot?.document || snapshot.incomplete) return null;
    const {document, settings} = validateSnapshot(snapshot.document);
    const slot = (bytes, kind, index) => ({kind, keycode: `${kind === "via" ? "VIA_MACRO" : "MACRO"}_${index}`,
        payload: decodeMacroPayload(bytes, kind), empty: bytes.length === 0, bytes: bytes.length});
    return {identity: snapshot.fingerprint,
        viaMacros: document.macros.map((value, index) => slot(Buffer.from(value, "base64"), "via", index)),
        hardcodedMacros: settings.macros.map((bytes, index) => slot(bytes, "user", index)),
        macroPayloadKeycodes: macroKeycodes()};
}

function editMacro(snapshot, message, capabilities) {
    if (!snapshot?.document || !message.expectedFingerprint || message.expectedFingerprint !== snapshot.fingerprint) throw fail("The keyboard changed since this macro draft was opened. Read the keyboard and review the draft before saving again.");
    const match = /^(VIA_MACRO|MACRO)_(\d+)$/.exec(message.keycode || "");
    const index = match && Number(match[2]), kind = match?.[1] === "VIA_MACRO" ? "via" : "user";
    if (!match || index >= (kind === "via" ? 64 : 16) || String(index) !== match[2]) throw fail("Choose a macro slot reported by the keyboard.");
    const value = validateSnapshot(snapshot.document, capabilities);
    const bytes = encodeMacroPayload(message.payload, kind);
    const document = JSON.parse(JSON.stringify(value.document));
    if (kind === "via") document.macros[index] = bytes.toString("base64");
    else {
        value.settings.macros[index] = bytes;
        const domains = decodeProfileBlob(value.profile).domains.map(domain => domain.id === 0x40 ? {...domain, payload: encodeSettings(value.settings)} : domain);
        document.profile = encodeProfileBlob({domains}).toString("base64");
    }
    validateSnapshot(document, capabilities);
    return document;
}

module.exports = {macroEditorView, editMacro};
