"use strict";

const {validateSnapshot, fingerprint, summary, reorderLayers} = require("../model/portable-profile");
const {profileReview} = require("../model/profile-review");
const {editSettings, settingsEditorView} = require("../model/settings-editor");
const {editMacro, macroEditorView} = require("../model/macro-editor");
const {editDeviceProfile, RGB_EDITS, COMBO_EDITS} = require("./device-profile-edits");
const {BEHAVIOR_EDITS} = require("./key-behavior-edits");
const {actionName, knownActionAbi} = require("./device-profile-view");
const {resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../protocol/via-layout-v1");
const keycodes = require("../data/keycode-catalog");
const {randomUUID} = require("node:crypto");
const copy = value => JSON.parse(JSON.stringify(value));
const fail = text => Object.assign(new Error(text), {code: "PROFILE_DRAFT_CONFLICT"});
const DRAFT_EDITS = new Set([...RGB_EDITS, ...COMBO_EDITS, ...BEHAVIOR_EDITS, "updateConfigDefaults", "updateViaMacro", "updateLayoutKeys", "applyAllChanges"]);

class ProfileDraftSession {
    constructor(snapshot, deviceId, capabilities) {
        if (!deviceId || snapshot.incomplete || capabilities.compiledLayerCount !== 8) throw fail("Read a complete eight-layer profile before editing.");
        validateSnapshot(snapshot.document, capabilities);
        this.deviceId = deviceId;
        this.id = randomUUID();
        this.capabilities = copy(capabilities);
        this.base = copy(snapshot);
        this.latest = copy(snapshot);
        this.history = [copy(snapshot.document)];
        this.cursor = 0;
        this.revision = 1;
        this.reviewedRevision = null;
    }
    get document() {return copy(this.history[this.cursor]);}
    get current() {return {...copy(this.base), incomplete: false, document: this.document, fingerprint: fingerprint(this.history[this.cursor]), summary: summary(this.history[this.cursor])};}
    get dirty() {return fingerprint(this.history[this.cursor]) !== this.base.fingerprint;}
    get stale() {return Boolean(this.needsRead || this.latest.fingerprint !== this.base.fingerprint);}
    observe(snapshot, deviceId) {
        if (!snapshot || deviceId !== this.deviceId) return;
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
        this.history = [copy(snapshot.document)]; this.cursor = 0;
        this.revision++; this.reviewedRevision = null;
        this.needsRead = false;
    }
    replace(document, revision) {
        this.assertRevision(revision);
        const valid = validateSnapshot(document, this.capabilities).document;
        if (fingerprint(valid) === this.current.fingerprint) return;
        this.history = this.history.slice(0, this.cursor + 1);
        this.history.push(copy(valid));
        if (this.history.length > 101) this.history.shift();
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
            if (message.expectedBase && ["source", "generation", "digest", "originHalf"].some(key => message.expectedBase[key] !== this.identity()[key])) throw fail("This behaviour form belongs to an older draft. Reload that row before keeping changes.");
            document = {...current.document, profile: editDeviceProfile(Buffer.from(current.document.profile, "base64"), message, {capabilities: this.capabilities, combos: this.combos()}).toString("base64")};
        }
        this.replace(document, message.draftRevision);
        return {message: copy(message), previousFingerprint: before, fingerprint: this.current.fingerprint};
    }
    editLayout(message) {
        if (message.adds?.length || message.deletes?.length) throw fail("Use Manage layers to name or reorder the eight available layers.");
        const groups = message.layoutGroups || message.layers || [{layer: message.layer, changes: message.changes}];
        if (!Array.isArray(groups) || !groups.length) throw fail("Choose keys to change.");
        const document = this.document;
        for (const group of groups) {
            const match = /^Layer ([0-7])$/.exec(group.layer);
            if (!match || !Array.isArray(group.changes)) throw fail("Choose a layer reported by this keyboard.");
            for (const change of group.changes) {
                const position = Number.isInteger(change.layoutIndex) && CHARYBDIS_4X6_LAYOUT_MATRIX[change.layoutIndex];
                const code = keycodes.encode(change.keycode) ?? (knownActionAbi(this.capabilities.actionAbiDigest) ? resolveNativeQmkExpression(change.keycode, {}) : undefined);
                if (!position || !Number.isInteger(code)) throw fail(`Cannot represent the key ${change.keycode} on this keyboard.`);
                document.layers[Number(match[1])][position[0] * 6 + position[1]] = code;
            }
        }
        return document;
    }
    reorder(order, names, revision) {this.replace(reorderLayers(this.document, order, names), revision);}
    undo(revision) {this.assertRevision(revision); if (this.cursor) {this.cursor--; this.revision++; this.reviewedRevision = null;}}
    redo(revision) {this.assertRevision(revision); if (this.cursor + 1 < this.history.length) {this.cursor++; this.revision++; this.reviewedRevision = null;}}
    review(revision) {this.assertRevision(revision); this.reviewedRevision = this.revision;}
    rebase(revision) {
        this.assertRevision(revision, {allowStale: true});
        if (this.needsRead) throw fail("Read the latest keyboard state before reviewing this draft again.");
        const target = this.document;
        if (this.latest.incomplete) {
            this.base = copy(this.latest); this.history = [target]; this.cursor = 0;
            this.revision++; this.reviewedRevision = this.revision; return;
        }
        this.reset(this.latest);
        this.replace(target, this.revision);
        this.reviewedRevision = this.revision;
    }
    async apply(service, revision, saveRecovery) {
        this.assertRevision(revision);
        if (!this.dirty || this.reviewedRevision !== revision) throw fail("Review the current draft before applying it.");
        if (!service.snapshot().connected || service.snapshot().selectedDeviceId !== this.deviceId) throw fail("Reconnect the keyboard this draft belongs to.");
        try {
            const result = await service.restorePortableProfile(this.document, {expectedFingerprint: this.base.fingerprint, saveRecovery});
            if (result.fingerprint !== this.current.fingerprint) {this.needsRead = true; throw fail("The saved profile did not match the draft. Keep the recovery copy and read the keyboard again.");}
            this.reset(result);
            return result;
        } catch (error) {
            if (["RESTORE_INCOMPLETE", "PROFILE_CHANGED", "RESTORE_VERIFY_FAILED"].includes(error.code)) this.needsRead = true;
            throw error;
        }
    }
    identity() {return {source: "draft", generation: this.revision, digest: this.current.fingerprint, originHalf: this.deviceId};}
    combos() {
        const {combos, settings} = validateSnapshot(this.document);
        const native = a => a.kind === 1 ? a.operand : resolveNativeQmkExpression(actionName(a), {});
        return {state: "read", enabled: Boolean(settings.values[20]), layerReferences: Array.from({length: 8}, (_, i) => (settings.values[27] >>> (4 * i)) & 15),
            holdTermMs: combos[0]?.holdTermMs || 0, rows: combos.map(row => ({...row, inputs: row.inputs.map(native), output: native(row.output)}))};
    }
    editingState(state) {
        if (state.selectedDeviceId !== this.deviceId) return state;
        const current = this.current, value = validateSnapshot(current.document), values = value.settings.values;
        const max = current.limits?.brightnessMax;
        const brightness = (values[22] >>> 16) & 255;
        const baseRgb = max === undefined ? state.baseRgb : {state: "read", effectId: values[21] & 255 ? (values[21] >>> 8) & 255 : 0,
            brightness: max ? Math.min(255, Math.round(brightness * 255 / max)) : 0, hue: values[22] & 255, saturation: (values[22] >>> 8) & 255, speed: (values[21] >>> 16) & 255};
        return {...state, busy: state.busy || this.stale || !state.connected,
            layout: {state: "read", layers: current.document.layers.map((values, layer) => ({layer, keys: CHARYBDIS_4X6_LAYOUT_MATRIX.map(([row, column], layoutIndex) => ({row, column, layoutIndex, keycode: values[row * 6 + column], resolved: keycodes.resolve(values[row * 6 + column])}))}))},
            committed: {...state.committed, state: "read", failures: [], domains: {rgb: value.rgb, keyBehaviors: value.behaviors, settings: value.settings}},
            baseRgb, combos: this.combos(), macroView: macroEditorView(current), settingsView: settingsEditorView(current)};
    }
    view(state) {
        const matching = state.selectedDeviceId === this.deviceId, connected = matching && state.connected;
        return {id: this.id, revision: this.revision, dirty: this.dirty, stale: this.stale, connected, matching,
            canUndo: this.cursor > 0, canRedo: this.cursor + 1 < this.history.length,
            reviewed: this.reviewedRevision === this.revision,
            changes: !this.dirty ? [] : this.base.incomplete ? [{area: "Recovery", label: "Complete profile", before: "Interrupted configuration; a full comparison is unavailable", after: `${this.current.summary.layers} layers, ${this.current.summary.behaviors} behaviours, ${this.current.summary.combos} combos, ${this.current.summary.macros} macros with content, lighting and settings`}] : profileReview(this.base, this.current)};
    }
}
module.exports = {ProfileDraftSession, DRAFT_EDITS};
