"use strict";

// Builds the model the ported Studio UI renders, from device state.
//
// This is the whole point of the port. Studio's host produced this shape by
// parsing the authored C source files. The UI never knew that; it just renders
// `model` and posts typed edits back. So the same UI works unchanged as long as
// something produces the same shape — and here that something is the keyboard.
//
// Domains the device cannot report yet come back empty rather than invented.
// An empty RGB tab is truthful; a tab populated from the authored source would
// be a lie about what the keyboard is running.

const keycodeCatalog = require("../data/keycode-catalog");
const {baseRgbForView, behaviorAliasesForView, behaviorRowsForView, combosForView, rgbForView} = require("./device-profile-view");

const CATALOG_SOURCE = "vendored QMK keycode catalog";

function buildDeviceModel(state = {}) {
    const catalog = catalogViews();
    if (state.committed?.state === "read" && state.committed.domains?.keyBehaviors) {
        const aliases = behaviorAliasesForView(state.committed.domains.keyBehaviors, state.capabilities);
        Object.assign(catalog.aliases, aliases);
        for (const [key, semantic] of Object.entries(aliases)) {
            const words = semantic.replace(/_/g, " ").toLowerCase();
            const label = words.charAt(0).toUpperCase() + words.slice(1);
            catalog.labels[key] = label;
            const entry = catalog.entries.find(entry => entry.key === key);
            if (entry) entry.label = label;
        }
    }
    return {
        // Repo concepts Studio carried. The live app has no repository, so it
        // reports the connected device instead of a profile directory.
        root: "",
        profiles: [],
        activeProfile: activeProfileFromDevice(state),
        files: {},

        layers: layersFromDevice(state.layout, catalog.labels),
        customKeycodes: [],

        // Read off the keyboard when the committed profile has been read;
        // empty rather than fabricated before that.
        keyBehaviors: committedKeyBehaviors(state.committed),
        behaviorEditing: {busy: Boolean(state.busy), writable: Boolean(state.capabilities?.supportedDomainMask & 2) && state.committed?.state === "read" && !state.committed.failures?.length && !state.busy},
        profileIdentity: state.committed?.state === "read" ? {source: state.committed.source, generation: state.committed.generation, digest: state.committed.digest, originHalf: state.committed.originHalf} : null,
        rgb: {...committedRgb(state.committed), baseEffect: baseRgbForView(state.baseRgb)},

        // Independently read native combo definitions.
        combos: combosForView(state.combos, catalog.labels),
        comboReadback: state.combos ? {...state.combos, rows: undefined, writable: Boolean(state.capabilities?.supportedDomainMask & 4) && state.committed?.state === "read" && !state.busy} : {state: "unread"},
        // Still awaiting their own reads.
        viaMacros: [],
        hardcodedMacros: [],
        behaviorTimingDefaults: {},
        configDefaults: [],
        macroPayloadKeycodes: [],

        qmkKeycodes: catalog.entries,
        qmkKeyLabels: catalog.labels,
        qmkKeycodeAliases: catalog.aliases,
        qmkKeycodeSource: CATALOG_SOURCE,

        diagnostics: diagnosticsFor(state),

        // Not a Studio field. The ported header renders this instead of a
        // profile picker, because the thing being edited is a keyboard.
        device: deviceHeader(state),
    };
}

// The UI keys layers by name and the device only knows indexes, so synthesise
// stable names. They are display strings, not identifiers from source.
function layersFromDevice(layout, labels) {
    if (!layout || layout.state !== "read" || !Array.isArray(layout.layers)) {
        return [];
    }
    return layout.layers.map((entry) => ({
        name: `Layer ${entry.layer}`,
        index: entry.layer,
        positions: entry.keys.map((key) => {
            const resolved = {...key.resolved, label: labels[key.resolved.name] || key.resolved.label};
            return {
                layoutIndex: key.layoutIndex,
                keycode: key.resolved.name,
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
        return trim(resolved.tap);
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

function activeProfileFromDevice(state) {
    if (!state.capabilities) {
        return null;
    }
    const label = [state.device?.manufacturer, state.device?.product].filter(Boolean).join(" ");
    return {
        id: state.device?.id || "connected-device",
        name: label || "Connected Charybdis",
        keymapPath: "",
        configPath: "",
        rgbPath: "",
    };
}

function deviceHeader(state) {
    const connected = Boolean(state.capabilities);
    if (!connected) {
        return {connected: false, label: "", summary: "", subtitle: ""};
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
    return {connected: true, label, summary, subtitle};
}

// Both halves agreeing is the thing worth seeing at a glance; anything else
// gets said plainly rather than hidden behind a green chip.
function convergence(status) {
    const agreed =
        status.activeGeneration === status.committedGeneration &&
        status.committedGeneration === status.peerGeneration;
    return agreed ? "both halves agree" : "halves not converged";
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

// Studio's catalog entries key on the keycode *name*; ours key on the numeric
// value, so map between them here rather than reshaping the vendored file.
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
