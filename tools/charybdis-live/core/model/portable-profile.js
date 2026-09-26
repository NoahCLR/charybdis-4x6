"use strict";
const {actionLimitsFor, keycodeAction, pdSlotOfCode} = require("../schema/actions");
const {decodePdDomain, encodePdDomain} = require("../schema/pd-mode-domain-v1");
const {decodeProfileBlob, encodeProfileBlob, crc32, fnv1a32} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../schema/rgb-domain-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../schema/key-behavior-domain-v1");
const {decodeComboDomainV1, encodeComboDomainV1} = require("../schema/combo-domain-v1");
const {decodeSettings, encodeSettings} = require("../schema/settings-domain-v1");
const {layerName} = require("./vocabulary");
const fail = message => Object.assign(new Error(message), {code: "INVALID_PORTABLE_PROFILE"});
const u16 = value => Number.isInteger(value) && value >= 0 && value <= 65535;
function base64(value, max, label) {
    if (typeof value !== "string" || value.length > Math.ceil(max / 3) * 4 || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(value)) throw fail(`Invalid ${label}.`);
    const bytes = Buffer.from(value, "base64");
    if (bytes.length > max || bytes.toString("base64") !== value) throw fail(`Invalid ${label}.`);
    return bytes;
}
function macroSlots(bytes, count) {
    if (!Buffer.isBuffer(bytes) || bytes.at(-1) !== 0) throw fail("The macro bank has an unfinished write.");
    const slots = []; let offset = 0;
    while (slots.length < count) {
        const end = bytes.indexOf(0, offset);
        if (end < offset || end >= bytes.length - 1) throw fail("The macro bank is missing a slot terminator.");
        const slot = Buffer.from(bytes.subarray(offset, end)); validateViaMacro(slot); slots.push(slot); offset = end + 1;
    }
    return slots;
}
function validateViaMacro(bytes) {
    const held = new Set(); let index = 0;
    const key = v => (v >= 4 && v <= 0xa4) || (v >= 0xe0 && v <= 0xe7);
    while (index < bytes.length) {
        const b = bytes[index++];
        if (b !== 1) {if (!(b === 9 || b === 10 || (b >= 32 && b <= 126))) throw fail("Unsupported macro character."); continue;}
        const type = bytes[index++];
        if (type === 4) {
            let digits = "";
            while (index < bytes.length && bytes[index] >= 48 && bytes[index] <= 57 && digits.length < 5) digits += String.fromCharCode(bytes[index++]);
            if (!digits || Number(digits) > 65535 || bytes[index++] !== 124) throw fail("Invalid macro delay.");
        } else {
            const k = bytes[index++];
            if (![1, 2, 3].includes(type) || !key(k)) throw fail("Unsupported macro key instruction.");
            if (type === 1 && held.has(k)) throw fail("A macro taps a key it already holds.");
            if (type === 2) {if (held.has(k) || held.size >= 16) throw fail("Unbalanced macro keys."); held.add(k);}
            if (type === 3 && !held.delete(k)) throw fail("Unbalanced macro keys.");
        }
    }
    if (held.size) throw fail("A macro leaves keys pressed.");
}
// The macro bank a document's storage geometry has, when no keyboard says.
const macroBankBytes = document => document.layers.length === 5 ? 7551 : 7191;
function macroBank(slots, capacity) {
    const size = slots.reduce((total, bytes) => total + bytes.length + 1, 1);
    if (size > capacity) throw fail(`Macros need ${size} bytes; this keyboard has ${capacity}.`);
    const result = Buffer.alloc(capacity); let offset = 0;
    for (const slot of slots) {slot.copy(result, offset); offset += slot.length + 1;}
    return result;
}
function materializeProfile(active, defaults, combos, settings) {
    if (!combos || combos.noTimer || combos.customTrigger || combos.customRelease || combos.customRepress || combos.strictTimer || combos.fixedReference) throw fail("This keyboard uses combo hooks or timing that cannot be represented by a portable profile.");
    const live = decodeProfileBlob(active), fallback = decodeProfileBlob(defaults);
    if (live.schema.major !== fallback.schema.major) throw fail("Active and default profiles use different schemas.");
    const domains = (live.schema.major === 2 ? [0x10, 0x20, 0x50] : [0x10, 0x20]).map(id => {
        const domain = live.domains.find(d => d.id === id) || fallback.domains.find(d => d.id === id);
        if (!domain) throw fail("The keyboard did not report every profile domain.");
        return domain;
    });
    const rows = combos.rows.map(row => ({...row, inputs: row.inputs.map(keycodeAction), output: keycodeAction(row.output)}));
    domains.push({id: 0x30, version: 1, payload: encodeComboDomainV1(rows)}, {id: 0x40, version: settings[0], payload: settings});
    return encodeProfileBlob({schema: live.schema, domains});
}
function createSnapshot({profile, via, actionAbiDigest}) {
    const document = {format: "charybdis-profile", version: decodeProfileBlob(profile).schema.major, keyboard: "charybdis-4x6", actionAbiDigest,
        layers: Array.from({length: via.layers}, (_, layer) => Array.from({length: 60}, (_, pos) => via.layout.readUInt16BE((layer * 60 + pos) * 2))),
        profile: profile.toString("base64"), macros: macroSlots(via.macros, via.macroSlots).map(bytes => bytes.toString("base64"))};
    validateSnapshot(document); return document;
}
function validateSnapshot(value, capabilities) {
    if (typeof value === "string") {
        if (Buffer.byteLength(value) > 100000) throw fail("This profile file is too large.");
        try {value = JSON.parse(value);} catch {throw fail("This is not a valid profile file.");}
    }
    if (!value || value.format !== "charybdis-profile" || ![1, 2].includes(value.version) || value.keyboard !== "charybdis-4x6" || !Number.isInteger(value.actionAbiDigest) || value.actionAbiDigest < 1 || value.actionAbiDigest > 0xffffffff) throw fail("Choose a supported Charybdis profile file.");
    if (Object.keys(value).some(key => !["format", "version", "keyboard", "actionAbiDigest", "layers", "profile", "macros", "pdModeSource"].includes(key))) throw fail("This profile contains unsupported fields.");
    if (!Array.isArray(value.layers) || ![5, 8].includes(value.layers.length) || value.layers.some(keys => !Array.isArray(keys) || keys.length !== 60 || !keys.every(u16))) throw fail("A complete profile must contain all matrix layers.");
    if (!Array.isArray(value.macros) || value.macros.length !== 64) throw fail("A complete profile must contain all 64 macro slots.");
    const macros = value.macros.map(slot => {const bytes = base64(slot, 8192, "macro"); validateViaMacro(bytes); return bytes;});
    const profile = base64(value.profile, value.version === 2 ? 5088 : 4064, "profile data"), decoded = decodeProfileBlob(profile), domains = decoded.domains;
    if (decoded.schema.major !== value.version || domains.map(d => d.id).join() !== (value.version === 2 ? "16,32,48,64,80" : "16,32,48,64")) throw fail("The profile is missing configuration. Partial profiles cannot be restored as a complete backup.");
    const actionOptions = actionLimitsFor(value.version);
    const rgb = decodeRgbDomainV1(domains[0].payload), behaviors = decodeKeyBehaviorDomain(domains[1].payload, actionOptions), combos = decodeComboDomainV1(domains[2].payload, actionOptions), settings = decodeSettings(domains[3].payload);
    const pdModes = value.version === 2 ? decodePdDomain(domains[4].payload) : undefined;
    // Settings may be one version ahead of the document: v3 names the VIA
    // macros inside a schema-2 profile.
    const settingsVersion = settings.formatVersion ?? 1;
    if (rgb.formatVersion !== value.version || !(settingsVersion === value.version || (value.version === 2 && settingsVersion >= 3)) || domains[3].version !== settingsVersion) throw fail("Profile domain versions disagree.");
    if (rgb.layerColors.length !== value.layers.length || rgb.layerColors.some(row => row.layerId >= 8)) throw fail("RGB does not cover all eight layers.");
    // A binding for an empty slot is allowed, because the keyboard allows it:
    // the mode keycodes are a fixed registry, and the runtime refuses to
    // activate a slot whose record is empty rather than misbehaving
    // (`noah_effective_pd_for_mask` returns nothing, and both activate and lock
    // bail out on that). So a pointing-mode key stays where it is and does
    // nothing until its slot is configured again. They are counted here so the
    // interface can say so before anyone wonders why a key went quiet.
    const danglingPdBindings = new Map();
    const checkPd = (id, source) => {
        if (!pdModes || id === undefined || pdModes[id]?.kind) return;
        danglingPdBindings.set(id, (danglingPdBindings.get(id) || 0) + 1);
        void source;
    };
    const checkNativePd = (code, source) => checkPd(pdSlotOfCode(code), source);
    if (pdModes) value.layers.flat().forEach(code => checkNativePd(code, "layer"));
    const checkAction = action => {
        if ([4, 5].includes(action.kind)) checkPd(action.operand, "action");
        if (action.kind === 1) checkNativePd(action.operand, "action");
        if ([2, 3].includes(action.kind) && action.operand >= 8) throw fail("A profile action references a missing layer.");
    };
    walkActions(behaviors, checkAction); walkActions(combos, checkAction);
    if (value.pdModeSource !== undefined) {
        const source = value.pdModeSource;
        if (value.version !== 1 || !source || source.version !== 1 || ![0xdcb00959, 0xeb80829c].includes(source.actionAbiDigest) || !Number.isInteger(source.compiledDefaultDigest) || source.compiledDefaultDigest < 1 || source.compiledDefaultDigest > 0xffffffff || Object.keys(source).some(key => !["version", "actionAbiDigest", "compiledDefaultDigest", "domain"].includes(key))) throw fail("Unsupported pointing-mode migration source.");
        const modes = decodePdDomain(base64(source.domain, 776, "pointing-mode source"));
        if (modes.some((mode, id) => mode.kind !== [2, 1, 1, 1, 1, 2, 0, 0][id])) throw fail("The migration source does not represent the six legacy modes.");
    }
    const layout = Buffer.alloc(value.layers.length * 120); value.layers.flat().forEach((v, id) => layout.writeUInt16BE(v, id * 2));
    if (capabilities?.compiledLayerCount === 8 && value.layers.length === 5) {
        return validateSnapshot(upgradeFiveLayerSnapshot(value), capabilities);
    }
    if (capabilities?.schema?.major === 2 && value.version === 1) return validateSnapshot(upgradePdSnapshot(value), capabilities);
    const capacity = capabilities?.viaMacroBytes ?? macroBankBytes(value);
    if (capabilities && (value.actionAbiDigest !== capabilities.actionAbiDigest || capabilities.compiledLayerCount !== value.layers.length || (capabilities.supportedDomainMask & (value.version === 2 ? 31 : 15)) !== (value.version === 2 ? 31 : 15))) throw fail("The connected firmware does not support this profile's action vocabulary or eight-layer storage.");
    const bank = macroBank(macros, capacity);
    return {document: value, profile, layout, macros: bank, settings, rgb, behaviors, combos,
        ...(pdModes ? {pdModes, danglingPdBindings: Object.fromEntries(danglingPdBindings)} : {})};
}
function upgradePdSnapshot(source) {
    if (source.layers?.length === 5) source = upgradeFiveLayerSnapshot(source);
    const value = validateSnapshot(source);
    if (source.version !== 1 || source.actionAbiDigest !== 0xeb80829c) throw fail("This profile does not use the supported legacy action vocabulary.");
    if (!source.pdModeSource) throw fail("This backup does not contain the old pointing-mode settings. Keep the original firmware and export a new backup using its matching PD readback bridge before changing storage geometry.");
    const slots = decodePdDomain(Buffer.from(source.pdModeSource.domain, "base64"));
    for (let id = 0; id < 6; id++) slots[id].dpi = id === 0 || id === 5 ? Math.max(100, value.settings.values[10]) : value.settings.values[10 + id];
    const rgb = value.rgb; rgb.formatVersion = 2;
    for (let id = 6; id < 8; id++) rgb.pdModeColors.push({pdModeId: id, color: {h: 0, s: 0, v: 0}, locality: 2});
    const settings = value.settings; settings.formatVersion = 2; settings.values.fill(0, 10, 15);
    const profile = encodeProfileBlob({schema: {major: 2, minor: 0}, domains: [
        {id: 16, version: 2, payload: encodeRgbDomainV1(rgb)}, {id: 32, version: 1, payload: encodeKeyBehaviorDomain(value.behaviors)},
        {id: 48, version: 1, payload: encodeComboDomainV1(value.combos)}, {id: 64, version: 2, payload: encodeSettings(settings)},
        {id: 80, version: 1, payload: encodePdDomain(slots)},
    ]});
    const result = {...source, version: 2, actionAbiDigest: 0x61072732, profile: profile.toString("base64")};
    delete result.pdModeSource;
    validateSnapshot(result);
    return result;
}

