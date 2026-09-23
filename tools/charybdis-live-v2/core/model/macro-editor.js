"use strict";

const {validateSnapshot} = require("./portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../schema/profile-blob-v1");
const {SETTINGS, encodeSettings, macroNamesOf, upgradeSettings} = require("../schema/settings-domain-v1");
const {macroKeycodes, encodeMacroPayload, decodeMacroPayload} = require("../schema/macro-payload");
const fail = message => Object.assign(new Error(message), {code: "MACRO_EDIT_CONFLICT"});

// The bytes all 64 names may share: the space the retired user macros had.
const NAME_BYTES_SHARED = SETTINGS.MAX_SIZE - SETTINGS.FIXED_SIZE - SETTINGS.MACRO_NAMES;

function macroEditorView(snapshot) {
    if (!snapshot?.document || snapshot.incomplete) return null;
    const {document, settings} = validateSnapshot(snapshot.document);
    const names = macroNamesOf(settings);
    const slot = (bytes, index) => ({kind: "via", keycode: `VIA_MACRO_${index}`, name: names[index],
        payload: decodeMacroPayload(bytes, "via"), empty: bytes.length === 0, bytes: bytes.length});
    return {identity: snapshot.fingerprint,
        viaMacros: document.macros.map((value, index) => slot(Buffer.from(value, "base64"), index)),
        names: {used: names.reduce((total, name) => total + Buffer.byteLength(name), 0), shared: NAME_BYTES_SHARED, perName: SETTINGS.MACRO_NAME_BYTES},
        macroPayloadKeycodes: macroKeycodes()};
}

// A VIA macro's steps, its name, or both. A name lives in the profile's
// settings domain, so naming a macro upgrades that domain to v3.
function editMacro(snapshot, message, capabilities) {
    if (!snapshot?.document || !message.expectedFingerprint || message.expectedFingerprint !== snapshot.fingerprint) throw fail("The keyboard changed since this macro draft was opened. Read the keyboard and review the draft before saving again.");
    if (/^MACRO_\d+$/.test(message.keycode || "")) throw fail("User macros are retired; use a VIA macro.");
    const match = /^VIA_MACRO_(\d+)$/.exec(message.keycode || "");
    const index = match && Number(match[1]);
    if (!match || index >= 64 || String(index) !== match[1]) throw fail("Choose a macro slot reported by the keyboard.");
    if (message.payload === undefined && message.name === undefined) throw fail("Send the macro's steps, its name, or both.");
    const value = validateSnapshot(snapshot.document, capabilities);
    const document = JSON.parse(JSON.stringify(value.document));
    if (message.payload !== undefined) document.macros[index] = encodeMacroPayload(message.payload, "via").toString("base64");
    if (message.name !== undefined && message.name !== macroNamesOf(value.settings)[index]) {
        if (typeof message.name !== "string") throw fail("A macro name must be text.");
        // Names live in settings v3, which only a schema-2 profile carries.
        if (value.document.version !== 2) throw fail("Naming macros needs the eight-slot pointing firmware (profile schema 2).");
        const settings = upgradeSettings(value.settings);
        settings.macroNames[index] = message.name.trim();
        const domains = decodeProfileBlob(value.profile).domains.map(domain => domain.id === 0x40 ? {...domain, version: settings.formatVersion, payload: encodeSettings(settings)} : domain);
        document.profile = encodeProfileBlob({schema: {major: value.document.version, minor: 0}, domains}).toString("base64");
    }
    validateSnapshot(document, capabilities);
    return document;
}

module.exports = {macroEditorView, editMacro};
