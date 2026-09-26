"use strict";

// Which layers a profile lets you reach, and whether you can always get back.
//
// The keyboard runs its layers through layer ownership (users/noah/lib/state/
// ownership): a layer is on while a key holds it or while it is locked, and
// base is always on. TG() and LOCK_LAYER() toggle a lock, TO(n) locks n alone
// and TO(0) releases every lock, TT()'s TAPPING_TOGGLE-th tap locks, and MO(),
// TT(), LT()'s hold, LM() and OSL() hold their layer. There is no timeout and no
// "clear all" key, so a lock with nothing to release it stays until the
// keyboard is unplugged.
//
// This walks every state the profile can reach from a keyboard at rest: a set
// of locked layers, and on top of it any layers held while keys are down. In
// each state every key resolves as the firmware does (the highest layer on that
// is not transparent answers), and each key's own layer action, its behaviour
// branches, the combos that match, and the trackball waking the pointer layer
// move it to another state. From that it names:
//
// - traps: a set of locks you can get into and cannot get back to Base from,
//   the only finding that asks for a confirmation before Apply;
// - layers nothing reaches, layer keys that reach a layer with no keys of its
//   own, layer keys that hold or toggle Base (which is always on, so they do
//   nothing), transparent and KC_NO keys on Base, a pointer layer that cannot work, and
//   layer keys the keyboard does not run through its layer tracking.
//
// Holding keys is not limited by fingers or by which key is already down, and
// a one-shot counts as a hold, so the walk finds every way out a person could
// find and at most a few they could not. It never invents a trap.

