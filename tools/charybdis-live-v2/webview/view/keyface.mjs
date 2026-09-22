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

// One spelling per key: the picker says KC_ENT where a behaviour row says
// KC_ENTER, and an expression may or may not space its arguments. Every name
// inside it goes through the keyboard's alias table.
export const canonicalKeycode = (model, expression) => String(expression ?? "").replace(/\s+/g, "")
    .replace(/[A-Za-z_][A-Za-z0-9_]*/g, (name) => model?.qmkKeycodeAliases?.[name] ?? name);

// The behaviour row a picked expression would land on, however it is spelled.
export const behaviourListeningTo = (model, expression) => {
    const wanted = canonicalKeycode(model, expression);
    return (model?.keyBehaviors || []).find((row) => canonicalKeycode(model, row.keycode) === wanted);
};

// Every activation this layer can be the top of: layer 0 is always on, this
// layer is held, and any set of the layers between may be held alongside.
// Eight layers make at most 64 of them, so the ones that need exact answers
// simply enumerate.
//
// Layer 0 stands in for the default layer. The keyboard can move that with
// NOAH_SETTING_DEFAULT_LAYERS and does not report it, but layer 0 is also what
// the firmware falls back to when nothing active answers, so the set of layers
// that can answer is right either way — only whether an answer needs a layer
// held would be misread, and only on a keyboard whose default has moved.
export function activations(at) {
    const sets = [[]];
    for (let layer = 1; layer < at; layer += 1) {
        for (const held of [...sets]) sets.push([...held, layer]);
    }
    return sets;
}

/**
 * What each key of this layer answers with under one activation.
 *
 * The firmware resolves a press by scanning the layers whose bit is set,
 * highest first, and taking the first that is not transparent. An inactive
 * layer is skipped entirely — it neither answers nor blocks — so `held` names
 * the layers on besides layer 0 and this one.
 */
export function resolvedPositions(stack, at, held = []) {
    const active = new Set([0, at, ...held]);
    const resolved = [];
    for (const position of stack[at]?.positions || []) {
        if (keyFace(position).kind !== "transparent") {
            resolved.push({position, layer: stack[at], fellThrough: false, whileHeld: false});
            continue;
        }
        for (let below = at - 1; below >= 0; below -= 1) {
            if (!active.has(below)) continue;
            const there = (stack[below]?.positions || [])
                .find((other) => other.layoutIndex === position.layoutIndex);
            if (!there || keyFace(there).kind === "transparent") continue;
            resolved.push({position: there, layer: stack[below], fellThrough: true, whileHeld: below !== 0});
            break;
        }
    }
    return resolved;
}

/**
 * Every answer this layer's keys can give, across every activation.
 *
 * For one key this is exactly the union of the layers below that are not
 * transparent at that position: holding {default, M, this} makes M answer, and
 * every answer is some such M. So the set is built directly rather than by
 * enumerating activations — they would produce the same thing. A key answered
 * by anything but the default layer only answers that way while that layer is
 * held too, which `whileHeld` records.
 *
 * Anything reached through a single key — a behaviour, and so the macros and
 * pointing modes its branches send — is exact here. Combos are not: their
 * inputs have to answer at the same time, under one activation, so they
 * enumerate instead.
 */
export function reachablePositions(stack, at) {
    const reachable = [];
    for (const position of stack[at]?.positions || []) {
        if (keyFace(position).kind !== "transparent") {
            reachable.push({position, layer: stack[at], fellThrough: false, whileHeld: false});
            continue;
        }
        for (let below = at - 1; below >= 0; below -= 1) {
            const there = (stack[below]?.positions || [])
                .find((other) => other.layoutIndex === position.layoutIndex);
            if (!there || keyFace(there).kind === "transparent") continue;
            reachable.push({position: there, layer: stack[below], fellThrough: true, whileHeld: below !== 0});
        }
    }
    return reachable;
}

// Reach lists read down the stack: what the default layer answers first, then
// what each layer you would have to hold adds. An entry answered by several
// layers sorts by the nearest one to the default, which is also the layer its
// row names first. Sorting is stable, so entries from one layer keep the order
// the board gave them.
const sourceRank = (entry) => Math.min(
    entry.layer ? entry.layer.index ?? 0 : Infinity,
    ...(entry.keys || []).filter((key) => key.fellThrough).map((key) => key.layer?.index ?? 0),
    ...(entry.behaviours || []).filter((row) => row.layer).map((row) => row.layer.index ?? 0),
);
const downTheStack = (entries) => [...entries].sort((one, other) => sourceRank(one) - sourceRank(other));

/**
 * Where to press for this, on this layer.
 *
 * The keys that name it, and — for anything a behaviour branch sends — the keys
 * carrying that behaviour, because those are the ones you actually reach it
 * through. A key answered from a layer below keeps its physical position, which
 * is what the board draws.
 */
export function reachKeys(stack, at, entry) {
    const indexes = new Set((entry?.keys || []).map((key) => key.position.layoutIndex));
    const behaviours = new Set((entry?.behaviours || []).map((row) => row.keycode));
    if (behaviours.size) {
        for (const {position} of reachablePositions(stack, at)) {
            if (behaviours.has(keyMeaning(position))) indexes.add(position.layoutIndex);
        }
    }
    return [...indexes];
}

