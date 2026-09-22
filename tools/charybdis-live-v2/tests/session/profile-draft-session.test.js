"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {ProfileDraftSession} = require("../../core/session/profile-draft-session");
const {document} = require("../fixtures/portable-profile");
const {options} = require("../fixtures/keyboard-options");
const {fingerprint, summary, validateSnapshot} = require("../../core/model/portable-profile");
const {settingsEditorView} = require("../../core/model/settings-editor");
const {behaviorRowsForView} = require("../../core/session/device-profile-view");
function fixture() {
    const value = document(), snapshot = {document:value, fingerprint:fingerprint(value), summary:summary(value), limits:{brightnessMax:200}, options:options()};
    const caps = {compiledLayerCount:8,supportedDomainMask:15,actionAbiDigest:value.actionAbiDigest};
    return {snapshot, caps, draft:new ProfileDraftSession(snapshot, "board", caps)};
}
function settings(draft, id, updates) {
    const section = settingsEditorView(draft.current).sections.find(row => row.id === id);
    return {type:"updateConfigDefaults", sectionId:id, expectedFingerprint:draft.current.fingerprint,
        fields:section.fields.map(field => field.kind === "toggle" ? {macro:field.macro, enabled:updates[field.macro] ?? field.enabled} : {macro:field.macro,value:updates[field.macro] ?? field.value})};
}
function stage(draft, edit) {return draft.stage({...edit,draftRevision:draft.revision});}
test("one draft composes every editor without changing its device snapshot", () => {
    const {draft,snapshot} = fixture(), original = JSON.stringify(snapshot);
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 0",changes:[{layoutIndex:0,keycode:"KC_A"}]}]});
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"hello"});
    stage(draft,{type:"updateViaMacro",keycode:"MACRO_0",payload:"{KC_LGUI,KC_N}"});
    stage(draft,settings(draft,"keyTiming",{tapHoldTerm:"175"}));
    const row = behaviorRowsForView(validateSnapshot(draft.document).behaviors)[0];
    stage(draft,{type:"saveBehavior",behavior:{...row,tapHoldTerm:"210"},expectedBase:draft.identity()});
    stage(draft,{type:"updateLayerColor",layer:"Layer 0",h:"23",s:"255",v:"100",mode:"KEYS_MAPPED_ON_THIS_LAYER_ONLY"});
    stage(draft,{type:"saveCombo",id:0,inputs:["KC_A","KC_B"],output:"G(KC_N)",termMs:"60",holdTermMs:"200"});
    const view = draft.view({connected:true,selectedDeviceId:"board"});
    assert.deepEqual(new Set(view.changes.map(c=>c.area)),new Set(["Layout","Macros","Defaults","Behaviours","RGB","Combos"]));
    assert.match(view.changes.find(c=>c.area==="Combos").after,/Cmd\+N/);
    assert.equal(JSON.stringify(snapshot),original);
    assert.equal(draft.base.fingerprint,snapshot.fingerprint);
    assert.equal(validateSnapshot(draft.document).settings.values[1],175);
    assert.equal(draft.combos().rows[0].termMs,60);
});
test("undo, redo, branching and no-op edits maintain one bounded history", () => {
    const {draft,snapshot} = fixture();
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"one"});
    const first = draft.current.fingerprint;
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"two"});
    draft.undo(draft.revision); assert.equal(draft.current.fingerprint,first);
    draft.undo(draft.revision); assert.equal(draft.dirty,false); assert.equal(draft.current.fingerprint,snapshot.fingerprint);
    draft.redo(draft.revision); assert.equal(draft.current.fingerprint,first);
    const revision = draft.revision;
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"one"}); assert.equal(draft.revision,revision);
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"branch"}); assert.equal(draft.view({}).canRedo,false);
    for (let i=0;i<105;i++) stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:String(i)});
    assert.equal(draft.history.length,101); assert.equal(draft.base.fingerprint,snapshot.fingerprint);
});
test("invalid edits and obsolete revisions cannot partially change the draft", () => {
    const {draft,snapshot} = fixture();
    assert.throws(()=>stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 0",changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"NOT_A_KEY"}]}]}));
    assert.equal(draft.current.fingerprint,snapshot.fingerprint);
    assert.throws(()=>draft.stage({type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"text",draftRevision:0}),/draft changed/);
    assert.equal(draft.history.length,1);
});
test("external reads retain the draft and require an explicit new comparison", () => {
    const {draft,snapshot,caps} = fixture();
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"mine"});
    const target = draft.current.fingerprint, external = new ProfileDraftSession(snapshot,"board",caps);
    stage(external,settings(external,"keyTiming",{tapHoldTerm:"190"}));
    draft.observe(external.current,"different-board"); assert.equal(draft.stale,false);
    draft.observe(external.current,"board"); assert.equal(draft.stale,true);
    assert.throws(()=>draft.review(draft.revision),/keyboard changed/);
    assert.equal(draft.current.fingerprint,target);
    draft.rebase(draft.revision);
    assert.equal(draft.stale,false); assert.equal(draft.base.fingerprint,external.current.fingerprint);
    assert.equal(draft.current.fingerprint,target);
    assert.ok(draft.view({}).changes.some(c=>c.area==="Defaults" && c.before==="190" && c.after==="150"),"review reveals changes the draft would overwrite");
});
test("apply requires the reviewed revision and original device, uses one verified restore, then clears history", async () => {
    const {draft,snapshot} = fixture(), calls=[];
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"text"});
    const saveRecovery=()=>"recovery", service={snapshot:()=>({connected:true,selectedDeviceId:"board"}),restorePortableProfile:async(document,options)=>{calls.push({document,options});return {...draft.current};}};
    await assert.rejects(draft.apply(service,draft.revision,saveRecovery),/Review/);
    draft.review(draft.revision);
    await assert.rejects(draft.apply({...service,snapshot:()=>({connected:true,selectedDeviceId:"other"})},draft.revision,saveRecovery),/Reconnect/);
    await draft.apply(service,draft.revision,saveRecovery);
    assert.equal(calls.length,1); assert.equal(calls[0].options.expectedFingerprint,snapshot.fingerprint); assert.equal(calls[0].options.saveRecovery,saveRecovery);
    assert.equal(draft.dirty,false); assert.equal(draft.cursor,0);
});
test("interrupted apply retains the full target and can review against an incomplete recovery capture", async () => {
    const {draft} = fixture(); stage(draft,{type:"updateViaMacro",keycode:"MACRO_0",payload:"retained"}); draft.review(draft.revision);
    const target = draft.current.fingerprint;
    const service={snapshot:()=>({connected:true,selectedDeviceId:"board"}),restorePortableProfile:async()=>{throw Object.assign(Error("interrupted"),{code:"RESTORE_INCOMPLETE"});}};
    await assert.rejects(draft.apply(service,draft.revision,()=>"recovery"),/interrupted/);
    assert.equal(draft.current.fingerprint,target); assert.equal(draft.stale,true);
    draft.observe({incomplete:true,fingerprint:"incomplete:device",document:{format:"charybdis-recovery-capture"}},"board");
    draft.rebase(draft.revision);
    assert.equal(draft.base.fingerprint,"incomplete:device"); assert.equal(draft.current.fingerprint,target);
    assert.match(draft.view({}).changes[0].before,/Interrupted configuration/);
    await draft.apply({...service,restorePortableProfile:async(document,options)=>{assert.equal(options.expectedFingerprint,"incomplete:device");return draft.current;}},draft.revision,()=>"recovery");
    assert.equal(draft.dirty,false);
});
test("layer reordering is undoable and updates layout and settings references together", () => {
    const {draft,snapshot} = fixture();
    draft.reorder([0,2,1,3,4,5,6,7],["Base","Symbols","Numbers","Navigation","Pointer","Extra 1","Extra 2","Extra 3"],draft.revision);
    assert.equal(draft.current.summary.names[1],"Symbols");
    assert.equal(draft.document.layers[0][0],0x5222);
    draft.undo(draft.revision); assert.equal(draft.current.fingerprint,snapshot.fingerprint);
});
