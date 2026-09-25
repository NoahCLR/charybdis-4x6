"use strict";

const {validateSnapshot, fingerprint, fingerprintOf, summaryOf} = require("../model/portable-profile");
const {layerOrderReview, profileReview} = require("../model/profile-review");
const {IDENTITY, compose, inverse, isIdentity, layerUnit, rearranged} = require("../model/layer-order");
const {layerName: layerCalled} = require("../model/vocabulary");
const {revertUnits} = require("../model/profile-revert");
const {baseLighting, editSettings, settingsEditorView} = require("../model/settings-editor");
const {editMacro, macroEditorView} = require("../model/macro-editor");
const {editDeviceProfile, RGB_EDITS, COMBO_EDITS, PD_EDITS} = require("./device-profile-edits");
const {BEHAVIOR_EDITS} = require("./key-behavior-edits");
const {actionName, knownActionAbi, layerOfRef, nativeCode} = require("../schema/actions");
const {resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../data/charybdis-layout");
const keycodes = require("../data/keycode-catalog");
const {randomUUID} = require("node:crypto");
const copy = value => JSON.parse(JSON.stringify(value));
// History entries are frozen: they are handed out by reference, decoded once
// and kept, so nothing may edit one in place.
const freeze = value => {
    if (value && typeof value === "object" && !Object.isFrozen(value)) {
        Object.values(value).forEach(freeze);
        Object.freeze(value);
    }
    return value;
};
const fail = text => Object.assign(new Error(text), {code: "PROFILE_DRAFT_CONFLICT"});

// What one staged message did, in the words a group of review items is
// titled with. Only a group of two or more items shows it.
const EDIT_LABELS = {
    retargetBehavior: message => message.conflict === "swap" ? "Swapped two behaviours" : "Moved a behaviour",
    saveBehavior: "Edited a behaviour", addBehavior: "Added a behaviour", deleteBehavior: "Removed a behaviour",
    addCombo: "Added a combo", saveCombo: "Edited a combo", deleteCombo: "Removed a combo", updateComboHoldTerm: "Changed the combo hold threshold",
    savePdMode: "Edited a pointing mode", clearPdMode: "Cleared a pointing mode", duplicatePdMode: "Duplicated a pointing mode",
    updateConfigDefaults: "Saved a settings section", updateViaMacro: "Edited a macro", applyAllChanges: "Changed keys",
};
function editLabel(message, document) {
    if (message.type === "updateLayoutKeys") {
        const groups = message.layoutGroups || message.layers || [{layer: message.layer, changes: message.changes}];
        const changes = groups.flatMap(group => (group.changes || []).map(change => ({...change, layer: group.layer})));
        const [first, second] = changes;
        const stored = change => {
            const position = CHARYBDIS_4X6_LAYOUT_MATRIX[change.layoutIndex], layer = layerOfRef(change.layer);
            return position && document.layers[layer]?.[position[0] * 6 + position[1]];
        };
        if (changes.length === 2 && first.layer === second.layer && keycodes.encode(first.keycode) === stored(second) && keycodes.encode(second.keycode) === stored(first)) return "Swapped two keys";
        return changes.length > 1 ? `Changed ${changes.length} keys at once` : "Changed a key";
    }
    if (RGB_EDITS.has(message.type)) return "Edited lighting";
    const label = EDIT_LABELS[message.type];
    return typeof label === "function" ? label(message) : label || "Edited the draft";
}

// What one Edit layers save did: the order it changed and the names it
// changed, each named as the layers were called before it.
function layersLabel(step, before, after) {
    const moved = step.map((from, slot) => slot).filter(slot => step[slot] !== slot);
    const renamed = step.map((from, slot) => [from, slot]).filter(([from, slot]) => layerCalled(before, from) !== layerCalled(after, slot));
    const parts = [];
    if (moved.length === 2) parts.push(`Swapped ${layerCalled(before, moved[0])} and ${layerCalled(before, moved[1])}`);
    else if (moved.length) parts.push("Reordered layers");
    if (renamed.length === 1) parts.push(`Renamed ${layerCalled(before, renamed[0][0])} to ${layerCalled(after, renamed[0][1])}`);
    else if (renamed.length) parts.push(`Renamed ${renamed.length} layers`);
    return parts.map((part, index) => index ? part[0].toLowerCase() + part.slice(1) : part).join(", ") || "Edited layers";
}

// Review items name what the model cannot: the keycode a behaviour is listed
// under, the name its edit messages use, so the review can go there.
function placed(item) {
    if (item.place?.kind === "behaviour") return {...item, place: {kind: "behaviour", keycode: actionName(item.place.target)}};
    return item;
}
const DRAFT_EDITS = new Set([...PD_EDITS, ...RGB_EDITS, ...COMBO_EDITS, ...BEHAVIOR_EDITS, "updateConfigDefaults", "updateViaMacro", "updateLayoutKeys", "applyAllChanges"]);

class ProfileDraftSession {
    constructor(snapshot, deviceId, capabilities, connectionToken = null) {
        if (!deviceId || snapshot.incomplete || capabilities.compiledLayerCount !== 8) throw fail("Read a complete eight-layer profile before editing.");
        validateSnapshot(snapshot.document, capabilities);
        this.deviceId = deviceId;
        this.connectionToken = connectionToken ?? null;
        this.latestConnectionToken = this.connectionToken;
        this.connectionChanged = false;
        this.id = randomUUID();
        this.capabilities = copy(capabilities);
        this.base = copy(snapshot);
        this.latest = copy(snapshot);
        this.history = [freeze(copy(snapshot.document))];
        // Why each history entry exists, beside it: "edit" is one staged
        // message, the only kind that ties the units it changed together.
        this.origins = [null];
        this.labels = [null];
        // When each entry was made, for the draft history.
        this.times = [this.now()];
        // Which keyboard layer each slot of the entry holds (layer-order.js).
        this.orders = [IDENTITY];
        this.cursor = 0;
        this.revision = 1;
        this.reviewedRevision = null;
    }
    now() {return Date.now();}
    get document() {return copy(this.history[this.cursor]);}
    get order() {return this.orders[this.cursor];}
    // A history entry decoded once: its validated form (read-only), its
    // fingerprint and summary. Entries are never edited in place, so the
    // answer is kept with the entry for as long as the entry exists.
    decode(document) {
        this.decodeCache ??= new WeakMap();
        let known = this.decodeCache.get(document);
        if (!known) {
            const decoded = validateSnapshot(document);
            known = {decoded, fingerprint: fingerprintOf(decoded), summary: summaryOf(decoded)};
            this.decodeCache.set(document, known);
        }
        return known;
    }
    // The draft's revision as a snapshot: the base's keyboard facts with this
    // revision's document, plus its decoded form for read-only consumers.
    get current() {
        const {decoded, fingerprint: print, summary: brief} = this.decode(this.history[this.cursor]);
        return {...this.base, incomplete: false, document: this.history[this.cursor], fingerprint: print, summary: brief, decoded};
    }
    // The keyboard's side as a snapshot, decoded once too.
    get baseSnapshot() {
        return this.base.incomplete ? this.base : {...this.base, decoded: this.decode(this.base.document).decoded};
    }
    get dirty() {return this.decode(this.history[this.cursor]).fingerprint !== this.base.fingerprint;}
    get stale() {return Boolean(this.needsRead || this.connectionChanged || this.latest.fingerprint !== this.base.fingerprint);}
    noteConnection(connectionToken) {
        const token = connectionToken ?? null;
        if (token !== this.connectionToken && token !== this.latestConnectionToken) {
            this.connectionChanged = true;
            this.needsRead = true;
        }
    }
    observe(snapshot, deviceId, connectionToken = null) {
        if (!snapshot || deviceId !== this.deviceId) return;
        const token = connectionToken ?? null;
        this.latestConnectionToken = token;
        if (token !== this.connectionToken) {
            if (this.dirty) this.connectionChanged = true;
            else {this.connectionToken = token; this.connectionChanged = false;}
        }
        this.needsRead = false;
        if (snapshot.incomplete) {this.latest = copy(snapshot); return;}
        if (!this.dirty) {
            if (snapshot.fingerprint !== this.base.fingerprint) this.reset(snapshot);
            else {this.base = copy(snapshot); this.latest = copy(snapshot);}
        } else this.latest = copy(snapshot);
    }
    assertRevision(revision, {allowStale = false} = {}) {
        if (revision !== this.revision) throw fail("The draft changed. Review the current changes before continuing.");
        if (this.stale && !allowStale) throw fail("The keyboard changed. Review your draft against the latest keyboard state before continuing.");
    }
    reset(snapshot) {
        this.base = copy(snapshot); this.latest = copy(snapshot);
        this.history = [freeze(copy(snapshot.document))]; this.origins = [null]; this.labels = [null]; this.times = [this.now()]; this.orders = [IDENTITY]; this.cursor = 0;
        this.revision++; this.reviewedRevision = null;
        this.needsRead = false;
        this.connectionChanged = false;
    }
    // A new entry keeps the entry's layer order unless it says otherwise.
    replace(document, revision, origin = "edit", label = null, order = this.order) {
        this.assertRevision(revision);
        const valid = validateSnapshot(document, this.capabilities).document;
        if (fingerprint(valid) === this.decode(this.history[this.cursor]).fingerprint) return;
        this.history = this.history.slice(0, this.cursor + 1);
        this.origins = this.origins.slice(0, this.cursor + 1);
        this.labels = this.labels.slice(0, this.cursor + 1);
        this.times = this.times.slice(0, this.cursor + 1);
        this.orders = this.orders.slice(0, this.cursor + 1);
        this.history.push(freeze(copy(valid)));
        this.origins.push(origin);
        this.labels.push(label);
        this.times.push(this.now());
        this.orders.push(Object.freeze([...order]));
        if (this.history.length > 101) {this.history.shift(); this.origins.shift(); this.labels.shift(); this.times.shift(); this.orders.shift();}
        this.cursor = this.history.length - 1;
        this.revision++; this.reviewedRevision = null;
    }
    stage(message) {
        this.assertRevision(message.draftRevision);
        const current = this.current, before = current.fingerprint;
        let document;
        if (message.type === "updateConfigDefaults") document = editSettings(current, {...message, expectedFingerprint: message.expectedFingerprint || before}, this.capabilities);
        else if (message.type === "updateViaMacro") document = editMacro(current, {...message, expectedFingerprint: message.expectedFingerprint || before}, this.capabilities);
        else if (["updateLayoutKeys", "applyAllChanges"].includes(message.type)) document = this.editLayout(message);
        else {
            if (!DRAFT_EDITS.has(message.type)) throw fail("Unsupported draft edit.");
            if (message.expectedBase && ["source", "generation", "digest", "originHalf"].some(key => message.expectedBase[key] !== this.identity()[key])) throw fail("This form belongs to an older draft. Reload it before keeping changes.");
            document = {...current.document, profile: editDeviceProfile(Buffer.from(current.document.profile, "base64"), message, {
                capabilities: this.capabilities, combos: this.combos(), maximumBrightness: current.limits?.brightnessMax,
            }).toString("base64")};
        }
        this.replace(document, message.draftRevision, "edit", editLabel(message, current.document));
        return {message: copy(message), previousFingerprint: before, fingerprint: this.decode(this.history[this.cursor]).fingerprint};
    }
    // An Edit layers save: its document, and the order it saved, which
    // rearranges this revision. An order and each name are separate
    // decisions, so the step links nothing in the review.
    editLayers(document, revision, step) {
        const before = this.current.decoded.settings.names, after = validateSnapshot(document, this.capabilities).settings.names;
        this.replace(document, revision, "layers", layersLabel(step, before, after), compose(this.order, step));
    }
    editLayout(message) {
        if (message.adds?.length || message.deletes?.length) throw fail("Use Manage layers to name or reorder the eight available layers.");
        const groups = message.layoutGroups || message.layers || [{layer: message.layer, changes: message.changes}];
        if (!Array.isArray(groups) || !groups.length) throw fail("Choose keys to change.");
        const document = this.document;
        for (const group of groups) {
            const layer = layerOfRef(group.layer);
            if (!(layer < 8) || !Array.isArray(group.changes)) throw fail("Choose a layer reported by this keyboard.");
            for (const change of group.changes) {
                const position = Number.isInteger(change.layoutIndex) && CHARYBDIS_4X6_LAYOUT_MATRIX[change.layoutIndex];
                const code = keycodes.encode(change.keycode) ?? (knownActionAbi(this.capabilities.actionAbiDigest) ? resolveNativeQmkExpression(change.keycode, {}) : undefined);
                if (!position || !Number.isInteger(code)) throw fail(`Cannot represent the key ${change.keycode} on this keyboard.`);
                document.layers[layer][position[0] * 6 + position[1]] = code;
            }
        }
        return document;
    }
    // The keyboard's profile rearranged into an order, with the order it
    // stands for; the keyboard's own profile, and no order, when the order
    // moves nothing, including a reorder nothing can tell apart.
    referenceFor(order) {
        const base = this.baseSnapshot;
        if (this.base.incomplete || isIdentity(order)) return {snapshot: base, order: null};
        this.referenceCache ??= new WeakMap();
        const known = this.referenceCache.get(order);
        if (known?.base === this.base) return known.reference;
        const decoded = validateSnapshot(rearranged(this.base.document, order), this.capabilities), print = fingerprintOf(decoded);
        const reference = print === this.base.fingerprint ? {snapshot: base, order: null}
            : {snapshot: {...this.base, document: decoded.document, decoded, fingerprint: print, summary: summaryOf(decoded)}, order};
        this.referenceCache.set(order, {base: this.base, reference});
        return reference;
    }
    // What a document in an order differs from the keyboard by: the order,
    // then everything else compared with the reference.
    describe(document, order) {
        const {snapshot, order: moved} = this.referenceFor(order);
        const after = {...this.base, incomplete: false, document, ...this.decode(document)};
        return [...(moved ? [layerOrderReview(after, moved)] : []), ...profileReview(snapshot, after)];
    }
    // The review rows, each with the group it belongs to. Rows are grouped by
    // the edits that made them: the units one staged message changed belong
    // together (a swap, a moved behaviour), and so does
    // anything linked to them through a later edit. Only units the review
    // still shows link; a unit edited back to the keyboard's value no longer
    // ties anything. Steps from a rebase, a discard, Edit layers, or before
    // the bounded history leave their units on their own. Units link by the
    // layer they belong to, not the slot, so a reorder between two edits
    // never ties two different layers' keys.
    changes() {
        if (!this.dirty) return [];
        const current = this.current;
        if (this.base.incomplete) return [{area: "Recovery", unit: null, title: "Complete profile", status: "changed", group: null, place: null,
            fields: [{label: "", before: "Interrupted configuration; a full comparison is unavailable", after: `${current.summary.layers} layers, ${current.summary.behaviors} behaviours, ${current.summary.combos} combos, ${current.summary.macros} macros with content, lighting and settings`}]}];
        const rows = this.describe(current.document, this.order), shown = new Set(rows.map(row => layerUnit(row.unit, this.order)));
        const parent = new Map([...shown].map(unit => [unit, unit]));
        const find = unit => {while (parent.get(unit) !== unit) unit = parent.get(unit); return unit;};
        const steps = [];
        for (let step = 1; step <= this.cursor; step++) {
            if (this.origins[step] !== "edit") continue;
            const touched = this.stepUnits(step).map(unit => layerUnit(unit, this.orders[step])).filter(unit => shown.has(unit));
            for (const unit of touched.slice(1)) parent.set(find(unit), find(touched[0]));
            if (touched.length) steps.push({label: this.labels[step], unit: touched[0]});
        }
        // A group is titled by the edits that made it, in the order they were
        // made: one edit by its own words, several by the first and a count.
        const titles = new Map();
        for (const {label, unit} of steps) {
            const root = find(unit), list = titles.get(root) || [];
            if (label && !list.includes(label)) list.push(label);
            titles.set(root, list);
        }
        const title = list => !list?.length ? null : list.length === 1 ? list[0] : `${list[0]} and ${list.length - 1} more edit${list.length === 2 ? "" : "s"}`;
        const groups = new Map();
        return rows.map(row => {
            const root = find(layerUnit(row.unit, this.order));
            if (!groups.has(root)) groups.set(root, groups.size);
            return placed({...row, group: groups.get(root), groupTitle: title(titles.get(root))});
        });
    }
    // The units one history step changed, remembered with the entry: history
    // entries are never edited in place, so the answer never goes stale.
    stepUnits(step) {
        this.stepCache ??= new WeakMap();
        const entry = this.history[step], previous = this.history[step - 1], cached = this.stepCache.get(entry);
        if (cached?.previous === previous) return cached.units;
        const snapshot = document => ({...this.base, incomplete: false, document, ...this.decode(document)});
        const units = [...new Set(profileReview(snapshot(previous), snapshot(entry)).map(row => row.unit))];
        this.stepCache.set(entry, {previous, units});
        return units;
    }
    // The draft history, oldest first: every entry with when it was made, in
    // the words it was recorded with, and what it changed from the entry
    // before it (not from the keyboard). A step that moved layers compares
    // against the entry before it moved the same way, so a reorder is one
    // item and everything else is compared layer with layer. The first
    // entry has nothing before it: where the draft began, or the oldest one
    // the bounded history still keeps.
    steps() {
        return this.history.map((entry, step) => ({
            step, at: this.times[step], origin: this.origins[step],
            label: this.labels[step] || (step ? "Reviewed against the keyboard" : "Read from the keyboard"),
            current: step === this.cursor, undone: step > this.cursor,
            changes: step ? this.stepReview(step) : null,
        }));
    }
    stepReview(step) {
        this.reviewCache ??= new WeakMap();
        const entry = this.history[step], previous = this.history[step - 1], cached = this.reviewCache.get(entry);
        const before = this.orders[step - 1], after = this.orders[step];
        if (cached?.previous === previous && cached.before === before && cached.after === after) return cached.rows;
        const snapshot = document => ({...this.base, incomplete: false, document, ...this.decode(document)});
        const back = inverse(before), moved = Object.freeze(after.map(layer => back[layer]));
        const now = snapshot(entry);
        const rows = isIdentity(moved) ? profileReview(snapshot(previous), now)
            : [layerOrderReview(now, moved), ...profileReview(snapshot(validateSnapshot(rearranged(previous, moved), this.capabilities).document), now)];
        const shown = rows.map(placed);
        this.reviewCache.set(entry, {previous, before, after, rows: shown});
        return shown;
    }
    // Discard one group of review rows: its units go back to what the keyboard
    // holds, as one more undoable step. A review that was current stays
    // current, since what is left is part of what was reviewed.
    discard(revision, group) {
        this.assertRevision(revision);
        if (this.base.incomplete) throw fail("Review this recovery as a whole; its changes cannot be discarded one by one.");
        const items = this.changes().filter(row => row.group === group), units = new Set(items.map(row => row.unit));
        if (!units.size) throw fail("That change is no longer in the draft.");
        const reviewed = this.reviewedRevision === revision;
        // The order goes back by moving the layers back, so everything else
        // moves with its layer and stays; anything else goes back to the
        // reference, so the order stays.
        let document, order = this.order;
        if (units.has("layerOrder")) {
            if (units.size > 1) throw fail("Discard the layer order on its own.");
            document = rearranged(this.current.document, inverse(order));
            order = IDENTITY;
        } else document = revertUnits(this.referenceFor(order).snapshot, this.current, units, this.capabilities);
        // Some bytes carry no row of their own, such as the settings format a
        // macro name upgraded. Once nothing described is left, the draft is
        // the keyboard's profile again, not an indescribable difference.
        const left = this.describe(validateSnapshot(document, this.capabilities).document, order);
        if (left.every(row => row.unit === "profile")) {document = copy(validateSnapshot(this.base.document, this.capabilities).document); order = IDENTITY;}
        const label = units.has("layerOrder") ? "Put the layer order back" : items.length === 1 ? `Discarded ${items[0].title}` : `Discarded ${items.length} changes made together`;
        this.replace(document, revision, "discard", label, order);
        if (reviewed) this.reviewedRevision = this.revision;
    }
    // Discard the whole draft as one more step, so undo brings it back: the
    // keyboard is not read again, since nothing about it changed.
    discardAll(revision) {
        this.assertRevision(revision);
        if (this.base.incomplete) throw fail("Read the keyboard again to discard a recovery draft.");
        if (!this.dirty) return;
        const count = this.changes().length;
        this.replace(validateSnapshot(this.base.document, this.capabilities).document, revision, "discard", `Discarded the draft (${count} change${count === 1 ? "" : "s"})`, IDENTITY);
    }
    undo(revision) {this.assertRevision(revision); if (this.cursor) {this.cursor--; this.revision++; this.reviewedRevision = null;}}
    redo(revision) {this.assertRevision(revision); if (this.cursor + 1 < this.history.length) {this.cursor++; this.revision++; this.reviewedRevision = null;}}
    // Several undos or redos at once: straight to one entry of the history.
    jump(revision, step) {
        this.assertRevision(revision);
        if (!Number.isInteger(step) || step < 0 || step >= this.history.length) throw fail("That step is no longer in the draft history.");
        if (step !== this.cursor) {this.cursor = step; this.revision++; this.reviewedRevision = null;}
    }
    review(revision) {this.assertRevision(revision); this.reviewedRevision = this.revision;}
    // Leaving the review un-reviews the draft, even one gone stale meanwhile.
    closeReview(revision) {this.assertRevision(revision, {allowStale: true}); this.reviewedRevision = null;}
    rebase(revision) {
        this.assertRevision(revision, {allowStale: true});
        if (this.needsRead) throw fail("Read the latest keyboard state before reviewing this draft again.");
        const target = this.document, order = this.order;
        if (this.latest.incomplete) {
            this.base = copy(this.latest); this.history = [freeze(target)]; this.origins = [null]; this.labels = [null]; this.times = [this.now()]; this.orders = [IDENTITY]; this.cursor = 0;
            this.connectionToken = this.latestConnectionToken; this.connectionChanged = false;
            this.revision++; this.reviewedRevision = this.revision; return;
        }
        this.reset(this.latest);
        this.connectionToken = this.latestConnectionToken;
        // The order carries over: the reference and the items still describe
        // the whole difference to the keyboard read now.
        this.replace(target, this.revision, "rebase", null, order);
        this.reviewedRevision = this.revision;
    }
    async apply(service, revision, saveRecovery) {
        this.assertRevision(revision);
        if (!this.dirty || this.reviewedRevision !== revision) throw fail("Review the current draft before applying it.");
        if (!service.snapshot().connected || service.snapshot().selectedDeviceId !== this.deviceId) throw fail("Reconnect the keyboard this draft belongs to.");
        if ((service.snapshot().connectionToken ?? null) !== this.connectionToken) throw fail("Review this draft against the connected keyboard before applying it.");
        try {
            const result = await service.restorePortableProfile(this.document, {expectedFingerprint: this.base.fingerprint, saveRecovery});
            if (result.fingerprint !== this.current.fingerprint) {this.needsRead = true; throw fail("The saved profile did not match the draft. Keep the recovery copy and read the keyboard again.");}
            this.reset(result);
            return result;
        } catch (error) {
            if (["RESTORE_INCOMPLETE", "RESTORE_NOT_SAVED", "PROFILE_CHANGED", "RESTORE_VERIFY_FAILED"].includes(error.code)) this.needsRead = true;
            throw error;
        }
    }
    identity() {return {source: "draft", generation: this.revision, digest: this.decode(this.history[this.cursor]).fingerprint, originHalf: this.deviceId};}
    combos() {
        const {combos, settings} = this.decode(this.history[this.cursor]).decoded;
        const native = nativeCode;
        return {state: "read", enabled: Boolean(settings.values[20]), layerReferences: Array.from({length: 8}, (_, i) => (settings.values[27] >>> (4 * i)) & 15),
            holdTermMs: combos[0]?.holdTermMs || 0, rows: combos.map(row => ({...row, inputs: row.inputs.map(native), output: native(row.output)}))};
    }
    editingState(state) {
        if (state.selectedDeviceId !== this.deviceId) return state;
        const current = this.current, value = current.decoded, values = value.settings.values;
        const max = current.limits?.brightnessMax;
        const lighting = baseLighting(values);
        const baseRgb = max === undefined ? state.baseRgb : {state: "read", effectId: lighting.enabled ? lighting.effect : 0,
            brightness: max ? Math.min(255, Math.round(lighting.brightness * 255 / max)) : 0, hue: lighting.hue, saturation: lighting.saturation, speed: lighting.speed};
        return {...state, busy: state.busy || this.stale || !state.connected,
            layout: {state: "read", layers: current.document.layers.map((values, layer) => ({layer, keys: CHARYBDIS_4X6_LAYOUT_MATRIX.map(([row, column], layoutIndex) => ({row, column, layoutIndex, keycode: values[row * 6 + column], resolved: keycodes.resolve(values[row * 6 + column])}))}))},
            committed: {...state.committed, state: "read", failures: [], domains: {rgb: value.rgb, keyBehaviors: value.behaviors, settings: value.settings, pdModes: value.pdModes}},
            baseRgb, combos: this.combos(), macroView: macroEditorView(current, this.capabilities), settingsView: settingsEditorView(current)};
    }
    view(state) {
        const matching = state.selectedDeviceId === this.deviceId, connected = matching && state.connected;
        return {id: this.id, revision: this.revision, dirty: this.dirty, stale: this.stale, connectionChanged: this.connectionChanged, connected, matching,
            canUndo: this.cursor > 0, canRedo: this.cursor + 1 < this.history.length,
            // What undo and redo would take back or bring back, in the words
            // the step was recorded with.
            undoLabel: this.cursor > 0 ? this.labels[this.cursor] : null,
            redoLabel: this.cursor + 1 < this.history.length ? this.labels[this.cursor + 1] : null,
            // How many steps the history holds; the steps themselves come
            // only while the history sheet is open (steps()).
            historyLength: this.history.length,
            reviewed: this.reviewedRevision === this.revision,
            changes: this.changes()};
    }
}
module.exports = {ProfileDraftSession, DRAFT_EDITS};
