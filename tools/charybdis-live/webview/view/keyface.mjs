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
    const layerKey = /^(MO|TO|TG|OSL|DF|PDF|TT)\(\s*([A-Z0-9_]+)\s*\)$/.exec(keycode);
    const modTap = /^MT\(([^,]+),/.exec(keycode);
    // A pointing key for a slot that holds nothing keeps its own name on the
    // cap; that it is inert belongs on the second line, where a key cap says
    // what is true of the key rather than of its label.
    const emptySlot = /^(.+) \(empty\)$/.exec(position.display || "");
    if (emptySlot) return {main: emptySlot[1], sub: "empty", kind: "key"};
    // A layer keycode's cap names its layer, so the second line says what it
    // does to that layer; a layer-tap's second line names its layer; a mod-tap reads as its tap key, so the second
    // line names the modifier its hold sends — otherwise MT(Shift, A) and a
    // plain A, or MO(1) and TG(1), would be indistinguishable on the board.
    return {
        main: position.display || keycode,
        sub: layerTap ? position.layerLabel || shortLayer(layerTap[1]) : layerKey ? LAYER_VERBS[layerKey[1]] : modTap ? modTapHold(modTap[1]) : "",
        kind: layerTap || layerKey ? "layer" : "key",
    };
}

// The layer a keycode acts on, as its place in the stack: MO(3), LOCK_LAYER(3)
// and LT(3,KC_F) all name layer 3. A stored name the host knows by a semantic
// one (QK_USER_31 → LOCK_LAYER(3)) is read by that. Null for any other key.
const LAYER_KEYCODE = /^(?:MO|TG|TT|OSL|TO|DF|PDF|LOCK_LAYER)\(\s*(\d+)\s*\)$|^LT\(\s*(\d+)\s*,/;
export function layerOfKeycode(model, keycode) {
    const name = model?.qmkKeycodeAliases?.[keycode] ?? keycode;
    const match = LAYER_KEYCODE.exec(String(name ?? ""));
    return match ? Number(match[1] ?? match[2]) : null;
}

// A key by its whole name, for anything that is not a key cap. A cap shows what
// fits — "F" over "Navigation" — and the name is the host's for the keycode,
// "F / Navigation" (core/model/key-names.js).
export function keyName(position) {
    const face = keyFace(position);
    if (face.kind === "transparent") return "Transparent";
    if (face.kind === "disabled" || face.kind === "none") return "Unmapped";
    return position.editLabel || face.main || position.keycode || "";
}

const LAYER_VERBS = {MO: "momentary", TO: "move", TG: "toggle", OSL: "one-shot", DF: "default", PDF: "default", TT: "tap-toggle"};
const MOD_WORDS = {CTL: "Ctrl", SFT: "Shift", ALT: "Alt", GUI: "Cmd"};
// MOD_LCTL|MOD_LSFT → "Ctrl+Shift"; right-hand modifiers are marked R.
const modTapHold = (mods) => mods.split("|").map((mod) => {
    const match = /^MOD_([LR])(CTL|SFT|ALT|GUI)$/.exec(mod.trim());
    return match ? `${match[1] === "R" ? "R" : ""}${MOD_WORDS[match[2]]}` : mod.trim();
}).join("+");

// Only for a position the host has not named: the layer the keycode holds.
const shortLayer = (name) => name.replace(/^LAYER_/, "").toLowerCase();

// A position carries two names: the one the keyboard stores (`QK_USER_16`, or
// bare hex for a value the shipped vocabulary never named) and the one the rest
// of the profile uses (`DRAGSCROLL`, `VIA_MACRO_0`, `LEFT_THUMB`). Behaviour
// rows, macro slots and pointing slots are all keyed by the second, so every
// lookup from a key to what it reaches goes through this.
export const keyMeaning = (position) => position?.semantic || position?.keycode || "";

// Macro slots have one profile name across all 64 slots, even though QMK's
// catalogue names only the first 32 native values. Keep position.keycode for
// round-trip edits; use this when a screen prints that keycode to a person.
export const visibleKeycode = (model, name) => {
    const semantic = model?.qmkKeycodeAliases?.[name] || name || "";
    return /^VIA_MACRO_(?:[0-9]|[1-5][0-9]|6[0-3])$/.test(semantic) ? semantic : name || "";
};

// How a keycode name is spoken in prose: the label the keyboard's own
// vocabulary gives it, so a behaviour row, a reach line and the key cap all
// call one key the same thing. Names the vocabulary does not cover — a device
// action outside the advertised ABI — keep their numeric identity.
export const actionLabel = (model, name) => model?.qmkKeyLabels?.[name]
    || model?.qmkKeyLabels?.[model?.qmkKeycodeAliases?.[name]]
    || String(name ?? "");

// What a behaviour cell reads as in the grid: the name a person knows the key
// by, so it says "#", "Shift+Enter" and "Hold Numbers" where the keyboard
// stores KC_HASH, LSFT(KC_ENTER) and MO(1). Every name comes from the host
// (core/model/key-names.js); with none, the keycode is the label. The cell
// editor keeps the keycode.
export function cellLabel(model, branch) {
    const action = String(branch?.action ?? "");
    const named = actionLabel(model, action);
    return named !== action ? named : branch?.label || action;
}

export const behaviourFor = (model, keycode) =>
    (model?.keyBehaviors || []).find((row) => row.keycode === keycode);

// One spelling per key: the picker says KC_ENT and G(KC_C) where a behaviour
// row says KC_ENTER and LGUI(KC_C), and an expression may or may not space its
// arguments or nest two modifiers either way round. Every name goes through the
// keyboard's alias table, and every modifier wrapper becomes one ordered set.
const WRAPPER_MODS = {
    C: ["LCTL"], LCTL: ["LCTL"], S: ["LSFT"], LSFT: ["LSFT"], A: ["LALT"], LALT: ["LALT"], LOPT: ["LALT"],
    G: ["LGUI"], LGUI: ["LGUI"], LCMD: ["LGUI"], LWIN: ["LGUI"],
    RCTL: ["RCTL"], RSFT: ["RSFT"], RALT: ["RALT"], ROPT: ["RALT"], ALGR: ["RALT"], RGUI: ["RGUI"], RCMD: ["RGUI"], RWIN: ["RGUI"],
    LCS: ["LCTL", "LSFT"], LCA: ["LCTL", "LALT"], LCG: ["LCTL", "LGUI"], LSA: ["LSFT", "LALT"], LSG: ["LSFT", "LGUI"],
    LAG: ["LALT", "LGUI"], LCAG: ["LCTL", "LALT", "LGUI"], MEH: ["LCTL", "LSFT", "LALT"], HYPR: ["LCTL", "LSFT", "LALT", "LGUI"],
};
const MOD_ORDER = ["LCTL", "LSFT", "LALT", "LGUI", "RCTL", "RSFT", "RALT", "RGUI"];
export function canonicalKeycode(model, expression) {
    let text = String(expression ?? "").replace(/\s+/g, "");
    const mods = new Set();
    for (let match; (match = /^([A-Z]+)\((.*)\)$/.exec(text)) && WRAPPER_MODS[match[1]];) {
        WRAPPER_MODS[match[1]].forEach((mod) => mods.add(mod));
        text = match[2];
    }
    const inner = text.replace(/[A-Za-z_][A-Za-z0-9_]*/g, (name) => model?.qmkKeycodeAliases?.[name] ?? name);
    return MOD_ORDER.filter((mod) => mods.has(mod)).reduceRight((wrapped, mod) => `${mod}(${wrapped})`, inner);
}

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
    ...(entry.combos || []).flatMap((route) => route.keys).filter((key) => key.fellThrough).map((key) => key.layer?.index ?? 0),
);
const downTheStack = (entries) => [...entries].sort((one, other) => sourceRank(one) - sourceRank(other));