function walkActions(value, action) {
    if (!value || typeof value !== "object") return;
    if (Number.isInteger(value.kind) && Number.isInteger(value.operand)) {action(value); return;}
    for (const child of Object.values(value)) if (Array.isArray(child)) child.forEach(v => walkActions(v, action)); else if (child && typeof child === "object") walkActions(child, action);
}
// The fingerprint and summary of a document, from its decoded form when the
// caller already has it, so a document decoded once is not decoded again.
function fingerprintOf({document, profile, layout, macros}) {
    const bytes = Buffer.concat([profile, layout, macros, ...(document.pdModeSource ? [Buffer.from(document.pdModeSource.domain, "base64")] : [])]);
    return `${document.actionAbiDigest}:${crc32(bytes)}:${fnv1a32(bytes)}`;
}
const fingerprint = document => fingerprintOf(validateSnapshot(document));
// A snapshot's decoded form: the one it carries when that belongs to its very
// document (the draft decodes each revision once and hands it on), otherwise
// decoded now. A copy with another document never reuses a stale decode.
const decodedOf = snapshot => snapshot.decoded && snapshot.decoded.document === snapshot.document ? snapshot.decoded : validateSnapshot(snapshot.document);
// Moves each layer to its new place with everything it holds: its keys, name,
// RGB colour and per-layer settings, and the settings that point at it (the
// pointer and sniping layers). With keysFollow, the default, every layer key
// (MO, LT, TG, TO, TT, OSL, DF, LM, LOCK_LAYER...) on a layer, in a behaviour
// or on a combo is renumbered to reach the same layer; without it those keys
// keep their numbers and reach whatever layer now sits there.
//
// Any layer may be put at the bottom, which makes it the base: always on, and
// what every transparent key falls through to. A layer key that reaches the base
// reaches the bottom slot, whatever sits there, so TO(0) still goes home and a
// key that reached the new base now goes home too; the old base keeps nothing
// that reaches it. The startup layers are slots in the same way. The pointer and
// sniping layers and the combo references name a layer by what it holds, so
// they follow it.
function reorderLayers(document, order, names, {keysFollow = true} = {}) {
    if (document.layers?.length !== 8) throw fail("Layer ordering becomes available after the eight-layer update.");
    const validated = validateSnapshot(document), result = JSON.parse(JSON.stringify(document));
    if (!Array.isArray(order) || order.length !== 8 || new Set(order).size !== 8 || order.some(id => !Number.isInteger(id) || id < 0 || id >= 8)) throw fail("Include every layer once.");
    const remap = []; order.forEach((old, next) => {remap[old] = next;});
    const reach = old => (old === 0 ? 0 : remap[old]);
    function native(code) {
        if (!keysFollow) return code;
        if (code >= 0x4000 && code <= 0x4fff) {const layer = (code >> 8) & 15; if (layer >= 8) throw fail("A key points outside the layer bank."); return (code & 0xf0ff) | (reach(layer) << 8);}
        for (const start of [0x5200, 0x5220, 0x5240, 0x5260, 0x5280, 0x52c0, 0x52e0, 0x7e5c]) {
            const width = start === 0x7e5c ? 8 : 32;
            if (code >= start && code < start + width) {if (code - start >= 8) throw fail("A key points outside the layer bank."); return start + reach(code - start);}
        }
        // Layer-mod stores a four-bit layer followed by five modifier bits.
        if (code >= 0x5000 && code <= 0x51ff) {const layer = (code >> 5) & 15; if (layer >= 8) throw fail("Invalid layer-mod reference."); return (code & 0xfe1f) | reach(layer) << 5;}
        return code;
    }
    result.layers = order.map(old => document.layers[old].map(native));
    const {rgb, behaviors, combos, settings, pdModes} = validated;
    const actionOptions = actionLimitsFor(pdModes ? 2 : 1);
    const action = a => {if (!keysFollow) return; if ([2, 3].includes(a.kind)) a.operand = reach(a.operand); else if (a.kind === 1) a.operand = native(a.operand);};
    walkActions(behaviors, action); walkActions(combos, action);
    rgb.layerColors.forEach(row => {row.layerId = remap[row.layerId];}); rgb.layerColors.sort((a, b) => a.layerId - b.layerId);
    rgb.layerGroupRows.forEach(row => {if (row.selector !== 255) row.selector = remap[row.selector];});
    settings.names = names || order.map(old => settings.names[old]);
    settings.values[5] = remap[settings.values[5]]; settings.values[9] = remap[settings.values[9]];
    settings.values[23] = remap.reduce((mask, next, old) => mask | ((settings.values[23] >> old) & 1) << reach(old), 0);
    const oldReferences = settings.values[27]; settings.values[27] = order.reduce((packed, old, next) => (packed | remap[(oldReferences >>> (old * 4)) & 15] << (next * 4)) >>> 0, 0);
    result.profile = encodeProfileBlob({schema: {major: document.version, minor: 0}, domains: [
        {id: 16, version: document.version, payload: encodeRgbDomainV1(rgb)}, {id: 32, version: 1, payload: encodeKeyBehaviorDomain(behaviors, actionOptions)},
        {id: 48, version: 1, payload: encodeComboDomainV1(combos, actionOptions)}, {id: 64, version: settings.formatVersion ?? document.version, payload: encodeSettings(settings)},
        ...(pdModes ? [{id: 80, version: 1, payload: encodePdDomain(pdModes)}] : []),
    ]}).toString("base64");
    validateSnapshot(result); return result;
}
function upgradeFiveLayerSnapshot(source) {
    if (source.actionAbiDigest !== 0xdcb00959) throw fail("This five-layer firmware vocabulary cannot be upgraded automatically.");
    const value = validateSnapshot(source), result = JSON.parse(JSON.stringify(source));
    const native = code => {
        if (code >= 0x7ffd && code <= 0x7fff) throw fail("A legacy user trigger has no corresponding slot in the eight-layer firmware.");
        return code >= 0x7e61 && code <= 0x7fff ? code + 3 : code;
    };
    result.layers = source.layers.map(layer => layer.map(native));
    while (result.layers.length < 8) result.layers.push(Array(60).fill(1));
    walkActions(value.behaviors, action => {if (action.kind === 1) action.operand = native(action.operand);});
    walkActions(value.combos, action => {if (action.kind === 1) action.operand = native(action.operand);});
    for (let id = 5; id < 8; id++) value.rgb.layerColors.push({layerId: id, color: {h: 0, s: 0, v: 0}, mode: 1});
    result.actionAbiDigest = 0xeb80829c;
    result.profile = encodeProfileBlob({domains: [
        {id:16,version:1,payload:encodeRgbDomainV1(value.rgb)}, {id:32,version:1,payload:encodeKeyBehaviorDomain(value.behaviors)},
        {id:48,version:1,payload:encodeComboDomainV1(value.combos)}, {id:64,version:1,payload:encodeSettings(value.settings)},
    ]}).toString("base64");
    return result;
}
const summary = document => summaryOf(validateSnapshot(document));
function summaryOf(value) {
    return {layers: value.document.layers.length, behaviors: value.behaviors.rows.length, combos: value.combos.length,
        macros: value.document.macros.filter(Boolean).length + (value.settings.macros || []).filter(bytes => bytes.length).length,
        names: value.settings.names.map((name, index) => layerName(value.settings.names, index))};
}
module.exports = {upgradePdSnapshot, createSnapshot, validateSnapshot, materializeProfile, macroSlots, macroBank, macroBankBytes, validateViaMacro, fingerprint, fingerprintOf, decodedOf, reorderLayers, summary, summaryOf};
