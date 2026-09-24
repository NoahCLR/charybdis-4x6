// The webview's whole state: what the keyboard said, and where the person is
// standing in the app. Edits never change the model here — they are posted to
// the host, which answers with a new model. The draft lives on that side too,
// so what is drawn is always what would be applied.

const vscode = acquireVsCodeApi();

const openGroups = () => ({here: true, branches: true, combos: true, through: false, belowBranches: false, belowCombos: false, elsewhere: false});

export const state = {
    screen: "keys",
    layer: 0,
    selected: 0,
    tab: "key",
    behaviourRow: null,
    behaviourRowShown: null,
    behaviourRoute: null, // {row, group}: the group the picked behaviour was picked in, so the board rings that route only
    // Every tab that answers "what does this layer reach" is grouped the same
    // way and opens the same way: what the layer holds itself, and the rest a
    // click away.
    groups: {behaviours: openGroups(), macros: openGroups(), combos: openGroups(), pointing: openGroups()},
    // The row picked in each tab. The board rings the keys that reach it, so
    // a row in a table can answer "where do I press for this".
    reachRow: {macros: null, combos: null, pointing: null},
    cell: null,
    cellHow: null,       // {cell, keycode, helper, repeatHz}: how the open empty cell runs, chosen before it sends anything
    comboOpen: false,
    comboPicking: false,
    comboInputs: [],
    comboInputCodes: {},  // layoutIndex → the input name the keyboard stores, for a combo being edited
    comboExtraInputs: [], // inputs of the combo being edited that this layer cannot reach
    comboForm: {output: "", termMs: "", mustHold: false, mustTap: false, ordered: false}, // the builder's fields, kept across renders
    comboAwaiting: false, // a Keep or Delete posted; the builder closes when the host accepts it
    comboEditId: null,
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
    macroDrafts: {},
    macroSteps: {},
    macroCursors: {},
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
    picker: null,
    notice: "",
    error: "",
};

// The combo builder, opened for a combo (or none, for a new one) and closed
// again. Its fields live here rather than in the DOM, because a board click
// while picking inputs redraws the whole screen.
export function openComboBuilder(combo = null, inputs = {positions: [], codes: {}, extras: []}) {
    Object.assign(state, {
        comboOpen: true, comboPicking: false, comboAwaiting: false, comboEditId: combo?.id ?? null,
        comboForm: {output: combo?.output || "", termMs: String(combo?.termMs ?? ""), mustHold: Boolean(combo?.mustHold), mustTap: Boolean(combo?.mustTap), ordered: Boolean(combo?.ordered)},
        comboInputs: inputs.positions.slice(), comboInputCodes: {...inputs.codes}, comboExtraInputs: inputs.extras.slice(),
    });
}
export function closeComboBuilder() {
    openComboBuilder();
    state.comboOpen = false;
}

// Undo, redo, discard and rebase replace the draft under the forms. Every form
// that holds its own unstaged text starts again from the model, so nothing on
// screen shows an edit the draft no longer has.
export function resetDraftForms() {
    state.macroDrafts = {};
    state.macroSteps = {};
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
export const layerName = (layer) => layer?.displayName || layer?.name || "";
export const positionAt = (layer, index) =>
    (layer?.positions || []).find((position) => position.layoutIndex === index);
export const selectedPosition = () => positionAt(currentLayer(), state.selected)
    || (currentLayer()?.positions || [])[0];

// Editing is only offered where the keyboard says it is possible; everywhere
// else the control stays visible and disabled, with the reason.
export const writable = () => Boolean(getModel()?.draft?.matching && !getModel()?.draft.stale && !getModel()?.draft.busy && getModel()?.device?.connected);