/**
 * Where to press for this, on this layer.
 *
 * The keys that name it, and — for anything a behaviour branch sends — the keys
 * carrying that behaviour, because those are the ones you actually reach it
 * through. For anything a combo sends, the keys you chord to fire it. A key answered from a layer below keeps its physical position, which
 * is what the board draws.
 */
export function reachKeys(stack, at, entry) {
    const indexes = new Set([...(entry?.keys || []), ...(entry?.combos || []).flatMap((route) => route.keys)]
        .map((key) => key.position.layoutIndex));
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
 * is worth naming which layer answers. A combo that sends a behaviour's
 * keycode fires it too, with no key carrying it at all. Everything else the
 * profile carries is somewhere this layer never reaches.
 */
export function behaviourGroups(model, stack, at) {
    const here = [], through = [];
    for (const {position, layer, fellThrough, whileHeld} of reachablePositions(stack, at)) {
        const row = behaviourFor(model, keyMeaning(position));
        if (!row) continue;
        if (!fellThrough) { if (!here.includes(row)) here.push(row); }
        else if (!through.some((entry) => entry.row === row)) through.push({row, layer, whileHeld});
    }

    // A combo's output is processed like a key press, so a combo sending a
    // behaviour's keycode fires it though no key carries it. A combo chorded on
    // this layer's own keys is this layer's; one needing a transparent key is
    // reached down the stack.
    const combos = [], combosBelow = [];
    const groupsOfCombos = comboGroups(model, stack, at);
    for (const [entries, list] of [[groupsOfCombos.onKeys, combos], [groupsOfCombos.throughKeys, combosBelow]]) {
        for (const {combo, keys} of entries) {
            const row = behaviourFor(model, comboOutput(model, combo));
            if (!row) continue;
            const entry = list.find((candidate) => candidate.row === row);
            if (entry) entry.combos.push({combo, keys});
            else list.push({row, combos: [{combo, keys}]});
        }
    }

    // A behaviour reached several ways is listed under each, the same as
    // anything else: the groups say how this layer gets at it, not which way won.
    const reached = new Set([...here, ...[...through, ...combos, ...combosBelow].map((entry) => entry.row)]);
    return {here, through: downTheStack(through), combos, combosBelow: downTheStack(combosBelow),
        elsewhere: (model?.keyBehaviors || []).filter((row) => !reached.has(row))};
}

/**
 * The keys the board rings for a behaviour picked under one of its routes.
 *
 * A behaviour reached several ways is listed under each group, and a pick
 * names the group it was made in — as the macro and pointing lists do — so
 * picking it under "through a combo" rings the chord, not also the transparent
 * key that happens to fall through to it. With no route, every way this layer
 * reaches it.
 */
export function behaviourRouteKeys(model, stack, at, keycode, route = null) {
    if (!keycode) return [];
    const carrying = reachablePositions(stack, at).filter(({position}) => keyMeaning(position) === keycode);
    const groups = behaviourGroups(model, stack, at);
    const chorded = (entries) => entries.filter((entry) => entry.row.keycode === keycode)
        .flatMap((entry) => entry.combos).flatMap((combo) => combo.keys);
    const keys = {
        here: carrying.filter((entry) => !entry.fellThrough),
        through: carrying.filter((entry) => entry.fellThrough),
        combos: chorded(groups.combos),
        belowCombos: chorded(groups.combosBelow),
    };
    const chosen = route ? keys[route] || [] : Object.values(keys).flat();
    return [...new Set(chosen.map((entry) => entry.position.layoutIndex))];
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
    : (combo?.inputs || []).some((input) => inputOn(input, position));
// Whether a key presses the input a combo stores by this name: by the keycode
// it stores, or by what that keycode means.
const inputOn = (name, position) => name === position.keycode || name === keyMeaning(position);

// The matching rule alone, for one key, with no layer in view. Screens ask
// combosAt(), which applies it the way the keyboard does on the layer shown.
export function combosForKey(model, position) {
    if (!position) return [];
    return (model?.combos || []).filter((combo) => comboTouches(combo, position));
}

// Every position among these that carries one of a combo's inputs, and how
// many distinct inputs they cover. A combo fires from the keycodes the active
// layer produces, so all of its inputs have to be covered; an input placed on
// two keys can be pressed on either, so both are its keys.
function comboKeysAmong(positions, combo) {
    const inputs = combo?.inputs || [];
    const keys = [], covered = new Set();
    for (const position of positions) {
        if (!comboTouches(combo, position)) continue;
        keys.push(position);
        covered.add(inputs.find((name) => inputOn(name, position)) ?? position.keycode);
    }
    return {keys, covered: covered.size};
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
// Which layer's key assignments combos are matched against while `at` is the
// top layer — the keyboard's Combo Layer Matching setting. The draft's settings
// section is read first, so an edit there shows here before it is applied;
// the combo readback is the keyboard's own answer when settings are absent.
export function comboReferenceLayer(model, at) {
    const section = (model?.configDefaults || []).find((row) => row.id === "comboReferences");
    const field = section?.fields?.find((row) => row.macro === `comboReference${at}`);
    const written = /^Layer (\d+)$/.exec(String(field?.value ?? ""));
    if (written) return Number(written[1]);
    const read = model?.comboReadback?.layerReferences?.[at];
    return Number.isInteger(read) ? read : at;
}

// Every reach list asks for the combos — the Combos tab, the board's badges,
// and the macros and pointing modes a combo sends — and the Macros screen asks
// for every layer at once. Enumerating activations is the costly part, so each
// (model, stack, layer) is grouped once. The model is replaced, never mutated.
const comboGroupsByModel = new WeakMap();
export function comboGroups(model, stack, at) {
    if (!model) return groupCombos(model, stack, at);
    let perLayer = comboGroupsByModel.get(model);
    if (!perLayer) comboGroupsByModel.set(model, perLayer = new Map());
    const cached = perLayer.get(at);
    if (cached?.stack === stack) return cached.groups;
    const groups = groupCombos(model, stack, at);
    perLayer.set(at, {stack, groups});
    return groups;
}

function groupCombos(model, stack, at) {
    // QMK matches combos against the reference layer's raw keycodes whenever
    // that layer is not the top one (process_combo.c: keymap_key_to_keycode on
    // ref_layer, no transparency). Then there is exactly one way to press the
    // combo, whatever else is held.
    const reference = comboReferenceLayer(model, at);
    const referenced = reference !== at && stack[reference] ? stack[reference] : null;
    // The activations do not depend on the combo, so each is resolved once and
    // every combo is tried against it. Resolving per combo instead re-walks the
    // whole board 64 times over, on every render.
    const tries = referenced
        ? [(() => {
            const resolved = (referenced.positions || []).map((position) =>
                ({position, layer: referenced, fellThrough: false, whileHeld: false, reference: true}));
            return {held: [], resolved, answering: new Map(resolved.map((entry) => [entry.position, entry]))};
        })()]
        : activations(at).map((held) => {
            const resolved = resolvedPositions(stack, at, held);
            return {held, resolved, answering: new Map(resolved.map((entry) => [entry.position, entry]))};
        });

    const onKeys = [], throughKeys = [], elsewhere = [];
    for (const combo of model?.combos || []) {
        const inputs = (combo.inputs || []).length;
        let fires = null, most = null;
        for (const {held, resolved, answering} of tries) {
            const found = comboKeysAmong(resolved.map((entry) => entry.position), combo);
            const keys = found.keys.map((position) => answering.get(position));
            if (!most || found.covered > most.covered) most = {keys, held, covered: found.covered};
            if (inputs && found.covered >= inputs && (!fires || held.length < fires.held.length)) fires = {keys, held, covered: found.covered};
        }
        if (!fires) { elsewhere.push({combo, inputs, covered: most?.covered || 0, keys: most?.keys || [], held: []}); continue; }
        const entry = {combo, inputs, covered: fires.covered, keys: fires.keys, held: fires.held.map((index) => stack[index])};
        (entry.keys.every((key) => !key.fellThrough) ? onKeys : throughKeys).push(entry);
    }
    return {onKeys, throughKeys: downTheStack(throughKeys), fromBranches: [], fromBranchesBelow: [], elsewhere};
}

// The combos a key carries on this layer, keyed by layout index. The board
// marks what the layer stores, the same rule as behaviour dots: a transparent
// key shows nothing of the layer below it, and what it inherits is listed in
// the Combos tab under "through a transparent key". Of the rest, only combos
// the table says fire from this layer count — so a key sharing one input of a
// combo that can never fire here is not marked — and under Combo Layer
// Matching the keys whose position the reference layer supplies are. The
// board asks once per key, so each (model, layer) is grouped once.
const combosByModel = new WeakMap();
export function combosOnLayer(model, stack, at) {
    if (!model) return new Map();
    let perLayer = combosByModel.get(model);
    if (!perLayer) combosByModel.set(model, perLayer = new Map());
    const cached = perLayer.get(at);
    if (cached?.stack === stack) return cached.keys;
    const {onKeys, throughKeys} = comboGroups(model, stack, at);
    const order = new Map((model.combos || []).map((combo, index) => [combo, index]));
    const keys = new Map();
    for (const entry of [...onKeys, ...throughKeys]) {
        for (const key of entry.keys) {
            if (key.fellThrough) continue;
            const combos = keys.get(key.position.layoutIndex) || [];
            if (!combos.includes(entry.combo)) combos.push(entry.combo);
            keys.set(key.position.layoutIndex, combos);
        }
    }
    for (const combos of keys.values()) combos.sort((a, b) => order.get(a) - order.get(b));
    perLayer.set(at, {stack, keys});
    return keys;
}
export const combosAt = (model, stack, at, layoutIndex) => combosOnLayer(model, stack, at).get(layoutIndex) || [];

// The combo builder holds its inputs by the names the combo stores, in stored
// order, never by board position: a combo fires on keycodes, and one position
// carries a different key on every layer. The board is only where they are
// picked and shown.
//
// What the combo engine sees at each position while `at` is on top and `held`
// are previewed on under it. Under Combo Layer Matching that is the reference
// layer's raw keycode, as groupCombos matches it; otherwise the key that
// answers — the highest active layer that is not transparent, which is the
// default layer when nothing else is previewed on. Keyed by layout index; a
// position nothing answers is absent.
export function comboAnswers(model, stack, at, held = []) {
    const reference = comboReferenceLayer(model, at);
    const positions = reference !== at && stack[reference]
        ? stack[reference].positions || []
        : resolvedPositions(stack, at, held).map((entry) => entry.position);
    return new Map(positions.map((position) => [position.layoutIndex, position]));
}

// The board keys that press one of the builder's inputs from this view. An
// input placed on two keys rings both; one no key here presses rings nothing,
// and stays an input.
export function comboInputKeys(model, stack, at, held, inputs) {
    return [...comboAnswers(model, stack, at, held)]
        .filter(([, position]) => inputs.some((name) => inputOn(name, position)))
        .map(([layoutIndex]) => layoutIndex);
}

// Whether a builder input is pressed by any key in this view.
export const comboInputShown = (model, stack, at, held, name) =>
    [...comboAnswers(model, stack, at, held).values()].some((position) => inputOn(name, position));

// The combos that take the key at this position as an input, in the view the
// board shows: `at` on top with `held` previewed on under it, so a transparent
// key asks about what shows through it, as picking it would (comboAnswers).
// `position` is that key, or null where nothing answers.
export function combosOnKey(model, stack, at, held, layoutIndex) {
    const position = comboAnswers(model, stack, at, held).get(layoutIndex) || null;
    return {position, combos: combosForKey(model, position)};
}

// A board click while picking, given the key that answers there: the input it
// presses leaves the combo if the combo has it, and otherwise joins at the end
// under the keycode the keyboard stores for that key. One removed and picked
// again therefore counts as added. A position nothing answers, or a disabled
// key, is not an input.
export function toggleComboInput(inputs, position) {
    if (!position || keyFace(position).kind === "disabled") return inputs;
    return inputs.some((name) => inputOn(name, position))
        ? inputs.filter((name) => !inputOn(name, position))
        : [...inputs, position.keycode];
}

export const macroKeycodes = (keycode) =>
    [...String(keycode || "").matchAll(/\b((?:VIA_)?MACRO_\d+)\b/g)].map((match) => match[1]);

/**
 * The layers a macro is set off from, in stack order: every layer that holds a
 * way to it itself — a key naming it, a behaviour mapped there whose branch
 * sends it, or a combo firing from its keys that sends it, directly or through
 * a behaviour. Each route carries the group the Keys tab lists it under, so a
 * screen can open that layer on the same row, and the keys the board rings.
 * What a layer only reaches under a transparent key is the lower layer's route,
 * listed for that layer, not repeated for every layer above it. `at` is the
 * layer's position in the stack, which is how a screen selects it.
 */
export function macroPlacements(model, stack, keycode) {
    return (stack || []).map((layer, at) => {
        const reach = macroReach(model, stack, at);
        const routes = [["here", reach.onKeys], ["branches", reach.fromBranches], ["combos", reach.fromCombos]]
            .map(([group, entries]) => ({group, entry: entries.find((entry) => entry.name === keycode)}))
            .filter((route) => route.entry)
            .map((route) => ({...route, keys: reachKeys(stack, at, route.entry)}));
        return {layer, at, routes};
    }).filter((placement) => placement.routes.length);
}

// What a combo sends, by what it means: the combo reports the catalogue's name
// for its output (`QK_MACRO_0`, `QK_USER_16`), and every domain it could reach
// is keyed by the semantic one.
export const comboOutput = (model, combo) =>
    model?.qmkKeycodeAliases?.[combo?.output] ?? combo?.output ?? "";

/**
 * How this layer reaches a set of things named by keycode — the one shape the
 * Macros and Pointing modes tabs both group by, so they cannot drift apart.
 *
 *   on this layer        a key here names it
 *   through a behaviour  a branch of a behaviour mapped here sends it, which
 *                        no key cap can show
 *   through this layer   a transparent key lets a lower layer's key name it
 *   through a combo      a combo firing from this layer's keys sends it —
 *                        or sends a behaviour whose branch does, since a
 *                        combo's output runs through the same stages a key
 *                        press does
 *   through a behaviour  a transparent key lets a lower layer's behaviour
 *     below              answer, and one of its branches sends it
 *   through a combo      a combo that needs a transparent key to fire sends
 *     below              it, directly or through a behaviour
 *   elsewhere            the profile carries it, this layer reaches it no way
 *
 * The first three are what this layer itself holds, which is what a tab counts;
 * the rest it only reaches down the stack. `namesOf(keycode)` answers which
 * of the things a keycode names, as stable string keys, and `all` is every name
 * worth reporting as unreached.
 */
export function reachGroups(model, stack, at, namesOf, all) {
    const found = new Map();
    const reachOf = (name) => {
        if (!found.has(name)) found.set(name, {directKeys: [], fellKeys: [], directRows: [], belowRows: [], directCombos: [], belowCombos: []});
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

    // A combo's output is processed like a key press, so it reaches what it
    // sends and, when that is a behaviour's keycode, what the behaviour's
    // branches send. A combo firing from this layer's own keys is this layer's;
    // one that needs a transparent key is reached down the stack.
    const combos = comboGroups(model, stack, at);
    for (const [entries, below] of [[combos.onKeys, false], [combos.throughKeys, true]]) {
        for (const {combo, keys} of entries) {
            const output = comboOutput(model, combo);
            const sends = [{action: output, via: null}];
            for (const step of behaviourFor(model, output)?.steps || []) {
                for (const tier of [step.tap, step.hold, step.longHold]) {
                    if (tier?.action) sends.push({action: tier.action, via: output});
                }
            }
            for (const {action, via} of sends) {
                for (const name of namesOf(action)) {
                    const routes = reachOf(name)[below ? "belowCombos" : "directCombos"];
                    if (!routes.some((route) => route.combo === combo && route.action === action)) routes.push({combo, via, action, keys});
                }
            }
        }
    }

    // One thing is often reached several ways, and the groups answer which way
    // rather than which way first — so it is listed under each route it has,
    // each entry carrying only that route. A macro sitting on a key here that a
    // behaviour here also fires is two answers, not one with a footnote.
    const onKeys = [], fromBranches = [], fromCombos = [], throughKeys = [], fromBranchesBelow = [], fromCombosBelow = [];
    for (const [name, reach] of found) {
        if (reach.directKeys.length) onKeys.push({name, keys: reach.directKeys, behaviours: [], combos: []});
        if (reach.directRows.length) fromBranches.push({name, keys: [], behaviours: reach.directRows, combos: []});
        if (reach.directCombos.length) fromCombos.push({name, keys: [], behaviours: [], combos: reach.directCombos});
        if (reach.fellKeys.length) throughKeys.push({name, keys: reach.fellKeys, behaviours: [], combos: []});
        if (reach.belowRows.length) fromBranchesBelow.push({name, keys: [], behaviours: reach.belowRows, combos: []});
        if (reach.belowCombos.length) fromCombosBelow.push({name, keys: [], behaviours: [], combos: reach.belowCombos});
    }
    return {onKeys, fromBranches, fromCombos,
        throughKeys: downTheStack(throughKeys), fromBranchesBelow: downTheStack(fromBranchesBelow),
        fromCombosBelow: downTheStack(fromCombosBelow),
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
    (model?.viaMacros || [])
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
    const named = slots.find((slot) => slot.binding && [slot.binding.hold, slot.binding.lock].includes(String(alias || "")));
    if (named) return named;
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

// A branch or combo action read as the pointing mode it sends: the slot, the
// name it goes by, and whether the key holds the mode or toggles it on. Null
// for any other action. An empty slot keeps its keycodes, so it still
// resolves, marked empty.
export function pointingAction(model, action) {
    const slot = pointingSlotFor(model, action);
    if (!slot) return null;
    return {
        slot,
        name: slot.displayName || slot.name || `Slot ${slot.id}`,
        how: pointingVariant(model, action),
        empty: !slot.kind,
    };
}

// A branch or combo action read as the macro it plays: the slot and the name
// it was given, or its number when it has none. Null for anything but a bare
// macro keycode.
export function macroAction(model, action) {
    const [keycode] = macroKeycodes(action);
    if (!keycode || keycode !== String(action).trim()) return null;
    const slot = (model?.viaMacros || []).find((row) => row.keycode === keycode);
    return {slot, keycode, name: slot?.name || `Macro ${keycode.split("_").at(-1)}`, empty: Boolean(slot?.empty)};
}

// An action that names something the person set up — a pointing mode or a
// macro — as that thing: what kind it is, its name, and the tags that qualify
// it. Null for a plain keycode, which reads as itself.
export function namedAction(model, action) {
    const pd = pointingAction(model, action);
    if (pd) return {kind: "pointing", word: "pointing mode", name: pd.name, slot: pd.slot, tags: [pd.how, ...(pd.empty ? ["empty"] : [])]};
    const macro = macroAction(model, action);
    if (macro) return {kind: "macro", word: "macro", name: macro.name, slot: macro.slot, tags: macro.empty ? ["empty"] : []};
    return null;
}

// The two values a slot answers to: hold and toggle, as the host's binding
// registry gives them. Matching on the numbers rather than on names matters
// for an empty slot, whose record carries no name for the model to resolve —
// the keycode exists on the keyboard either way.
export const slotKeycodes = (slot) => slot.binding ? [slot.binding.holdCode, slot.binding.lockCode] : [];

// What still reaches a pointing slot. The keyboard keeps its mode keycodes
// whatever a slot holds, so a key bound to an empty slot is inert rather than
// invalid — and the interface has to say which keys those are.
export function bindingsForSlot(model, slot) {
    if (!slot) return {keys: [], behaviours: [], combos: [], layers: []};
    const values = new Set(slotKeycodes(slot));
    const names = new Set([slot.binding?.hold, slot.binding?.lock].filter(Boolean));
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
    const combos = (model?.combos || []).filter((row) => names.has(row.output));
    return {keys, behaviours, combos, layers: [...layers]};
}

// The keycode name that holds a slot's mode.
export const bindingKeycode = (slot) => slot.binding?.hold || "";
