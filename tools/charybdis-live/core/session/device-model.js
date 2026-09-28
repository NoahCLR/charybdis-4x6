"use strict";

// Builds the model the webview renders, from device state.
//
// The webview never reads a file: it renders `model` and posts typed edits
// back, and everything in `model` comes from what the keyboard reported.
//
// Domains the device cannot report yet come back empty rather than invented.
// An empty RGB tab is truthful; a tab populated from the authored source would
// be a lie about what the keyboard is running.

const {PD_BINDINGS} = require("../data/pd-bindings");
const {VOCABULARY, slotName} = require("../model/vocabulary");
const keycodeCatalog = require("../data/keycode-catalog");
const {baseRgbForView, behaviorRowsForView, combosForView, rgbForView} = require("./device-profile-view");
const {keyLabel, profileKeyNames} = require("../model/key-names");
const {layerName} = require("../model/vocabulary");
const {knownActionAbi, layerRef, nativeCode} = require("../schema/actions");
const {dpiChoices} = require("../model/pointer-dpi");
const {PROFILE_WIRE_FEATURES} = require("../protocol/profile-wire-v1");

const CATALOG_SOURCE = "vendored QMK keycode catalog";

function buildDeviceModel(state = {}) {
    const catalog = catalogViews();
    // What this profile calls its keys, laid over the catalogue: one rule for
    // the screens and the review (model/key-names.js).
    const names = profileKeyNames({
        layers: state.committed?.domains?.settings?.names,
        actionsKnown: knownActionAbi(state.capabilities?.actionAbiDigest),
        macros: state.macroView?.viaMacros,
        customKeys: state.customKeyView?.keys,
        behaviors: state.committed?.state === "read" ? state.committed.domains?.keyBehaviors?.rows : [],
        pdModes: state.committed?.domains?.pdModes,
    });
    Object.assign(catalog.aliases, names.aliases);
    Object.assign(catalog.labels, names.labels);
    for (const entry of catalog.entries) if (names.labels[entry.key] !== undefined) entry.label = names.labels[entry.key];
    // A pointing-mode keycode is offered in the picker under its slot.
    for (const {name, native, code, label} of names.pointing) {
        const entry = catalog.entries.find(entry => entry.keycode === code);
        const presentation = {value: name, key: name, label, group: "Pointing modes", aliases: [native], keycode: code, searchTerms: [name, label], search: `${name} ${label}`.toLowerCase(), searchCompact: `${name}${label}`.toLowerCase()};
        if (entry) Object.assign(entry, presentation); else catalog.entries.push(presentation);
    }
    // So is each custom key, under its name.
    for (const {name, native, code, label} of names.custom) {
        const entry = catalog.entries.find(entry => entry.keycode === code);
        const presentation = {value: name, key: name, label, group: "Custom keys", aliases: [native], keycode: code, searchTerms: [name, label], search: `${name} ${label}`.toLowerCase(), searchCompact: `${name}${label}`.toLowerCase()};
        if (entry) Object.assign(entry, presentation); else catalog.entries.push(presentation);
    }
    // Every keycode the profile holds is in the table by its name here, so the
    // interface names any of them by keycode and never composes a name itself:
    // a layer-tap cannot be listed ahead of time, one per layer and tap key.
    for (const value of profileKeycodes(state)) catalog.labels[keycodeCatalog.resolve(value).name] = keyLabel(names, value);
    return {
        // The words for every value the keyboard stores, so the interface
        // names things as the review does (model/vocabulary.js).
        vocabulary: VOCABULARY,
        layers: layersFromDevice(state.layout, catalog.labels, catalog.aliases, names.layers),

        // Read off the keyboard when the committed profile has been read;
        // empty rather than fabricated before that.
        // Each slot with the name a person reads for it, one rule for every
        // surface (model/vocabulary.js slotName).
        // and the keycodes that bind it (data/pd-bindings.js), so the interface
        // names and finds a pointing-mode key without a registry of its own.
        pdModes: (state.committed?.domains?.pdModes || []).map((slot) => ({...slot, displayName: slotName(slot), binding: PD_BINDINGS[slot.id]})),
        pdModeEditing: {writable: Boolean(state.capabilities?.supportedDomainMask & 16) && state.committed?.state === "read" && !state.committed.failures?.length && !state.busy,
            dpiChoices: dpiChoices({normalSpeed: true})},
        keyBehaviors: committedKeyBehaviors(state.committed),
        behaviorEditing: {
            busy: Boolean(state.busy),
            writable: Boolean(state.capabilities?.supportedDomainMask & 2) && state.committed?.state === "read" && !state.committed.failures?.length && !state.busy,
            maxTapStepsPerBehavior: state.capabilities?.maxTapStepsPerBehavior || 5,
        },
        profileIdentity: state.committed?.state === "read" ? {source: state.committed.source, generation: state.committed.generation, digest: state.committed.digest, originHalf: state.committed.originHalf} : null,
        rgb: {...committedRgb(state.committed),
            ...(Number.isInteger(state.settingsView?.brightnessMax) ? {maximumBrightness: state.settingsView.brightnessMax} : {}),
            baseEffect: baseRgbForView(state.baseRgb, state.settingsView?.brightnessMax)},

        // Independently read native combo definitions.
        combos: combosForView(state.combos, catalog.labels),
        comboReadback: state.combos ? {...state.combos, rows: undefined, writable: Boolean(state.capabilities?.supportedDomainMask & 4) && state.committed?.state === "read" && !state.busy} : {state: "unread"},
        viaMacros: state.macroView?.viaMacros || [],
        macroNameSpace: state.macroView?.names || null,
        macroBank: state.macroView?.macroBank || null,
        macroEditing: {identity: state.macroView?.identity || "", writable: Boolean(state.macroView) && state.capabilities?.compiledLayerCount === 8 && !state.busy},
        behaviorTimingDefaults: state.settingsView?.timing || {},
        configDefaults: state.settingsView?.sections || [],
        settingsEditing: {identity: state.settingsView?.identity || "", writable: Boolean(state.settingsView) && state.capabilities?.compiledLayerCount === 8 && !state.busy},
        macroPayloadKeycodes: state.macroView?.macroPayloadKeycodes || [],
        // The 64 named custom keys, on a keyboard with the keycode blocks.
        customKeys: state.customKeyView?.keys || [],
        customKeyNameSpace: state.customKeyView?.names || null,
        customKeyEditing: {identity: state.customKeyView?.identity || "", writable: Boolean(state.customKeyView) && !state.busy},

        // TT(), OSL() and TO() are offered only by a keyboard that runs them
        // through its own layer ownership (Profile Wire feature bit 14).
        ownsLayerKeys: Boolean(state.capabilities?.featureFlags & PROFILE_WIRE_FEATURES.OWNED_LAYER_TOGGLES),
        qmkKeycodes: catalog.entries,
        qmkKeyLabels: catalog.labels,
        qmkKeycodeAliases: catalog.aliases,
        qmkKeycodeSource: CATALOG_SOURCE,

        diagnostics: diagnosticsFor(state),

        // The rail's header: which keyboard this is, and its health.
        device: deviceHeader(state),
    };
}

