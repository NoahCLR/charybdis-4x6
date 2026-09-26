"use strict";

// Put chosen units of a draft back to what the keyboard holds.
//
// A unit is what a review item is (profile-review.js): a key, a layer name,
// a behaviour, a combo, a macro with its name, one settings section, a
// pointing slot, or one lighting record. Each is copied from `before` into `after` at the
// level the keyboard stores it, and every domain it touched is encoded again,
// so the result passes the same validation as any other edit.

const {actionLimitsFor} = require("../schema/actions");
const {validateSnapshot} = require("./portable-profile");
const {settingsEditorView, fieldMask} = require("./settings-editor");
const {decodeProfileBlob, encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../schema/rgb-domain-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../schema/key-behavior-domain-v1");
const {decodeComboDomainV1, encodeComboDomainV1} = require("../schema/combo-domain-v1");
const {decodeSettings, encodeSettings, macroNamesOf} = require("../schema/settings-domain-v1");
const {decodePdDomain, encodePdDomain} = require("../schema/pd-mode-domain-v1");
const {singleComboRemovalIndex} = require("./profile-review");

const copy = value => JSON.parse(JSON.stringify(value));
const fail = text => Object.assign(new Error(text), {code: "PROFILE_REVERT_UNSUPPORTED"});
const GROUP_TABLES = ["groups", "layerGroupRows", "pdModeGroupRows", "comboGroupRows", "keyGroupRows"];


function revertUnits(before, after, units, capabilities) {
    const a = validateSnapshot(before.document, capabilities), b = validateSnapshot(after.document, capabilities);
    if (units.has("profile")) return copy(a.document);
    const document = copy(b.document);
    const blob = decodeProfileBlob(Buffer.from(document.profile, "base64"));
    const baseBlob = decodeProfileBlob(Buffer.from(a.document.profile, "base64"));
    const actionOptions = actionLimitsFor(document.version);
    const rgbOptions = {maximumBrightness: after.limits?.brightnessMax ?? 255};
    // Each domain is decoded once, edited for every unit, and encoded once.
    const codecs = {
        [PROFILE_DOMAIN_IDS.RGB]: [payload => decodeRgbDomainV1(payload, rgbOptions), value => encodeRgbDomainV1(value, rgbOptions)],
        [PROFILE_DOMAIN_IDS.KEY_BEHAVIORS]: [payload => decodeKeyBehaviorDomain(payload, actionOptions).rows, rows => encodeKeyBehaviorDomain({rows}, actionOptions)],
        [PROFILE_DOMAIN_IDS.COMBOS]: [payload => decodeComboDomainV1(payload, actionOptions), rows => encodeComboDomainV1(rows, actionOptions)],
        [PROFILE_DOMAIN_IDS.SETTINGS]: [payload => decodeSettings(payload), value => encodeSettings(value)],
        [PROFILE_DOMAIN_IDS.PD_MODES]: [payload => decodePdDomain(payload), slots => encodePdDomain(slots)],
    };
    const open = new Map();
    const domain = id => {
        if (!open.has(id)) {
            const [decode] = codecs[id];
            const mine = blob.domains.find(row => row.id === id), theirs = baseBlob.domains.find(row => row.id === id);
            if (!mine || !theirs) throw fail("This change cannot be discarded on its own.");
            open.set(id, {mine: decode(mine.payload), theirs: decode(theirs.payload)});
        }
        return open.get(id);
    };
    const sections = new Map(settingsEditorView(before).sections.map(section => [section.id, section.fields]));
    const settingBits = (settings, id, mask) => {
        settings.mine.values[id] = ((settings.mine.values[id] & ~mask) | (settings.theirs.values[id] & mask)) >>> 0;
    };

    for (const unit of units) {
        const [kind, ...parts] = unit.split(":");
        const rest = parts.join(":");
        if (kind === "layout") {
            const [layer, position] = parts.map(Number);
            document.layers[layer][position] = a.document.layers[layer][position];
        } else if (kind === "macro") {
            // A macro is its steps and its name; the name lives in settings.
            const index = Number(rest);
            document.macros[index] = a.document.macros[index];
            const settings = domain(PROFILE_DOMAIN_IDS.SETTINGS);
            const name = macroNamesOf(settings.theirs)[index] || "";
            if (settings.mine.macroNames) settings.mine.macroNames[index] = name;
            else if (name) throw fail("This macro name cannot be put back on its own.");
        } else if (kind === "layerName") {
            const settings = domain(PROFILE_DOMAIN_IDS.SETTINGS);
            settings.mine.names[Number(rest)] = settings.theirs.names[Number(rest)];
        } else if (kind === "settings") {
            const settings = domain(PROFILE_DOMAIN_IDS.SETTINGS);
            if (rest === "otherKeyOptions") {
                const masks = after.options?.keymapMasks?.reduce((mask, value) => mask | value, 0) || 0;
                settingBits(settings, 24, ~masks >>> 0);
            } else {
                const fields = sections.get(rest);
                if (!fields || fields.some(field => !Number.isInteger(field.id))) throw fail("These settings cannot be discarded on their own.");
                for (const field of fields) settingBits(settings, field.id, fieldMask(field));
            }
        } else if (kind === "behavior") {
            const behaviors = domain(PROFILE_DOMAIN_IDS.KEY_BEHAVIORS);
            const same = row => JSON.stringify(row.target) === rest;
            const kept = behaviors.mine.filter(row => !same(row)), stored = behaviors.theirs.find(same);
            behaviors.mine = stored ? [...kept, stored] : kept;
        } else if (kind === "combo") {
            const combos = domain(PROFILE_DOMAIN_IDS.COMBOS), index = Number(rest);
            // Restore a lone middle deletion by inserting the stored row;
            // assigning it would overwrite the next combo in the packed table.
            const removed = singleComboRemovalIndex(combos.theirs, combos.mine);
            if (removed === index) combos.mine.splice(index, 0, combos.theirs[index]);
            else combos.mine[index] = combos.theirs[index];
        } else if (kind === "pd") {
            const slots = domain(PROFILE_DOMAIN_IDS.PD_MODES);
            slots.mine[Number(rest)] = slots.theirs[Number(rest)];
        } else if (kind === "rgb") {
            const rgb = domain(PROFILE_DOMAIN_IDS.RGB), mine = rgb.mine, theirs = rgb.theirs;
            if (rest === "stages") mine.stageEnableMask = theirs.stageEnableMask;
            else if (rest === "automouse") mine.automouseFade = theirs.automouseFade;
            else if (rest === "combo") mine.comboFeedback = theirs.comboFeedback;
            else if (rest === "key") mine.keyFeedback = theirs.keyFeedback;
            else if (rest === "groups") for (const table of GROUP_TABLES) mine[table] = theirs[table];
            else if (parts[0] === "layer" || parts[0] === "pd") {
                const [table, key] = parts[0] === "layer" ? ["layerColors", "layerId"] : ["pdModeColors", "pdModeId"];
                const id = Number(parts[1]), stored = theirs[table].find(row => row[key] === id);
                const index = mine[table].findIndex(row => row[key] === id);
                if (!stored || index < 0) throw fail("This lighting change cannot be discarded on its own.");
                mine[table][index] = stored;
            } else throw fail("This lighting change cannot be discarded on its own.");
        } else throw fail("This change cannot be discarded on its own.");
    }

    for (const [id, {mine}] of open) {
        const row = blob.domains.find(entry => entry.id === id);
        row.payload = codecs[id][1](id === PROFILE_DOMAIN_IDS.COMBOS ? mine.filter(Boolean) : mine);
    }
    if (open.size) document.profile = encodeProfileBlob(blob).toString("base64");
    return document;
}

module.exports = {revertUnits};
