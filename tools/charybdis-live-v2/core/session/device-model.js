"use strict";

// Builds the model the webview renders, from device state.
//
// The webview never reads a file: it renders `model` and posts typed edits
// back, and everything in `model` comes from what the keyboard reported.
//
// Domains the device cannot report yet come back empty rather than invented.
// An empty RGB tab is truthful; a tab populated from the authored source would
// be a lie about what the keyboard is running.

const keycodeCatalog = require("../data/keycode-catalog");
const {baseRgbForView, behaviorAliasesForView, behaviorRowsForView, combosForView, rgbForView, knownActionAbi} = require("./device-profile-view");
const {resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {dpiChoices} = require("../model/pointer-dpi");

const CATALOG_SOURCE = "vendored QMK keycode catalog";

// LEFT_THUMB → "Left thumb", LOCK_LAYER(2) → "Lock layer 2".
function semanticLabel(semantic) {
    const [, name, args] = /^([A-Z][A-Z0-9_]*)(?:\((.*)\))?$/.exec(semantic) || [, semantic, undefined];
    const words = name.replace(/_/g, " ").toLowerCase();
    return words.charAt(0).toUpperCase() + words.slice(1) + (args === undefined ? "" : ` ${args.replace(/\s+/g, "")}`);
}

function buildDeviceModel(state = {}) {
    const catalog = catalogViews();
    if (state.macroView && knownActionAbi(state.capabilities?.actionAbiDigest)) {
        for (const slot of [...state.macroView.viaMacros, ...state.macroView.hardcodedMacros]) {
            const native = keycodeCatalog.resolve(resolveNativeQmkExpression(slot.keycode, {})).name;
            const label = (slot.kind === "via" ? "VIA macro " : "User macro ") + slot.keycode.split("_").at(-1);
            catalog.aliases[native] = slot.keycode;
            catalog.labels[native] = label;
            catalog.labels[slot.keycode] = label;
        }
    }
    // A behaviour row keyed by a semantic target gives its keycode a name only
    // where the vocabulary has none better than a bare user slot: LOCK_LAYER(2)
    // reads "Lock layer 2" instead of "User 30". A key the catalog already
    // names — MO(1) is "Layer hold 1" — or one an earlier pass named, such as
    // "VIA macro 0", keeps that name, so one key reads the same on every
    // screen and in the picker whether or not a behaviour sits on it.
    if (state.committed?.state === "read" && state.committed.domains?.keyBehaviors) {
        const aliases = behaviorAliasesForView(state.committed.domains.keyBehaviors, state.capabilities);
        Object.assign(catalog.aliases, aliases);
        for (const [key, semantic] of Object.entries(aliases)) {
            const vocabulary = keycodeCatalog.resolve(resolveNativeQmkExpression(semantic, {}));
            const generic = !vocabulary.known || vocabulary.group === "user";
            if (!generic || (catalog.labels[key] !== undefined && catalog.labels[key] !== vocabulary.label)) continue;
            const label = semanticLabel(semantic);
            catalog.labels[key] = label;
            catalog.labels[semantic] = label;
            const entry = catalog.entries.find(entry => entry.key === key);
            if (entry) entry.label = label;
        }
    }
    // Every slot gets its keycodes, configured or not: the firmware's mode
    // keycodes are a fixed registry, so a key may be placed for a slot that is
    // still empty, and stays put when a slot is cleared. It does nothing until
    // the slot is configured, which the label says.
    if (knownActionAbi(state.capabilities?.actionAbiDigest)) for (const slot of state.committed?.domains?.pdModes || []) {
        for (const locked of [false, true]) {
            const name = slot.id < 6 ? ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"][slot.id] + (locked ? "_LOCK" : "") : `PD_SLOT_${slot.id}${locked ? "_LOCK" : ""}`;
            const code = slot.id < 6 ? 0x7e50 + slot.id + (locked ? 6 : 0) : 0x7ef0 + (slot.id - 6) * 2 + Number(locked);
            const title = slot.kind ? slot.name : `Slot ${slot.id + 1}`;
            const label = `${title} · ${locked ? "toggle" : "hold"}${slot.kind ? "" : " (empty)"}`, native = keycodeCatalog.resolve(code).name;
            catalog.aliases[native] = name; catalog.labels[native] = label; catalog.labels[name] = label;
            const entry = catalog.entries.find(entry => entry.keycode === code);
            const presentation = {value: name, key: name, label, group: "Pointing modes", aliases: [native], keycode: code, searchTerms: [name, label], search: `${name} ${label}`.toLowerCase(), searchCompact: `${name}${label}`.toLowerCase()};
            if (entry) Object.assign(entry, presentation); else catalog.entries.push(presentation);
        }
    }
    return {
        layers: layersFromDevice(state.layout, catalog.labels, catalog.aliases),

        // Read off the keyboard when the committed profile has been read;
        // empty rather than fabricated before that.
        pdModes: state.committed?.domains?.pdModes || [],
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
        hardcodedMacros: state.macroView?.hardcodedMacros || [],
        macroEditing: {identity: state.macroView?.identity || "", writable: Boolean(state.macroView) && state.capabilities?.compiledLayerCount === 8 && !state.busy},
        behaviorTimingDefaults: state.settingsView?.timing || {},
        configDefaults: state.settingsView?.sections || [],
        settingsEditing: {identity: state.settingsView?.identity || "", writable: Boolean(state.settingsView) && state.capabilities?.compiledLayerCount === 8 && !state.busy},
        macroPayloadKeycodes: state.macroView?.macroPayloadKeycodes || [],

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
function layersFromDevice(layout, labels, aliases = {}) {
    if (!layout || layout.state !== "read" || !Array.isArray(layout.layers)) {
        return [];
    }
    return layout.layers.map((entry) => ({
        name: `Layer ${entry.layer}`,
        index: entry.layer,
        positions: entry.keys.map((key) => {
            const resolved = {...key.resolved, label: labels[key.resolved.name] || key.resolved.label};
            // A dual-role key shows what it taps, named the way that keycode is
            // named everywhere else: `/`, not `SLASH`.
            if (resolved.tap && labels[resolved.tap]) resolved.tapLabel = labels[resolved.tap];
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
        return `L${resolved.layer}`;
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
            error: state.error?.message || state.liveApply?.error?.message || "",
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

module.exports = {buildDeviceModel};
