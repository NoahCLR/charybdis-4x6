// The webview's whole state: what the keyboard said, and where the person is
// standing in the app. Edits never change the model here — they are posted to
// the host, which answers with a new model. The draft lives on that side too,
// so what is drawn is always what would be applied.

import {layersOn, toggleLayer} from "./view/layer-set.mjs";

const vscode = acquireVsCodeApi();

const openGroups = () => ({here: true, branches: true, combos: true, through: false, belowBranches: false, belowCombos: false, elsewhere: false});

export const state = {
    screen: "keys",
    layer: 0,
    layersOn: [],       // layers previewed on with `layer`, which is the highest of them (view/layer-set.mjs)
    selected: 0,
    tab: "key",
    behaviourRow: null,
    behaviourRowShown: null,
    behaviourRoute: null, // {row, group}: the group the picked behaviour was picked in, so the board rings that route only
    // Every tab that answers "what does this layer reach" is grouped the same
    // way and opens the same way: what the layer holds itself, and the rest a
    // click away.
    groups: {behaviours: {...openGroups(), branches: false, combos: false}, macros: openGroups(), combos: openGroups(), pointing: openGroups()},
    // The row picked in each tab. The board rings the keys that reach it, so
    // a row in a table can answer "where do I press for this".
    reachRow: {macros: null, combos: null, pointing: null},
    cell: null,
    reveal: null,        // a selector the next render scrolls into view and marks, then forgets
    pickCombo: null,     // a combo id to pick in the Combos tab, wherever this layer lists it
    settingsOpen: [],    // settings sections opened by hand or by a Show, so a render keeps them open
    cellHow: null,       // {cell, keycode, helper, repeatHz}: how the open empty cell runs, chosen before it sends anything
    // The combo builder, opened and closed as one (openComboBuilder):
    //   inputs      layout indexes picked on the current layer
    //   inputCodes  layoutIndex → the input name the keyboard stores, for a combo being edited
    //   extraInputs inputs of the combo being edited that this layer cannot reach
    //   form        the builder's fields, kept across renders
    //   awaiting    a Keep or Delete posted; the builder closes when the host accepts it
    combo: {open: false, picking: false, inputs: [], inputCodes: {}, extraInputs: [],
        form: {output: "", termMs: "", mustHold: false, mustTap: false, ordered: false}, awaiting: false, editId: null},
    placement: null,     // {keycode, label}: next board click places it on the current layer
    retarget: null,      // {from, to, existing}: a behaviour move waiting on overwrite / swap / cancel
    keyClipboard: null,  // {keycode, label}: the key ⌘C copied, for ⌘V onto the selected key
    stage: "layers",
    layersOpen: false,  // the layer-stack panel on the layer row
    layersAsked: false, // its stack is requested once per opening
    pdSlot: 0,
    pdKind: null,      // movement selection while the rebuilt form catches up: {slot, kind}
    pdButtons: null,   // {slot, rows: {index: override}}: button overrides whose kind is chosen but not yet its shortcut or modifiers
    pdAdvanced: false,  // Advanced open on the pointing editor, whichever slot is shown
    applyDismissed: 0,  // the failed Apply (by id) the person closed, so it stays closed
    pdPreview: false,
    feedbackRow: "hold",
    macroSlot: null,
    macroSearch: "",
    macroForms: {},       // keycode → {draft, step, cursor}: one macro slot's unsaved text, the step being built, where it goes
    recording: null,     // {slot, before, last, captured} only while a take is being captured
    lastTake: null,      // {slot, before}: the finished take Clear take can undo
    recordDelays: true,
    recordMode: "compact",
    recordDelayThreshold: 30,
    recordDelayRound: 10,
    ledPicks: [],
    trackball: false,
    rowColour: {h: "0", s: "0", v: "0"},
    ledRow: {target: "layer", owner: "", source: ""}, // the LED group row being built, kept across renders
    settingsSearch: "",
    overlay: null,
    confirmChecks: null, // the draft revision whose checks Apply is asking about
    picker: null,
    notice: "",
    error: "",
};