// The UI keys layers by name and the device only knows indexes, so synthesise
// stable names. They are display strings, not identifiers from source.
function layersFromDevice(layout, labels, aliases = {}, layerNames = []) {
    if (!layout || layout.state !== "read" || !Array.isArray(layout.layers)) {
        return [];
    }
    return layout.layers.map((entry) => ({
        name: layerRef(entry.layer),
        index: entry.layer,
        positions: entry.keys.map((key) => {
            const resolved = {...key.resolved, label: labels[key.resolved.name] || key.resolved.label};
            // A dual-role key shows what it taps, named the way that keycode is
            // named everywhere else: `/`, not `SLASH`.
            if (resolved.tap && labels[resolved.tap]) resolved.tapLabel = labels[resolved.tap];
            // A layer key names its layer on the cap by the layer's name.
            if (resolved.layer !== undefined) resolved.layerLabel = layerName(layerNames, resolved.layer);
            return {
                layoutIndex: key.layoutIndex,
                keycode: key.resolved.name,
                // What the value means, beside what the keyboard calls it. A
                // behaviour row, a macro slot and a pointing mode are all named
                // semantically, while a position arrives as `QK_USER_16` or as
                // bare hex, so every lookup that crosses that gap matches on
                // this and nothing has to re-derive the mapping.
                semantic: aliases[key.resolved.name] || key.resolved.name,
                display: displayFor(resolved),
                editLabel: resolved.label,
                ...(resolved.layerLabel ? {layerLabel: resolved.layerLabel} : {}),
                row: key.row,
                column: key.column,
                value: key.keycode,
            };
        }),
    }));
}