/**
 * Behaviours, grouped by how this layer reaches them.
 *
 * A key stored on this layer reaches its behaviour directly. A transparent key
 * lets the layer underneath answer, so a behaviour stored below still fires
 * while this layer is active — that one is reached *through* the layer, and it
 * is worth naming which layer answers. Everything else the profile carries is
 * somewhere this layer never reaches.
 */
export function behaviourGroups(model, stack, at) {
    const here = [], through = [];
    for (const {position, layer, fellThrough, whileHeld} of reachablePositions(stack, at)) {
        const row = behaviourFor(model, keyMeaning(position));
        if (!row) continue;
        if (!fellThrough) { if (!here.includes(row)) here.push(row); }
        else if (!through.some((entry) => entry.row === row)) through.push({row, layer, whileHeld});
    }

    // A behaviour reached both ways is listed under both, the same as anything
    // else: the groups say how this layer gets at it, not which way won.
    const reached = new Set([...here, ...through.map((entry) => entry.row)]);
    return {here, through: downTheStack(through),
        elsewhere: (model?.keyBehaviors || []).filter((row) => !reached.has(row))};
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

// The positions among these that carry a combo's inputs, one per input. A
// combo fires from the keycodes the active layer produces, so all of its
// inputs have to be there.
function comboKeysAmong(positions, combo) {
    const inputs = combo?.inputs || [];
    const found = new Map();
    for (const position of positions) {
        if (!comboTouches(combo, position)) continue;
        const input = inputs.find((name) => name === position.keycode || name === keyMeaning(position)) ?? position.keycode;
        if (!found.has(input)) found.set(input, position);
    }
    return [...found.values()];
}

/**
 * Combos, grouped the way everything else is — but a combo is the one thing a
 * union cannot answer.
 *
 * A combo needs every input present at the same time, under one activation. Its
 * inputs could each be reachable under some hold and still never be reachable
 * together: one answered only while Numbers is held, another only while it is
 * not. So every activation this layer can top is tried, and the combo fires if
 * any single one carries all of its inputs — preferring the one that holds the
 * fewest extra layers, because that is the easiest way to press it.
 *
 * It has no branch case either: a behaviour fires a keycode, never a chord.
 */
export function comboGroups(model, stack, at) {
    // The activations do not depend on the combo, so each is resolved once and
    // every combo is tried against it. Resolving per combo instead re-walks the
    // whole board 64 times over, on every render.
    const tries = activations(at).map((held) => {
        const resolved = resolvedPositions(stack, at, held);
        return {held, resolved, answering: new Map(resolved.map((entry) => [entry.position, entry]))};
    });

    const onKeys = [], throughKeys = [], elsewhere = [];
    for (const combo of model?.combos || []) {
        const inputs = (combo.inputs || []).length;
        let fires = null, most = null;
        for (const {held, resolved, answering} of tries) {
            const keys = comboKeysAmong(resolved.map((entry) => entry.position), combo)
                .map((position) => answering.get(position));
            if (!most || keys.length > most.keys.length) most = {keys, held};
            if (inputs && keys.length >= inputs && (!fires || held.length < fires.held.length)) fires = {keys, held};
        }
        if (!fires) { elsewhere.push({combo, inputs, keys: most?.keys || [], held: []}); continue; }
        const entry = {combo, inputs, keys: fires.keys, held: fires.held.map((index) => stack[index])};
        (entry.keys.every((key) => !key.fellThrough) ? onKeys : throughKeys).push(entry);
    }
    return {onKeys, throughKeys: downTheStack(throughKeys), fromBranches: [], fromBranchesBelow: [], elsewhere};
}

// What the combo builder starts from when an existing combo is opened: the
// board positions its inputs sit on from this layer, each with the input name
// the keyboard stores, and any input this layer cannot reach, which is kept
// rather than silently dropped on save.
export function comboEditInputs(model, stack, at, combo) {
    const groups = comboGroups({...model, combos: [combo]}, stack, at);
    const entry = groups.onKeys[0] || groups.throughKeys[0] || groups.elsewhere[0];
    const names = combo?.inputs || [];
    const positions = [], codes = {}, used = new Set();
    for (const {position} of entry?.keys || []) {
        const input = names.find((name) => !used.has(name) && (name === position.keycode || name === keyMeaning(position)));
        if (input === undefined || positions.includes(position.layoutIndex)) continue;
        used.add(input);
        positions.push(position.layoutIndex);
        codes[position.layoutIndex] = input;
    }
    return {positions, codes, extras: names.filter((name) => !used.has(name))};
}

export const macroKeycodes = (keycode) =>
    [...String(keycode || "").matchAll(/\b((?:VIA_)?MACRO_\d+)\b/g)].map((match) => match[1]);

/**
 * How this layer reaches a set of things named by keycode — the one shape the
 * Macros and Pointing modes tabs both group by, so they cannot drift apart.
 *
 *   on this layer        a key here names it
 *   through a behaviour  a branch of a behaviour mapped here sends it, which
 *                        no key cap can show
 *   through this layer   a transparent key lets a lower layer's key name it
 *   through a behaviour  a transparent key lets a lower layer's behaviour
 *     below              answer, and one of its branches sends it
 *   elsewhere            the profile carries it, this layer reaches it no way
 *
 * The first two are what this layer itself holds, which is what a tab counts;
 * the next two it only reaches down the stack. `namesOf(keycode)` answers which
 * of the things a keycode names, as stable string keys, and `all` is every name
 * worth reporting as unreached.
 */
export function reachGroups(model, stack, at, namesOf, all) {
    const found = new Map();
    const reachOf = (name) => {
        if (!found.has(name)) found.set(name, {directKeys: [], fellKeys: [], directRows: [], belowRows: []});
        return found.get(name);
    };

    for (const {position, layer, fellThrough, whileHeld} of reachablePositions(stack, at)) {
        for (const name of namesOf(keyMeaning(position))) {
            reachOf(name)[fellThrough ? "fellKeys" : "directKeys"].push({position, layer, fellThrough, whileHeld});
        }
    }

    // Every behaviour this layer can fire, including the ones it only reaches
    // because a transparent key lets the default layer answer. Those carry the
    // layer that holds them, so a row can say so.
    const {here, through} = behaviourGroups(model, stack, at);
    for (const {row, from, whileHeld} of [...here.map((row) => ({row, from: null, whileHeld: false})),
        ...through.map((item) => ({row: item.row, from: item.layer, whileHeld: item.whileHeld}))]) {
        for (const step of row.steps || []) {
            for (const tier of [step.tap, step.hold, step.longHold]) {
                for (const name of namesOf(tier?.action)) {
                    const rows = reachOf(name)[from ? "belowRows" : "directRows"];
                    // The action matters as well as the behaviour: a pointing
                    // mode answers to two keycodes, and which one a branch
                    // sends is the difference between holding and toggling it.
                    if (!rows.some((entry) => entry.keycode === row.keycode && entry.action === tier.action)) {
                        rows.push({keycode: row.keycode, action: tier.action, layer: from, whileHeld});
                    }
                }
            }
        }
    }

    // One thing is often reached several ways, and the groups answer which way
    // rather than which way first — so it is listed under each route it has,
    // each entry carrying only that route. A macro sitting on a key here that a
    // behaviour here also fires is two answers, not one with a footnote.
    const onKeys = [], fromBranches = [], throughKeys = [], fromBranchesBelow = [];
    for (const [name, reach] of found) {
        if (reach.directKeys.length) onKeys.push({name, keys: reach.directKeys, behaviours: []});
        if (reach.directRows.length) fromBranches.push({name, keys: [], behaviours: reach.directRows});
        if (reach.fellKeys.length) throughKeys.push({name, keys: reach.fellKeys, behaviours: []});
        if (reach.belowRows.length) fromBranchesBelow.push({name, keys: [], behaviours: reach.belowRows});
    }
    return {onKeys, fromBranches,
        throughKeys: downTheStack(throughKeys), fromBranchesBelow: downTheStack(fromBranchesBelow),
        elsewhere: all.filter((name) => !found.has(name))};
}

/**
 * Macro slots, grouped by how this layer sets them off.
 *
 * A key on this layer can carry a macro keycode, which the board shows. A
 * transparent key lets a lower layer's macro key answer instead, which the
 * board shows only as falling through. A behaviour this layer reaches can fire
 * one from any of its branches, and that the board cannot show at all, because
 * the key cap carries the behaviour, not what its branches send. The rest are
 * slots holding a payload nothing here reaches.
 */
export const macroReach = (model, stack, at) => reachGroups(model, stack, at, macroKeycodes,
    [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])]
        // An empty slot is not a macro this layer is missing, it is a slot.
        .filter((slot) => !slot.empty).map((slot) => slot.keycode));

