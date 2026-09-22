// The webview's whole state: what the keyboard said, and where the person is
// standing in the app. Edits never change the model here — they are posted to
// the host, which answers with a new model. The draft lives on that side too,
// so what is drawn is always what would be applied.

const vscode = acquireVsCodeApi();

export const state = {
    screen: "keys",
    layer: 0,
    selected: 0,
    tab: "key",
    behaviourRow: null,
    cell: null,
    comboOpen: false,
    comboPicking: false,
    comboInputs: [],
    comboOutput: "",
    comboEditId: null,
    placement: null,     // {keycode, label}: next board click places it on the current layer
    stage: "layers",
    layersOpen: false,  // the layer-stack panel on the layer row
    layersAsked: false, // its stack is requested once per opening
    pdSlot: 0,
    pdKind: null,      // movement selection while the rebuilt form catches up: {slot, kind}
    pdAdvanced: null,
    pdPreview: false,
    feedbackRow: "hold",
    macroSlot: null,
    macroBank: "via",
    macroDrafts: {},
    macroSteps: {},
    macroCursors: {},
    recording: null,
    recordDelays: true,
    recordMode: "compact",
    recordDelayThreshold: 30,
    recordDelayRound: 10,
    ledPicks: [],
    trackball: false,
    rowColour: {h: "0", s: "0", v: "0"},
    settingsSearch: "",
    overlay: null,
    picker: null,
    notice: "",
    error: "",
    busyMessage: "",
};

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
export const writable = () => Boolean(getModel()?.draft && !getModel()?.draft.busy && getModel()?.device?.connected);