// Short enough for a key cap, never invented: an unresolved keycode shows its
// hex rather than being blanked or guessed at.
function displayFor(resolved) {
    if (resolved.name === "KC_TRANSPARENT") {
        return "▽";
    }
    if (resolved.name === "KC_NO") {
        return "";
    }
    if (resolved.kind === "layer") {
        return resolved.layerLabel;
    }
    if (resolved.kind === "layer-tap" || resolved.kind === "mod-tap") {
        return resolved.tapLabel || trim(resolved.tap);
    }
    // The SVG renderer scales labels to fit. Truncating here loses shortcut
    // modifiers and makes distinct unknown device IDs look identical.
    return resolved.label;
}

function trim(name) {
    const stripped = String(name || "").replace(/^KC_/, "");
    return stripped.length <= 5 ? stripped : stripped.slice(0, 5);
}

// Every keycode value the profile stores: on its layers, in its behaviours and
// in its combos.
function profileKeycodes(state) {
    const values = [];
    if (state.layout?.state === "read") for (const layer of state.layout.layers || []) for (const key of layer.keys) values.push(key.keycode);
    const behaviors = state.committed?.state === "read" ? state.committed.domains?.keyBehaviors?.rows || [] : [];
    if (knownActionAbi(state.capabilities?.actionAbiDigest)) for (const row of behaviors) {
        for (const action of [row.target, ...row.steps.flatMap((step) => [step.tap, step.hold?.action, step.longHold?.action])]) {
            if (action) values.push(nativeCode(action));
        }
    }
    if (state.combos?.state === "read") for (const row of state.combos.rows) values.push(...row.inputs, row.output);
    return values.filter((value) => Number.isInteger(value));
}

// The decoded domains reach the UI only once the whole payload verified, so a
// half-read profile is never rendered as if it were the keyboard's state.
function committedRgb(committed) {
    return committed?.state === "read" && committed.domains?.rgb ? rgbForView(committed.domains.rgb) : {};
}

function committedKeyBehaviors(committed) {
    const decoded = committed?.state === "read" ? committed.domains?.keyBehaviors : undefined;
    if (!decoded) {
        return [];
    }
    return behaviorRowsForView(decoded);
}

function deviceHeader(state) {
    const connected = Boolean(state.capabilities);
    if (!connected) {
        return {
            connected: false,
            label: "",
            summary: "",
            subtitle: "",
            health: {
                profile: "unavailable",
                converged: false,
                recoveryPending: false,
                restartNeeded: false,
                busy: false,
                phase: state.phase || "idle",
                error: state.error?.message || "",
            },
        };
    }
    const label = [state.device?.manufacturer, state.device?.product].filter(Boolean).join(" ") || "Charybdis";
    const status = state.status;
    // Say plainly when nothing is committed. "generation 0" reads like a real
    // generation and it is not one.
    const summary = !status
        ? ""
        : state.committed?.source === "compiled" || status.committedGeneration === 0
            ? `no committed profile · running compiled defaults · ${convergence(status)}`
            : `generation ${status.committedGeneration} · ${hex(status.committedDigest)} · ${convergence(status)}`;
    const layers = state.layout?.state === "read" ? state.layout.layers.length : 0;
    const parts = [];
    if (layers) {
        parts.push(`${layers} layers`);
    }
    if (state.committed?.state === "read") {
        parts.push(state.committed.source === "compiled" ? "compiled defaults" : `generation ${state.committed.generation}`);
    }
    const subtitle = parts.length
        ? `${parts.join(" and ")} read from the keyboard`
        : "Connected. Use Read from keyboard to load its configuration.";
    const converged = Boolean(status) && halvesConverged(status);
    return {
        connected: true,
        label,
        summary,
        subtitle,
        health: {
            profile: !status ? "unread" : converged ? "synced" : "attention",
            converged,
            recoveryPending: Boolean(state.mutationCompatibility?.recoveryPending || status?.candidatePending),
            // The other half never confirmed a cancelled save. Only a restart
            // releases it, and the keyboard refuses new saves until then.
            restartNeeded: Boolean(status?.peerCleanupPending),
            busy: Boolean(state.busy),
            phase: state.phase || "connected",
            error: state.error?.message || state.liveApply?.failure?.reason || "",
        },
    };
}