// The combo builder, opened for a combo (or none, for a new one) and closed
// again. Its fields live here rather than in the DOM, because a board click
// while picking inputs redraws the whole screen.
export function openComboBuilder(combo = null, inputs = {positions: [], codes: {}, extras: []}) {
    state.combo = {
        open: true, picking: false, awaiting: false, editId: combo?.id ?? null,
        form: {output: combo?.output || "", termMs: String(combo?.termMs ?? ""), mustHold: Boolean(combo?.mustHold), mustTap: Boolean(combo?.mustTap), ordered: Boolean(combo?.ordered)},
        inputs: inputs.positions.slice(), inputCodes: {...inputs.codes}, extraInputs: inputs.extras.slice(),
        // The combo's inputs in stored order; one removed here and picked
        // again counts as added (edits.comboInputOrder).
        storedOrder: (combo?.inputs || []).slice(),
    };
}
export function closeComboBuilder() {
    openComboBuilder();
    state.combo.open = false;
}

// Undo, redo, discard and rebase replace the draft under the forms. Every form
// that holds its own unstaged text starts again from the model, so nothing on
// screen shows an edit the draft no longer has.
// One macro slot's editor state, and a change to part of it.
export const macroForm = (keycode) => state.macroForms[keycode] || {};
export function setMacroForm(keycode, fields) {
    state.macroForms = {...state.macroForms, [keycode]: {...state.macroForms[keycode], ...fields}};
}

export function resetDraftForms() {
    // A take being recorded is typing in progress, not a stale form: its
    // slot keeps the text it is building on.
    const take = state.recording?.slot;
    state.macroForms = take && state.macroForms[take] ? {[take]: {draft: state.macroForms[take].draft}} : {};
    state.lastTake = null;
    state.pdKind = null;
    state.pdButtons = null;
    state.retarget = null;
    closeComboBuilder();
}

let model = null;
let renderer = () => {};

export const getModel = () => model;
export const setModel = (next) => { model = next; };
export const setRenderer = (fn) => { renderer = fn; };
export const render = () => renderer();

export function post(message) {
    const draft = model?.draft;
    vscode.postMessage(draft ? {draftId: draft.id, draftRevision: draft.revision, ...message} : message);
}

export const layers = () => getModel()?.layers || [];
export const currentLayer = () => layers()[state.layer] || layers()[0];
// The layers previewed on under the current one, and whether any are: the board
// then shows what the keyboard answers with, not what the layer stores.
export const heldLayers = () => layersOn(state.layer, state.layersOn, layers().length);
export const previewing = () => heldLayers().length > 0;
// A plain pick shows one layer; ⌘-click adds or removes one from the preview.
export function showLayer(index) {
    state.layer = index;
    state.layersOn = [];
}
export function toggleLayerOn(index) {
    const next = toggleLayer(state.layer, heldLayers(), index);
    state.layer = next.top;
    state.layersOn = next.on;
}
export const layerName = (layer) => layer?.displayName || layer?.name || "";
export const positionAt = (layer, index) =>
    (layer?.positions || []).find((position) => position.layoutIndex === index);
export const selectedPosition = () => positionAt(currentLayer(), state.selected)
    || (currentLayer()?.positions || [])[0];

// Editing is only offered where the keyboard says it is possible; everywhere
// else the control stays visible and disabled, with the reason.
export const writable = () => Boolean(getModel()?.draft?.matching && !getModel()?.draft.stale && !getModel()?.draft.busy && getModel()?.device?.connected);

// Whether an area can be edited now, decided in one place: every edit goes
// into the draft, so the draft must be this keyboard's, current and idle, and
// the area's own capability must hold. Exporting only reads the keyboard.
const AREA_CAPABILITY = {
    settings: (model) => Boolean(model?.settingsEditing?.writable),
    macros: (model) => Boolean(model?.macroEditing?.writable),
    pointing: (model) => Boolean(model?.pdModeEditing?.writable),
    combos: (model) => model?.comboReadback?.writable !== false,
    layers: (model) => Boolean(model?.portable?.available && model.portable.eightLayers && !model.portable.busy),
    import: (model) => Boolean(model?.portable?.available && model.portable.eightLayers && !model.portable.busy),
};
export function canEdit(area) {
    const model = getModel();
    if (area === "export") return Boolean(model?.portable?.available && !model.portable.busy);
    return writable() && (AREA_CAPABILITY[area] ? AREA_CAPABILITY[area](model) : true);
}
