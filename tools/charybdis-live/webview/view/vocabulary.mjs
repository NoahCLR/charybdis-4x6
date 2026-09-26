// The words for what the keyboard stores, as the host sends them.
//
// `model.vocabulary` is core/model/vocabulary.js: the one table the review is
// written from. Every editor names things from here too, so a dropdown, a
// hover card and the review never call one value two things. A value the
// table does not know reads as itself rather than disappearing.

const NONE = {holdHelpers: [], tiers: {}, branches: [], localities: [], paintModes: [], fadeModes: [], tapCommit: [],
    feedbackOwners: [], stages: [], pointing: {kinds: [], axes: [], invert: [], pointerLayer: [], buttons: [],
        emptyDirection: [], modifierPolicy: [], scrollFields: []}, modifiers: [], comboOptions: {}};

export const vocabulary = (model) => model?.vocabulary || NONE;

export const word = (list, key) => ((list || []).find(([id]) => id === key) || [, String(key ?? "")])[1];

// A branch by how many taps start it: "Double tap".
export const branchName = (model, count) => vocabulary(model).branches[count - 1] || `${count} taps`;

// The lighting stages in paint order, each with its id, words and bit.
export const stageOrder = (model) => vocabulary(model).stages;

// A pointing slot's name as the host gives it.
export const slotCalled = (model, id) => (model?.pdModes || []).find((slot) => slot.id === id)?.displayName || `Slot ${id}`;

// How a tier runs, in the editor's words: a tap tier only sends.
export const helperWord = (model, kind, helper) => kind === "tap"
    ? vocabulary(model).tapSends || "tap sends" : word(vocabulary(model).holdHelpers, helper);

// A tier's name: Tap, Hold, Long hold.
export const tierName = (model, kind) => vocabulary(model).tiers[kind] || kind;
