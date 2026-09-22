// What a key shows: its legend, and the marks for everything it reaches.
//
// The model already resolved each position to a display label; this adds the
// layer-tap hint, the behaviour tiers and the combo badges, and nothing else.

const TRANSPARENT = new Set(["KC_TRANSPARENT", "KC_TRNS", "_______"]);
const DISABLED = new Set(["KC_NO", "XXXXXXX"]);

export function keyFace(position) {
    if (!position) return {main: "", sub: "", kind: "none"};
    const keycode = position.keycode || "";
    if (TRANSPARENT.has(keycode)) return {main: "▽", sub: "", kind: "transparent"};
    if (DISABLED.has(keycode)) return {main: "", sub: "", kind: "disabled"};
    const layerTap = /^LT\(\s*([A-Z0-9_]+)\s*,/.exec(keycode);
    const momentary = /^MO\(\s*([A-Z0-9_]+)\s*\)$/.exec(keycode);
    // A pointing key for a slot that holds nothing keeps its own name on the
    // cap; that it is inert belongs on the second line, where a key cap says
    // what is true of the key rather than of its label.
    const emptySlot = /^(.+) \(empty\)$/.exec(position.display || "");
    if (emptySlot) return {main: emptySlot[1], sub: "empty", kind: "key"};
    return {
        main: position.display || keycode,
        sub: layerTap ? shortLayer(layerTap[1]) : momentary ? "momentary" : "",
        kind: layerTap || momentary ? "layer" : "key",
    };
}

const shortLayer = (name) => name.replace(/^LAYER_/, "").toLowerCase();

// A position carries two names: the one the keyboard stores (`QK_USER_16`, or
// bare hex for a value the shipped vocabulary never named) and the one the rest
// of the profile uses (`DRAGSCROLL`, `VIA_MACRO_0`, `LEFT_THUMB`). Behaviour
// rows, macro slots and pointing slots are all keyed by the second, so every
// lookup from a key to what it reaches goes through this.
export const keyMeaning = (position) => position?.semantic || position?.keycode || "";

// How a keycode name is spoken in prose: the label the keyboard's own
// vocabulary gives it, so a behaviour row, a reach line and the key cap all
// call one key the same thing. Names the vocabulary does not cover — a device
// action outside the advertised ABI — keep their numeric identity.
export const actionLabel = (model, name) => model?.qmkKeyLabels?.[name]
    || model?.qmkKeyLabels?.[model?.qmkKeycodeAliases?.[name]]
    || String(name ?? "");

export const behaviourFor = (model, keycode) =>
    (model?.keyBehaviors || []).find((row) => row.keycode === keycode);

/**
 * Behaviours, grouped by how this layer reaches them.
 *
 * A key stored on this layer reaches its behaviour directly. A transparent key
 * lets the layer underneath answer, so a behaviour stored below still fires
 * while this layer is active — that one is reached *through* the layer, and it
 * is worth naming which layer answers. Everything else the profile carries is
 * somewhere this layer never reaches.
 *
 * `stack` is the layers in index order and `at` is the position of the current
 * one in it, because falling through only ever goes down.
 */
export function behaviourGroups(model, stack, at) {
    const layer = stack[at];
    const here = [...new Set((layer?.positions || []).map(keyMeaning))]
        .map((code) => behaviourFor(model, code)).filter(Boolean);

    // Walk down the way the firmware does: a transparent key falls through, and
    // anything else — a real keycode or a disabled one — stops the walk.
    const through = [];
    for (const position of layer?.positions || []) {
        if (keyFace(position).kind !== "transparent") continue;
        for (let below = at - 1; below >= 0; below -= 1) {
            const there = (stack[below]?.positions || [])
                .find((other) => other.layoutIndex === position.layoutIndex);
            if (!there || keyFace(there).kind === "transparent") continue;
            const row = behaviourFor(model, keyMeaning(there));
            if (row && !here.includes(row) && !through.some((entry) => entry.row === row)) {
                through.push({row, layer: stack[below]});
            }
            break;
        }
    }

    const reached = new Set([...here, ...through.map((entry) => entry.row)]);
    return {here, through, elsewhere: (model?.keyBehaviors || []).filter((row) => !reached.has(row))};
}

// One dot per tier the behaviour uses anywhere, carrying how many branches use
// it — the same shape the feedback stage flashes.
export function behaviourTiers(behaviour) {
    if (!behaviour) return [];
    const count = (pick) => (behaviour.steps || []).filter((step) => step[pick]).length;
    return [
        {kind: "tap", count: count("tap")},
        {kind: "hold", count: count("hold")},
        {kind: "long", count: count("longHold")},
    ].filter((tier) => tier.count > 0);
}

// The device stores only populated tap steps, but the editor is a matrix of
// every step the firmware supports. Keep empty columns visible so a one-step
// behaviour can grow without first inventing data for the other columns.
export function behaviourGridSteps(behaviour, advertisedMaximum = 5) {
    const requested = Number(advertisedMaximum);
    const maximum = Number.isInteger(requested) && requested > 0 ? Math.min(requested, 5) : 5;
    const populated = new Map((behaviour?.steps || []).map((step) => [step.tapCount, step]));
    return Array.from({length: maximum}, (_, tapCount) => populated.get(tapCount) || {tapCount});
}

// Whether this key is one of a combo's inputs. The keyboard reports per-layer
// input references only when its firmware tracks them, so this falls back to
// the keycodes themselves — and everything that answers "is this combo on this
// layer" goes through here, or the board's badges and the combo table disagree.
const comboTouches = (combo, position) => Array.isArray(combo?.inputPositions) && combo.inputPositions.length
    ? combo.inputPositions.includes(position.layoutIndex)
    : (combo?.inputs || []).some((input) => input === position.keycode || input === keyMeaning(position));

export function combosForKey(model, position) {
    if (!position) return [];
    return (model?.combos || []).filter((combo) => comboTouches(combo, position));
}

// The keys on a layer that carry this combo's inputs. A combo fires from the
// keycodes the active layer produces, so all of its inputs have to be there.
export function comboKeysOnLayer(layer, combo) {
    if (!layer || !combo) return [];
    const inputs = combo.inputs || [];
    const found = new Map();
    for (const position of layer.positions || []) {
        if (!comboTouches(combo, position)) continue;
        const input = inputs.find((name) => name === position.keycode || name === keyMeaning(position)) ?? position.keycode;
        if (!found.has(input)) found.set(input, position);
    }
    return [...found.values()];
}

export const macroKeycodes = (keycode) =>
    [...String(keycode || "").matchAll(/\b((?:VIA_)?MACRO_\d+)\b/g)].map((match) => match[1]);

// A layout position carries the keyboard's own name for its keycode, which for
// a pointing mode is a bare user keycode (`QK_USER_16`) or, for a slot the
// vocabulary does not name at all, its hex. So resolve the alias the model
// published first, then fall back to the number: an empty slot has no name to
// match on, and its keycodes exist regardless.
export function pointingSlotFor(model, keycode) {
    const slots = model?.pdModes || [];
    const alias = model?.qmkKeycodeAliases?.[keycode] ?? keycode;
    const name = String(alias || "").replace(/_LOCK$/, "");
    const legacy = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
    const index = legacy.indexOf(name);
    if (index >= 0) return slots.find((slot) => slot.id === index);
    const numbered = /^PD_SLOT_(\d)$/.exec(name);
    if (numbered) return slots.find((slot) => slot.id === Number(numbered[1]));
    const value = typeof keycode === "number" ? keycode : /^0x[0-9a-f]+$/i.test(String(keycode)) ? Number(keycode) : NaN;
    return Number.isInteger(value) ? slots.find((slot) => slotKeycodes(slot).includes(value)) : undefined;
}

// The two values a slot answers to: hold and toggle. Matching on the numbers
// rather than on names matters for an empty slot, whose record carries no name
// for the model to resolve — the keycode exists on the keyboard either way.
export const slotKeycodes = (slot) => slot.id < 6
    ? [0x7e50 + slot.id, 0x7e50 + slot.id + 6]
    : [0x7ef0 + (slot.id - 6) * 2, 0x7ef0 + (slot.id - 6) * 2 + 1];

// What still reaches a pointing slot. The keyboard keeps its mode keycodes
// whatever a slot holds, so a key bound to an empty slot is inert rather than
// invalid — and the interface has to say which keys those are.
export function bindingsForSlot(model, slot) {
    if (!slot) return {keys: [], behaviours: [], layers: []};
    const values = new Set(slotKeycodes(slot));
    const names = new Set([bindingKeycode(slot), `${bindingKeycode(slot)}_LOCK`]);
    for (const entry of model?.qmkKeycodes || []) {
        if (values.has(entry.keycode)) names.add(entry.value);
    }
    const keys = [];
    const layers = new Set();
    for (const layer of model?.layers || []) {
        for (const position of layer.positions || []) {
            if (!names.has(position.keycode) && !values.has(position.value)) continue;
            keys.push({layer, position});
            layers.add(layer.displayName || layer.name);
        }
    }
    const behaviours = (model?.keyBehaviors || []).filter((row) => (row.steps || []).some((step) =>
        ["tap", "hold", "longHold"].some((tier) => names.has(step[tier]?.action))));
    return {keys, behaviours, layers: [...layers]};
}

const LEGACY_BINDINGS = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
export const bindingKeycode = (slot) =>
    slot.id < LEGACY_BINDINGS.length ? LEGACY_BINDINGS[slot.id] : `PD_SLOT_${slot.id}`;
