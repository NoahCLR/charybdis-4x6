"use strict";

// The 64 custom keys: named keys that do only what their behaviour says. A key
// has an identity (CUSTOM_KEY_n, keycode 0x7e40 + n) and a name; what it does
// is its behaviour row, edited like any other. Without a row it does nothing,
// and a behaviour step cannot send one: it is placed on a layer or emitted by
// a combo.

const {decodedOf, validateSnapshot} = require("./portable-profile");
const {decodeProfileBlob, encodeProfileBlob, PROFILE_ACTION_KINDS: ACTION} = require("../schema/profile-blob-v1");
const {SETTINGS, asciiName, customKeyNamesOf, encodeSettings, upgradeSettings} = require("../schema/settings-domain-v1");
const {knownActionAbi} = require("../schema/actions");
const {CUSTOM_KEY_SLOTS, customKeyCode} = require("../data/user-keycodes");
const fail = (message, code = "CUSTOM_KEY_EDIT_CONFLICT") => Object.assign(new Error(message), {code});

// A keyboard with the userspace keycode blocks has custom keys; its action ABI
// is the one the app knows, and its profiles are schema 2.
const customKeysSupported = (document, capabilities) => document?.version === 2 && knownActionAbi(capabilities?.actionAbiDigest ?? document.actionAbiDigest);

function customKeyEditorView(snapshot, capabilities) {
    if (!snapshot?.document || snapshot.incomplete || !customKeysSupported(snapshot.document, capabilities)) return null;
    const {settings, behaviors} = decodedOf(snapshot);
    const names = customKeyNamesOf(settings);
    const behaved = new Set(behaviors.rows.filter(row => row.target.kind === ACTION.CUSTOM_KEY).map(row => row.target.operand));
    return {identity: snapshot.fingerprint,
        keys: names.map((name, slot) => ({slot, keycode: `CUSTOM_KEY_${slot}`, code: customKeyCode(slot), name, hasBehavior: behaved.has(slot)})),
        // Every key can hold a full-length name, whatever the others hold.
        names: {perName: SETTINGS.MACRO_NAME_CHARS}};
}

// A custom key's name. Names live in the profile's settings domain, so naming
// a key upgrades that domain to v5.
function editCustomKey(snapshot, message, capabilities) {
    if (!snapshot?.document || !message.expectedFingerprint || message.expectedFingerprint !== snapshot.fingerprint) throw fail("The keyboard changed since this draft was opened. Read the keyboard and review the draft before saving again.");
    const match = /^CUSTOM_KEY_(\d+)$/.exec(message.keycode || "");
    const slot = match && Number(match[1]);
    if (!match || slot >= CUSTOM_KEY_SLOTS || String(slot) !== match[1]) throw fail("Choose a custom key the keyboard has.");
    if (typeof message.name !== "string") throw fail("A custom key name must be text.");
    if (!customKeysSupported(snapshot.document, capabilities)) throw fail("Custom keys need firmware with the userspace keycode blocks.", "CUSTOM_KEYS_UNSUPPORTED");
    const value = validateSnapshot(snapshot.document, capabilities);
    const name = message.name.trim();
    if (!asciiName(name)) throw fail(`A custom key name is up to ${SETTINGS.MACRO_NAME_CHARS} plain characters: letters, digits, spaces and punctuation.`, "CUSTOM_KEY_NAME_INVALID");
    if (name === customKeyNamesOf(value.settings)[slot]) return value.document;
    const document = JSON.parse(JSON.stringify(value.document));
    const settings = upgradeSettings(value.settings, 5);
    settings.customKeyNames[slot] = name;
    const domains = decodeProfileBlob(value.profile).domains.map(domain => domain.id === 0x40 ? {...domain, version: settings.formatVersion, payload: encodeSettings(settings)} : domain);
    document.profile = encodeProfileBlob({schema: {major: 2, minor: 0}, domains}).toString("base64");
    validateSnapshot(document, capabilities);
    return document;
}

module.exports = {customKeyEditorView, editCustomKey, customKeysSupported};
