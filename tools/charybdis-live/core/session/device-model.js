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

const CATALOG_SOURCE = "vendored QMK keycode catalog";

function buildDeviceModel(state = {}) {
    const catalog = catalogViews();
    return {
        // Repo concepts Studio carried. The live app has no repository, so it
        // reports the connected device instead of a profile directory.
        root: "",
        profiles: [],
        activeProfile: activeProfileFromDevice(state),
        files: {},

        layers: layersFromDevice(state.layout),
        customKeycodes: [],

        // Awaiting the committed-payload read. Empty, not fabricated.
        keyBehaviors: [],
        combos: [],
        viaMacros: [],
        hardcodedMacros: [],
        behaviorTimingDefaults: {},
        configDefaults: [],
        rgb: {},
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
function layersFromDevice(layout) {
    if (!layout || layout.state !== "read" || !Array.isArray(layout.layers)) {
        return [];
    }
    return layout.layers.map((entry) => ({
        name: `Layer ${entry.layer}`,
        index: entry.layer,
        positions: entry.keys.map((key) => ({
            layoutIndex: key.layoutIndex,
            keycode: key.resolved.name,
            display: displayFor(key.resolved),
            editLabel: key.resolved.label,
            row: key.row,
            column: key.column,
            value: key.keycode,
        })),
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
    return resolved.label.length <= 5 ? resolved.label : trim(resolved.name);
}

function trim(name) {
    const stripped = String(name || "").replace(/^KC_/, "");
    return stripped.length <= 5 ? stripped : stripped.slice(0, 5);
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
    const summary = status
        ? `generation ${status.committedGeneration} · ${hex(status.committedDigest)} · ${convergence(status)}`
        : "";
    const layers = state.layout?.state === "read" ? state.layout.layers.length : 0;
    const subtitle = layers
        ? `${layers} layers read from the keyboard`
        : "Connected. Use Read from keyboard to load its layout.";
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
    notes.push(
        "RGB, key behaviours, combos and macros need the committed profile read, which is not implemented yet."
    );
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
