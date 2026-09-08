"use strict";
const {decodeProfileBlob, encodeProfileBlob, crc32, fnv1a32} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../schema/rgb-domain-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../schema/key-behavior-domain-v1");
const {decodeComboDomainV1, encodeComboDomainV1} = require("../schema/combo-domain-v1");
const {decodeSettings, encodeSettings} = require("../schema/settings-domain-v1");
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
    const domains = [0x10, 0x20].map(id => {
        const domain = live.domains.find(d => d.id === id) || fallback.domains.find(d => d.id === id);
        if (!domain) throw fail("The keyboard did not report every profile domain.");
        return domain;
    });
    const rows = combos.rows.map(row => ({...row, inputs: row.inputs.map(operand => ({kind: 1, operand})), output: {kind: 1, operand: row.output}}));
    domains.push({id: 0x30, version: 1, payload: encodeComboDomainV1(rows)}, {id: 0x40, version: 1, payload: settings});
    return encodeProfileBlob({domains});
}
function createSnapshot({profile, via, actionAbiDigest}) {
    const document = {format: "charybdis-profile", version: 1, keyboard: "charybdis-4x6", actionAbiDigest,
        layers: Array.from({length: via.layers}, (_, layer) => Array.from({length: 60}, (_, pos) => via.layout.readUInt16BE((layer * 60 + pos) * 2))),
        profile: profile.toString("base64"), macros: macroSlots(via.macros, via.macroSlots).map(bytes => bytes.toString("base64"))};
    validateSnapshot(document); return document;
}
function validateSnapshot(value, capabilities) {
    if (typeof value === "string") {
        if (Buffer.byteLength(value) > 100000) throw fail("This profile file is too large.");
        try {value = JSON.parse(value);} catch {throw fail("This is not a valid profile file.");}
    }
    if (!value || value.format !== "charybdis-profile" || value.version !== 1 || value.keyboard !== "charybdis-4x6" || !Number.isInteger(value.actionAbiDigest) || value.actionAbiDigest < 1 || value.actionAbiDigest > 0xffffffff) throw fail("Choose a supported Charybdis profile file.");
    if (Object.keys(value).some(key => !["format", "version", "keyboard", "actionAbiDigest", "layers", "profile", "macros"].includes(key))) throw fail("This profile contains unsupported fields.");
    if (!Array.isArray(value.layers) || ![5, 8].includes(value.layers.length) || value.layers.some(keys => !Array.isArray(keys) || keys.length !== 60 || !keys.every(u16))) throw fail("A complete profile must contain all matrix layers.");
    if (!Array.isArray(value.macros) || value.macros.length !== 64) throw fail("A complete profile must contain all 64 macro slots.");
    const macros = value.macros.map(slot => {const bytes = base64(slot, 8192, "macro"); validateViaMacro(bytes); return bytes;});
    const profile = base64(value.profile, 4064, "profile data"), domains = decodeProfileBlob(profile).domains;
    if (domains.map(d => d.id).join() !== "16,32,48,64") throw fail("The profile is missing configuration. Partial profiles cannot be restored as a complete backup.");
    const rgb = decodeRgbDomainV1(domains[0].payload), behaviors = decodeKeyBehaviorDomain(domains[1].payload), combos = decodeComboDomainV1(domains[2].payload), settings = decodeSettings(domains[3].payload);
    if (rgb.layerColors.length !== value.layers.length || rgb.layerColors.some(row => row.layerId >= 8)) throw fail("RGB does not cover all eight layers.");
    const checkAction = action => {
        if ([2, 3].includes(action.kind) && action.operand >= 8) throw fail("A profile action references a missing layer.");
    };
    walkActions(behaviors, checkAction); walkActions(combos, checkAction);
    const layout = Buffer.alloc(value.layers.length * 120); value.layers.flat().forEach((v, id) => layout.writeUInt16BE(v, id * 2));
    if (capabilities?.compiledLayerCount === 8 && value.layers.length === 5) {
        return validateSnapshot(upgradeFiveLayerSnapshot(value), capabilities);
    }
    const capacity = capabilities?.viaMacroBytes ?? (value.layers.length === 5 ? 7551 : 7191);
    if (capabilities && (value.actionAbiDigest !== capabilities.actionAbiDigest || capabilities.compiledLayerCount !== value.layers.length || (capabilities.supportedDomainMask & 15) !== 15)) throw fail("The connected firmware does not support this profile's action vocabulary or eight-layer storage.");
    const bank = macroBank(macros, capacity);
    return {document: value, profile, layout, macros: bank, settings, rgb, behaviors, combos};
}
function walkActions(value, action) {
    if (!value || typeof value !== "object") return;
    if (Number.isInteger(value.kind) && Number.isInteger(value.operand)) {action(value); return;}
    for (const child of Object.values(value)) if (Array.isArray(child)) child.forEach(v => walkActions(v, action)); else if (child && typeof child === "object") walkActions(child, action);
}
function fingerprint(document) {
    const {profile, layout, macros} = validateSnapshot(document);
    const bytes = Buffer.concat([profile, layout, macros]);
    return `${document.actionAbiDigest}:${crc32(bytes)}:${fnv1a32(bytes)}`;
}
function reorderLayers(document, order, names) {
    if (document.layers?.length !== 8) throw fail("Layer ordering becomes available after the eight-layer update.");
    const validated = validateSnapshot(document), result = JSON.parse(JSON.stringify(document));
    if (!Array.isArray(order) || order.length !== 8 || order[0] !== 0 || new Set(order).size !== 8 || order.some(id => !Number.isInteger(id) || id < 0 || id >= 8)) throw fail("Keep Base first and include every layer once.");
    const remap = []; order.forEach((old, next) => {remap[old] = next;});
    function native(code) {
        if (code >= 0x4000 && code <= 0x4fff) {const layer = (code >> 8) & 15; if (layer >= 8) throw fail("A key points outside the layer bank."); return (code & 0xf0ff) | (remap[layer] << 8);}
        for (const start of [0x5200, 0x5220, 0x5240, 0x5260, 0x5280, 0x52c0, 0x52e0, 0x7e5c]) {
            const width = start === 0x7e5c ? 8 : 32;
            if (code >= start && code < start + width) {if (code - start >= 8) throw fail("A key points outside the layer bank."); return start + remap[code - start];}
        }
        // Layer-mod stores a four-bit layer followed by five modifier bits.
        if (code >= 0x5000 && code <= 0x51ff) {const layer = (code >> 5) & 15; if (layer >= 8) throw fail("Invalid layer-mod reference."); return (code & 0xfe1f) | remap[layer] << 5;}
        return code;
    }
    result.layers = order.map(old => document.layers[old].map(native));
    const {rgb, behaviors, combos, settings} = validated;
    const action = a => {if ([2, 3].includes(a.kind)) a.operand = remap[a.operand]; else if (a.kind === 1) a.operand = native(a.operand);};
    walkActions(behaviors, action); walkActions(combos, action);
    rgb.layerColors.forEach(row => {row.layerId = remap[row.layerId];}); rgb.layerColors.sort((a, b) => a.layerId - b.layerId);
    rgb.layerGroupRows.forEach(row => {if (row.selector !== 255) row.selector = remap[row.selector];});
    settings.names = names || order.map(old => settings.names[old]);
    settings.values[5] = remap[settings.values[5]]; settings.values[9] = remap[settings.values[9]];
    settings.values[23] = remap.reduce((mask, next, old) => mask | ((settings.values[23] >> old) & 1) << next, 0);
    const oldReferences = settings.values[27]; settings.values[27] = order.reduce((packed, old, next) => (packed | remap[(oldReferences >>> (old * 4)) & 15] << (next * 4)) >>> 0, 0);
    result.profile = encodeProfileBlob({domains: [
        {id: 16, version: 1, payload: encodeRgbDomainV1(rgb)}, {id: 32, version: 1, payload: encodeKeyBehaviorDomain(behaviors)},
        {id: 48, version: 1, payload: encodeComboDomainV1(combos)}, {id: 64, version: 1, payload: encodeSettings(settings)},
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
function summary(document) {
    const value = validateSnapshot(document);
    return {layers: value.document.layers.length, behaviors: value.behaviors.rows.length, combos: value.combos.length,
        macros: value.document.macros.filter(Boolean).length + value.settings.macros.filter(bytes => bytes.length).length,
        names: value.settings.names.map((name, index) => name || (index ? `Layer ${index}` : "Base"))};
}
module.exports = {createSnapshot, validateSnapshot, materializeProfile, macroSlots, macroBank, validateViaMacro, fingerprint, reorderLayers, summary};
