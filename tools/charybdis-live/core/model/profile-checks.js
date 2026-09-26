"use strict";

// Whole-profile findings shown before Apply. Layer reachability lives in its
// own walk; these checks cover actions that the firmware accepts but cannot
// perform, and destination limits that would refuse the save.
const {layerReach, compareFindings} = require("./layer-reach");
const {baseLighting} = require("./settings-editor");
const {macroProgramBytes, MACRO_PROGRAM_MAX} = require("../schema/macro-payload");

function profileFindings(decoded, destination = {}) {
    const findings = layerReach(decoded);
    for (const [slot, count] of Object.entries(decoded.danglingPdBindings || {})) {
        if (!count) continue;
        findings.push({kind: "inertPointing", level: "warning", layers: [], identity: `${slot}:${count}`, count,
            title: `${count} action${count === 1 ? "" : "s"} reach empty pointing slot ${Number(slot) + 1}`,
            detail: "Keys, behaviours or combo outputs still name this empty slot. The keyboard accepts them but does nothing when they run.",
            fix: "Configure the slot or replace the actions that reach it.", place: {kind: "pointing", slot: Number(slot)}});
    }
    (decoded.document?.macros || []).forEach((encoded, index) => {
        const bytes = Buffer.from(encoded, "base64");
        if (!bytes.length) return;
        const size = macroProgramBytes(bytes);
        if (size <= MACRO_PROGRAM_MAX) return;
        findings.push({kind: "unplayableMacro", level: "warning", layers: [], identity: `${index}:${size}`,
            title: `Macro ${index} is too long to play`,
            detail: `The keyboard stores this macro, but its ${size}-byte program exceeds the ${MACRO_PROGRAM_MAX}-byte playback limit. Pressing it does nothing.`,
            fix: "Shorten the macro before applying this profile.", place: {kind: "macro", index}});
    });
    const lighting = baseLighting(decoded.settings.values);
    const max = destination.brightnessMax;
    if (Number.isInteger(max) && lighting.brightness > max) {
        findings.push({kind: "brightnessBlocker", level: "blocker", layers: [], identity: String(lighting.brightness),
            title: `Base brightness exceeds this keyboard's limit of ${max}`,
            detail: `This profile stores brightness ${lighting.brightness}; the keyboard accepts at most ${max}. Apply will refuse the profile.`,
            fix: `Set Base Lighting brightness to ${max} or lower.`, place: {kind: "settings", area: "Settings", section: "rgbAppearance"}});
    }
    if (Array.isArray(destination.effects) && !destination.effects.some(effect => effect.id === lighting.effect)) {
        findings.push({kind: "effectBlocker", level: "blocker", layers: [], identity: String(lighting.effect),
            title: `Base lighting effect ${lighting.effect} is unavailable on this keyboard`,
            detail: "The connected firmware does not advertise this effect. Apply will refuse the profile.",
            fix: "Choose a base lighting effect this keyboard reports.", place: {kind: "settings", area: "Settings", section: "rgbAppearance"}});
    }
    return findings;
}

function draftProfileChecks(keyboard, draft, order, destination) {
    return compareFindings(keyboard ? profileFindings(keyboard, destination) : [], profileFindings(draft, destination), order);
}

module.exports = {profileFindings, draftProfileChecks};