/**
 * Pointing modes, grouped the same four ways. A mode is reached by the key that
 * holds or toggles it, by a transparent key letting a lower one do so, or by a
 * behaviour branch that sends its keycode.
 */
export const pointingReach = (model, stack, at) => reachGroups(model, stack, at,
    (keycode) => { const slot = pointingSlotFor(model, keycode); return slot ? [String(slot.id)] : []; },
    // A slot with no movement cannot be activated, so it is not a mode this
    // layer is missing either.
    (model?.pdModes || []).filter((slot) => slot.kind).map((slot) => String(slot.id)));


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

// Which of a slot's two keycodes this is. Everything else about the mode is the
// same either way, so holding it or toggling it on is the only thing a route
// has to carry beyond the slot itself.
export function pointingVariant(model, keycode) {
    const alias = model?.qmkKeycodeAliases?.[keycode] ?? keycode;
    if (/_LOCK$/.test(String(alias))) return "toggle";
    const slot = pointingSlotFor(model, keycode);
    const value = typeof keycode === "number" ? keycode
        : /^0x[0-9a-f]+$/i.test(String(keycode)) ? Number(keycode) : NaN;
    if (slot && Number.isInteger(value) && slotKeycodes(slot)[1] === value) return "toggle";
    return "hold";
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