// Both halves agreeing is the thing worth seeing at a glance; anything else
// gets said plainly rather than hidden behind a green chip.
function convergence(status) {
    return halvesConverged(status) ? "both halves agree" : "halves not converged";
}

function halvesConverged(status) {
    return Boolean(status) &&
        status.peerKnown === true &&
        status.peerConverged === true &&
        !status.conflictCount &&
        status.activeGeneration === status.committedGeneration &&
        status.committedGeneration === status.peerGeneration;
}

function hex(value) {
    return typeof value === "number" ? `0x${(value >>> 0).toString(16).toUpperCase().padStart(8, "0")}` : "";
}

function diagnosticsFor(state) {
    const notes = [];
    if (!state.layout || state.layout.state !== "read") {
        notes.push("Layout has not been read from the keyboard yet.");
    }
    if (state.committed?.state === "read") {
        notes.push(
            state.committed.source === "compiled"
                ? `Showing the firmware's compiled defaults (${state.committed.byteLength} bytes). Nothing is committed to the keyboard yet, so this is what it runs.`
                : `Committed profile generation ${state.committed.generation} read from the keyboard (${state.committed.byteLength} bytes).`
        );
        for (const failure of state.committed.failures || []) {
            notes.push(`Domain 0x${failure.domainId.toString(16)} did not decode: ${failure.message}`);
        }
    } else {
        notes.push("RGB and key behaviours need the committed profile read.");
    }
    if (state.combos?.state === "read") notes.push(`${state.combos.rows.length} combos read from the keyboard. Combos are ${state.combos.enabled ? "enabled" : "disabled"}.`);
    else notes.push(state.combos?.error?.message || "Combos have not been read from the keyboard yet.");
    notes.push(state.capabilities?.supportedDomainMask & 8 ? "Complete profile backups include both macro banks and global keyboard settings." : "Macro payloads and policy defaults need the complete-profile firmware update.");
    return notes;
}

// The webview's catalog entries key on the keycode *name*; the vendored
// catalog keys on the numeric value, so map between them here.
function catalogViews() {
    const aliases = keycodeCatalog.aliasTable();
    const entries = [];
    const labels = {};
    for (const entry of keycodeCatalog.entries()) {
        const searchTerms = [entry.name, entry.label, ...entry.aliases].filter(Boolean);
        entries.push({
            value: entry.name,
            key: entry.name,
            label: entry.label,
            group: entry.group,
            aliases: entry.aliases,
            keycode: entry.value,
            searchTerms,
            search: searchTerms.join(" ").toLowerCase(),
            searchCompact: searchTerms.join("").toLowerCase(),
        });
        labels[entry.name] = entry.label;
        for (const alias of entry.aliases) {
            if (!labels[alias]) {
                labels[alias] = entry.label;
            }
        }
    }
    return {aliases, entries, labels};
}

// The keyboard's own header and diagnostics, without the rest of the model:
// with a draft open the panel shows these from the keyboard and everything
// else from the draft.
const deviceSummary = (state = {}) => ({device: deviceHeader(state), diagnostics: diagnosticsFor(state)});

module.exports = {buildDeviceModel, deviceSummary};