const keycodes = require("../data/keycode-catalog");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../data/charybdis-layout");
const {PROFILE_ACTION_KINDS: ACTION} = require("../schema/profile-blob-v1");
const {KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");
const {actionName, nativeCode} = require("../schema/actions");
const {layerName} = require("./vocabulary");

const LAYERS = 8;
const TRANSPARENT = 0x0001;
const NOTHING = 0x0000;
// QMK's TAPPING_TOGGLE default (compat/qmk_tapping_contract.h).
const TAPPING_TOGGLE = 5;
// Settings the walk reads (users/noah/lib/profile/schema/profile_settings_v1.h).
const SETTING = Object.freeze({AUTO_MOUSE_ENABLED: 4, AUTO_MOUSE_LAYER: 5, AUTO_SNIPING_ENABLED: 8, AUTO_SNIPING_LAYER: 9,
    COMBOS_ENABLED: 20, DEFAULT_LAYERS: 23, COMBO_REFERENCES: 27});
const LEVELS = Object.freeze({TRAP: "trap", WARNING: "warning", NOTICE: "notice"});
// Enough to name a way in without listing every superset of one trap.
const MAX_TRAPS = 4;

const bit = (layer) => 1 << layer;
const layersIn = (mask) => Array.from({length: LAYERS}, (_, layer) => layer).filter((layer) => mask & bit(layer));
const codeName = (code) => (code >= 0x7e5c && code < 0x7e5c + LAYERS ? `LOCK_LAYER(${code - 0x7e5c})`
    : keycodes.resolve(code)?.name || `0x${code.toString(16).toUpperCase().padStart(4, "0")}`);

// What a native keycode does to the layers, if anything. `unowned` is a layer
// keycode the keyboard leaves to QMK (DF, PDF, a layer past the bank).
function layerEffects(code) {
    const within = (layer, effects) => (layer < LAYERS ? effects : [{kind: "unowned", layer}]);
    if (code >= 0x4000 && code <= 0x4fff) return within((code >> 8) & 15, [{kind: "hold", layer: (code >> 8) & 15}]);
    if (code >= 0x5000 && code <= 0x51ff) return within((code >> 5) & 15, [{kind: "hold", layer: (code >> 5) & 15}]);
    if (code >= 0x7e5c && code < 0x7e5c + LAYERS) return [{kind: "lock", layer: code - 0x7e5c}];
    if (code < 0x5200 || code >= 0x5300) return [];
    const layer = code & 31;
    switch (code & ~31) {
        case 0x5200: return within(layer, [{kind: "move", layer}]);
        case 0x5220: return within(layer, [{kind: "hold", layer}]);
        case 0x5260: return within(layer, [{kind: "lock", layer}]);
        case 0x5280: return within(layer, [{kind: "hold", layer, oneShot: true}]);
        case 0x52c0: return within(layer, [{kind: "hold", layer}, {kind: "lock", layer, taps: TAPPING_TOGGLE}]);
        case 0x5240: case 0x52e0: return [{kind: "unowned", layer}];
        default: return [];
    }
}

// What a stored action does to the layers.
function actionEffects(action) {
    if (!action) return [];
    if (action.kind === ACTION.LAYER_MOMENTARY) return action.operand < LAYERS ? [{kind: "hold", layer: action.operand}] : [];
    if (action.kind === ACTION.LAYER_LOCK) return action.operand < LAYERS ? [{kind: "lock", layer: action.operand}] : [];
    if (action.kind === ACTION.QMK_KEYCODE) return layerEffects(action.operand);
    return [];
}

// A branch as the taps and holds that reach it: tapIndex 0 is the first tap.
const TAP_WORDS = ["Tap", "Double-tap", "Triple-tap"];
const tapPhrase = (index, name) => (TAP_WORDS[index] ? `${TAP_WORDS[index]} ${name}` : `Tap ${name} ${index + 1} times`);
const holdPhrase = (index, name, longer) => `${index === 0 ? `Hold ${name}` : `Tap ${name}${index > 1 ? ` ${index} times` : ""}, then hold it`}${longer ? " longer" : ""}`;

// Every layer effect one key offers once it resolves to `code`: its own
// action, less the parts its behaviour row replaces, and each branch of that
// row. A hold branch that presses and holds until release holds the layer; any
// other hold, like a tap, sends the action once.
function keyEffects(code, row) {
    const effects = [];
    const first = row?.steps.find((step) => step.tapIndex === 0);
    for (const effect of layerEffects(code)) {
        if (effect.taps && row) continue;
        if (effect.kind === "hold" && !effect.oneShot ? first?.hold : first?.tap) continue;
        effects.push({...effect, how: effect.taps ? `Tap ${codeName(code)} ${effect.taps} times` : effect.kind === "hold" && !effect.oneShot ? `Hold ${codeName(code)}` : `Tap ${codeName(code)}`});
    }
    for (const step of row?.steps || []) {
        const sends = (action) => `its behaviour sends ${actionName(action)}`;
        for (const effect of actionEffects(step.tap)) {
            if (effect.kind !== "hold" || effect.oneShot) effects.push({...effect, how: tapPhrase(step.tapIndex, codeName(code)), note: sends(step.tap)});
        }
        for (const [part, longer] of [["hold", false], ["longHold", true]]) {
            const hold = step[part];
            for (const effect of actionEffects(hold?.action)) {
                const press = hold.mode === KEY_BEHAVIOR_HOLD_MODES.PRESS_AND_HOLD_UNTIL_RELEASE;
                if (effect.kind === "hold" && !press) continue;
                effects.push({...effect, how: holdPhrase(step.tapIndex, codeName(code), longer), note: sends(hold.action)});
            }
        }
    }
    return effects;
}

// The parts of a validated profile (portable-profile.validateSnapshot) the
// walk reads, in its own terms.
function profileFacts(decoded) {
    const values = decoded.settings.values;
    const names = decoded.settings.names;
    const layers = decoded.document.layers.map((codes) => CHARYBDIS_4X6_LAYOUT_MATRIX.map(([row, column]) => codes[row * 6 + column]));
    const rows = new Map();
    for (const row of decoded.behaviors?.rows || []) {
        const code = row.target.kind === ACTION.QMK_KEYCODE ? row.target.operand : nativeCode(row.target);
        if (Number.isInteger(code) && !rows.has(code)) rows.set(code, row);
    }
    const references = values[SETTING.COMBO_REFERENCES] >>> 0;
    const combos = values[SETTING.COMBOS_ENABLED] ? (decoded.combos || []).map((combo, index) => ({index,
        inputs: combo.inputs.map((input) => (input.kind === ACTION.QMK_KEYCODE ? input.operand : nativeCode(input))),
        output: combo.output})) : [];
    const pointer = values[SETTING.AUTO_MOUSE_ENABLED] ? values[SETTING.AUTO_MOUSE_LAYER] : undefined;
    const sniping = values[SETTING.AUTO_SNIPING_ENABLED] ? values[SETTING.AUTO_SNIPING_LAYER] : undefined;
    const allCombos = (decoded.combos || []).map((combo, index) => ({index, output: combo.output}));
    return {names, layers, rows, combos, allCombos, pointer, sniping,
        // Base is always on, and so are the default layers the keyboard keeps.
        always: 1 | (values[SETTING.DEFAULT_LAYERS] & 0xff),
        comboLayer: (layer) => (references >>> (layer * 4)) & 15};
}

// The code a key answers with while `active` layers are on, and the layer it
// comes from; null when every layer is transparent there.
function resolve(facts, active, position) {
    for (let layer = LAYERS - 1; layer >= 0; layer--) {
        if (!(active & bit(layer))) continue;
        const code = facts.layers[layer][position];
        if (code !== TRANSPARENT) return {layer, code};
    }
    return null;
}

const highest = (mask) => 31 - Math.clz32(mask);

// Everything that changes the layers from one state, each with where it came
// from so a path can be told.
function offers(facts, active, cache) {
    if (cache.has(active)) return cache.get(active);
    const found = [];
    facts.layers[0].forEach((_, position) => {
        const at = resolve(facts, active, position);
        if (!at || at.code === NOTHING) return;
        for (const effect of keyEffects(at.code, facts.rows.get(at.code))) {
            found.push({...effect, from: {layer: at.layer, layoutIndex: position, code: at.code},
                how: `${effect.how} on ${layerName(facts.names, at.layer)}${effect.note ? ` (${effect.note})` : ""}`});
        }
    });
    if (facts.combos.length) {
        const reference = facts.comboLayer(highest(active));
        const present = new Set(reference < LAYERS ? facts.layers[reference] : []);
        for (const combo of facts.combos) {
            if (!combo.inputs.length || !combo.inputs.every((code) => present.has(code))) continue;
            for (const effect of actionEffects(combo.output)) {
                if (effect.kind === "unowned") continue;
                found.push({...effect, from: {combo: combo.index}, how: `Press combo ${combo.index + 1} (${combo.inputs.map(codeName).join(" + ")}), which sends ${actionName(combo.output)}`});
            }
        }
    }
    // Moving the trackball wakes the pointer layer, unless the sniping layer
    // is on and keeps it off (pointer_layer_policy_apply).
    const stripped = facts.sniping !== undefined && facts.pointer !== facts.sniping && (active & bit(facts.sniping));
    if (facts.pointer !== undefined && facts.pointer < LAYERS && !stripped) {
        found.push({kind: "hold", layer: facts.pointer, from: {trackball: true}, how: `Move the trackball, which turns on ${layerName(facts.names, facts.pointer)}`});
    }
    cache.set(active, found);
    return found;
}

// A state is its locks and the layers held on top, one byte each.
const stateOf = (locks, held) => locks | (held << 8);
const locksOf = (state) => state & 0xff;
const heldOf = (state) => state >> 8;

function step(state, effect) {
    const locks = locksOf(state), held = heldOf(state);
    switch (effect.kind) {
        case "hold": return held & bit(effect.layer) ? state : stateOf(locks, held | bit(effect.layer));
        case "lock": return effect.layer === 0 ? state : stateOf(locks ^ bit(effect.layer), held);
        case "move": return stateOf(effect.layer === 0 ? 0 : bit(effect.layer), held);
        default: return state;
    }
}

// Walks every state reachable from rest. `edges` keeps, for each state, where
// each effect leads; `parent` the first way each state was reached.
function walk(facts) {
    const cache = new Map(), edges = new Map(), parent = new Map([[0, null]]);
    const queue = [0];
    for (let next = 0; next < queue.length; next++) {
        const state = queue[next];
        const active = facts.always | locksOf(state) | heldOf(state);
        const out = [];
        for (const effect of offers(facts, active, cache)) {
            if (effect.kind === "unowned") continue;
            out.push({to: step(state, effect), effect});
        }
        for (const layer of layersIn(heldOf(state))) out.push({to: stateOf(locksOf(state), heldOf(state) & ~bit(layer)), effect: {kind: "release", layer}});
        edges.set(state, out);
        for (const {to, effect} of out) {
            if (to === state || parent.has(to)) continue;
            parent.set(to, {from: state, effect});
            queue.push(to);
        }
    }
    // The states that can still get back to rest, walking the edges backwards.
    const back = new Map();
    for (const [from, out] of edges) for (const {to} of out) (back.get(to) || back.set(to, []).get(to)).push(from);
    const home = new Set([0]), pending = [0];
    while (pending.length) for (const from of back.get(pending.pop()) || []) if (!home.has(from)) {home.add(from); pending.push(from);}
    return {cache, edges, parent, home};
}

// How to get from rest to a state, in steps a person takes; releases are not
// steps.
function pathTo(parent, state) {
    const steps = [];
    for (let at = parent.get(state); at; at = parent.get(at.from)) if (at.effect.kind !== "release") steps.unshift(at.effect);
    return steps;
}

const namesOf = (facts, mask) => layersIn(mask).map((layer) => layerName(facts.names, layer));
const listed = (words) => (words.length < 2 ? words.join("") : `${words.slice(0, -1).join(", ")} and ${words[words.length - 1]}`);
// Where a finding is edited, in the review's place shapes (profile-review.js):
// a key on a layer slot of this profile, a combo, or a settings section.
const keyPlace = (layer, layoutIndex) => ({kind: "key", layer, layoutIndex});
const placeOf = (from) => (Number.isInteger(from?.layoutIndex) ? keyPlace(from.layer, from.layoutIndex)
    : Number.isInteger(from?.combo) ? {kind: "combo", index: from.combo} : undefined);

function traps(facts, walked) {
    const {edges, parent, home} = walked;
    // A set of locks is a trap once every key is let go and Base cannot be
    // reached from it; the way in is the lock that first leaves a state that
    // could still get home.
    const trapped = (locks) => parent.has(stateOf(locks, 0)) && !home.has(stateOf(locks, 0));
    const found = new Map();
    const depth = (state) => pathTo(parent, state).length;
    for (const [from, out] of edges) {
        if (trapped(locksOf(from))) continue;
        for (const {to, effect} of out) {
            const locks = locksOf(to);
            if (effect.kind === "release" || !trapped(locks)) continue;
            const known = found.get(locks);
            if (!known || depth(from) < depth(known.from)) found.set(locks, {from, effect});
        }
    }
    return [...found].sort(([a], [b]) => layersIn(a).length - layersIn(b).length || a - b).slice(0, MAX_TRAPS).map(([locks, entry]) => {
        const top = highest(locks);
        const names = namesOf(facts, locks);
        const path = [...pathTo(parent, entry.from), entry.effect];
        return {kind: "trap", level: LEVELS.TRAP, layers: layersIn(locks),
            title: `${listed(names)} can lock with no way back to ${layerName(facts.names, 0)}`,
            detail: `Once ${listed(names)} ${names.length === 1 ? "is" : "are"} locked, no key, behaviour or combo left on the board turns ${names.length === 1 ? "it" : "them"} off or moves back to ${layerName(facts.names, 0)}. Unplugging the keyboard clears it.`,
            path: path.map((effect) => effect.how),
            fix: `Put TO(0) or TG(${top}) on ${layerName(facts.names, top)}, where it answers, or take away the lock.`,
            place: placeOf(entry.effect.from)};
    });
}

function findings(facts, walked) {
    const results = [...traps(facts, walked)];
    const reached = [...walked.parent.keys()].reduce((mask, state) => mask | locksOf(state) | heldOf(state), facts.always);
    // A layer of transparent keys and KC_NO has nothing of its own to use.
    const hasKeys = (layer) => facts.layers[layer].some((code) => code !== TRANSPARENT && code !== NOTHING);

    for (let layer = 1; layer < LAYERS; layer++) {
        if (reached & bit(layer) || !hasKeys(layer)) continue;
        results.push({kind: "unreachable", level: LEVELS.WARNING, layers: [layer],
            title: `Nothing reaches ${layerName(facts.names, layer)}`,
            detail: `${layerName(facts.names, layer)} has keys, but no key, behaviour, combo or the trackball turns it on, so none of them can be used.`,
            fix: `Put a layer key for ${layerName(facts.names, layer)} (MO(${layer}), TG(${layer}), TO(${layer})…) on a layer you can reach.`,
            place: keyPlace(layer)});
    }

    // Layer keys that reach a layer with no keys of its own, once per layer.
    const empty = new Map();
    for (const offered of walked.cache.values()) for (const effect of offered) {
        if (effect.kind === "unowned" || effect.from.trackball || effect.layer === 0 || hasKeys(effect.layer) || empty.has(effect.layer)) continue;
        empty.set(effect.layer, effect);
    }
    for (const [layer, effect] of empty) {
        results.push({kind: "emptyTarget", level: LEVELS.NOTICE, layers: [layer],
            title: `A layer key reaches ${layerName(facts.names, layer)}, which has no keys of its own`,
            detail: `${effect.how}: ${layerName(facts.names, layer)} holds only transparent keys and KC_NO, so it adds nothing to the board.`,
            fix: `Give ${layerName(facts.names, layer)} keys, or take the layer key away.`,
            place: placeOf(effect.from)});
    }

    // Holding, toggling or locking a layer that is always on does nothing;
    // only TO() means something there. Every key, branch and combo output is
    // counted, reachable or not.
    const idle = (effects) => effects.some((effect) => effect.kind !== "move" && effect.kind !== "unowned" && facts.always & bit(effect.layer));
    const idlePlaces = [];
    facts.layers.forEach((codes, layer) => codes.forEach((code, position) => {
        if (idle(layerEffects(code))) idlePlaces.push({name: codeName(code), place: keyPlace(layer, position)});
    }));
    for (const [code, row] of facts.rows) for (const step of row.steps) {
        for (const action of [step.tap, step.hold?.action, step.longHold?.action]) {
            if (idle(actionEffects(action))) idlePlaces.push({name: `${actionName(action)} in the behaviour on ${codeName(code)}`, place: {kind: "behaviour", keycode: codeName(code)}});
        }
    }
    for (const combo of facts.allCombos) {
        if (idle(actionEffects(combo.output))) idlePlaces.push({name: `${actionName(combo.output)} on combo ${combo.index + 1}`, place: {kind: "combo", index: combo.index}});
    }
    if (idlePlaces.length) {
        const count = idlePlaces.length;
        results.push({kind: "idleLayerKey", level: LEVELS.NOTICE, layers: [0], count,
            title: `${count} layer key${count === 1 ? "" : "s"} hold${count === 1 ? "s" : ""} or toggle${count === 1 ? "s" : ""} ${layerName(facts.names, 0)}, which is always on`,
            detail: `${count > 3 ? `${idlePlaces.slice(0, 3).map((entry) => entry.name).join(", ")} and ${count - 3} more` : listed(idlePlaces.map((entry) => entry.name))}: ${layerName(facts.names, 0)} is always on, so holding, toggling or locking it does nothing. TO(0) is the one that means something there: it goes home.`,
            fix: "Point them at another layer, use TO(0) to go home, or give the keys something else to do.",
            place: idlePlaces[0].place});
    }

    const emptyBase = (code) => facts.layers[0].map((value, position) => value === code ? position : -1).filter(position => position >= 0);
    const transparent = emptyBase(TRANSPARENT);
    if (transparent.length) {
        results.push({kind: "deadBase", level: LEVELS.NOTICE, layers: [0], count: transparent.length,
            title: `${transparent.length} transparent key${transparent.length === 1 ? "" : "s"} on the base layer (${layerName(facts.names, 0)}) ${transparent.length === 1 ? "does" : "do"} nothing`,
            detail: `A transparent key on the base layer (${layerName(facts.names, 0)}) has no layer under it to answer, so it sends nothing unless a layer above covers it.`,
            fix: `Give ${transparent.length === 1 ? "it" : "them"} a keycode, or KC_NO to mark ${transparent.length === 1 ? "it" : "them"} intentionally empty.`,
            place: keyPlace(0, transparent[0])});
    }
    const no = emptyBase(NOTHING);
    if (no.length) {
        results.push({kind: "noBase", level: LEVELS.NOTICE, layers: [0], count: no.length,
            title: `${no.length} KC_NO key${no.length === 1 ? "" : "s"} on the base layer (${layerName(facts.names, 0)}) ${no.length === 1 ? "does" : "do"} nothing`,
            detail: `KC_NO on the base layer (${layerName(facts.names, 0)}) explicitly sends nothing unless a layer above covers it.`,
            fix: `Give ${no.length === 1 ? "it" : "them"} a keycode if ${no.length === 1 ? "it should" : "they should"} do something.`,
            place: keyPlace(0, no[0])});
    }

    if (facts.pointer !== undefined) {
        if (facts.always & bit(facts.pointer)) {
            results.push({kind: "pointerLayer", level: LEVELS.WARNING, layers: [facts.pointer],
                title: `The trackball turns on ${layerName(facts.names, facts.pointer)}, which is always on`,
                detail: "Moving the trackball changes nothing on the board, so the pointer layer's keys are never there when you mouse.",
                fix: "Choose the pointer layer under Mouse → Auto-mouse.", place: {kind: "settings", area: "Mouse"}});
        } else if (!hasKeys(facts.pointer)) {
            results.push({kind: "pointerLayer", level: LEVELS.NOTICE, layers: [facts.pointer],
                title: `The trackball turns on ${layerName(facts.names, facts.pointer)}, which has no keys of its own`,
                detail: "Moving the trackball changes nothing on the board.",
                fix: `Give ${layerName(facts.names, facts.pointer)} its mouse keys, or choose another pointer layer.`, place: keyPlace(facts.pointer)});
        }
    }

    facts.layers.forEach((codes, layer) => codes.forEach((code, position) => {
        if (!layerEffects(code).some((effect) => effect.kind === "unowned")) return;
        results.push({kind: "unowned", level: LEVELS.WARNING, layers: [layer],
            title: `${codeName(code)} on ${layerName(facts.names, layer)} bypasses layer tracking`,
            detail: `The keyboard leaves ${codeName(code)} to QMK's own layer code, so it can change the default layer or turn on a layer no key releases.`,
            fix: "Replace it with TO(), TG() or MO().", place: keyPlace(layer, position)});
    }));
    return results;
}

// A finding's identity across two profiles: what kind it is and which layers it
// is about, named by the keyboard layer each slot holds (`order[slot]`, as
// model/layer-order.js keeps it).
const IDENTITY = Object.freeze([0, 1, 2, 3, 4, 5, 6, 7]);
const findingKey = (finding, order = IDENTITY) => `${finding.kind}:${finding.layers.map((slot) => order[slot]).sort((a, b) => a - b).join(",")}`;

// The findings for a validated profile.
function layerReach(decoded) {
    if (decoded?.document?.layers?.length !== LAYERS) return [];
    const facts = profileFacts(decoded);
    return findings(facts, walk(facts));
}

// The findings for a draft beside its keyboard's: each finding in the draft is
// new or already on the keyboard, and each one the draft no longer has is fixed
// by it. The keyboard is walked as it is, not rearranged into the draft's
// order, since a reorder can make or mend a finding itself (a new base leaves
// the old one unreached); `order` says which keyboard layer each draft slot
// holds, so the two are matched layer with layer. Traps always ask to be
// confirmed.
function draftChecks(keyboard, draft, order = IDENTITY) {
    const before = keyboard ? layerReach(keyboard) : [], after = layerReach(draft);
    const known = new Set(before.map((finding) => findingKey(finding))), still = new Set(after.map((finding) => findingKey(finding, order)));
    return [
        ...after.map((finding) => ({...finding, key: findingKey(finding, order), status: known.has(findingKey(finding, order)) ? "existing" : "new"})),
        ...before.filter((finding) => !still.has(findingKey(finding))).map((finding) => ({...finding, key: findingKey(finding), status: "fixed", place: undefined})),
    ];
}

module.exports = {layerReach, draftChecks, findingKey, LEVELS};
