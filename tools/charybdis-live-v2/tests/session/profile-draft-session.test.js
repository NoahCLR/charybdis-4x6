"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {ProfileDraftSession} = require("../../core/session/profile-draft-session");
const {document} = require("../fixtures/portable-profile");
const {options} = require("../fixtures/keyboard-options");
const {fingerprint, reorderLayers, summary, validateSnapshot} = require("../../core/model/portable-profile");
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
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_1",payload:"{KC_LGUI,KC_N}"});
    stage(draft,settings(draft,"keyTiming",{tapHoldTerm:"175"}));
    const row = behaviorRowsForView(validateSnapshot(draft.document).behaviors)[0];
    stage(draft,{type:"saveBehavior",behavior:{...row,tapHoldTerm:"210"},expectedBase:draft.identity()});
    stage(draft,{type:"updateLayerColor",layer:"Layer 0",h:"23",s:"255",v:"100",mode:"KEYS_MAPPED_ON_THIS_LAYER_ONLY"});
    stage(draft,{type:"saveCombo",id:0,inputs:["KC_A","KC_B"],output:"G(KC_N)",termMs:"60",holdTermMs:"200"});
    const view = draft.view({connected:true,selectedDeviceId:"board"});
    assert.deepEqual(new Set(view.changes.map(c=>c.area)),new Set(["Layout","Macros","Settings","Behaviours","Lighting","Combos"]));
    assert.match(view.changes.find(c=>c.area==="Combos").fields.find(f=>f.label==="Sends").after,/Cmd\+N/);
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
    assert.ok(draft.view({}).changes.some(c=>c.area==="Settings" && c.fields.some(f=>f.before==="190" && f.after==="150")),"review reveals changes the draft would overwrite");
});
test("a reconnect requires review even when the HID path and profile are unchanged", async () => {
    const {snapshot, caps} = fixture();
    const draft = new ProfileDraftSession(snapshot, "board", caps, 1);
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"mine"});
    draft.observe(snapshot, "board", 2);
    assert.equal(draft.connectionChanged, true);
    assert.equal(draft.stale, true);
    assert.throws(() => draft.review(draft.revision), /keyboard changed/);
    draft.rebase(draft.revision);
    assert.equal(draft.connectionToken, 2);
    assert.equal(draft.stale, false);
    assert.equal(draft.dirty, true);
    const service = {snapshot: () => ({connected: true, selectedDeviceId: "board", connectionToken: 3})};
    await assert.rejects(draft.apply(service, draft.revision, () => {}), /Review this draft/);
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
    const {draft} = fixture(); stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_2",payload:"retained"}); draft.review(draft.revision);
    const target = draft.current.fingerprint;
    const service={snapshot:()=>({connected:true,selectedDeviceId:"board"}),restorePortableProfile:async()=>{throw Object.assign(Error("interrupted"),{code:"RESTORE_INCOMPLETE"});}};
    await assert.rejects(draft.apply(service,draft.revision,()=>"recovery"),/interrupted/);
    assert.equal(draft.current.fingerprint,target); assert.equal(draft.stale,true);
    draft.observe({incomplete:true,fingerprint:"incomplete:device",document:{format:"charybdis-recovery-capture"}},"board");
    draft.rebase(draft.revision);
    assert.equal(draft.base.fingerprint,"incomplete:device"); assert.equal(draft.current.fingerprint,target);
    assert.match(draft.view({}).changes[0].fields[0].before,/Interrupted configuration/);
    await draft.apply({...service,restorePortableProfile:async(document,options)=>{assert.equal(options.expectedFingerprint,"incomplete:device");return draft.current;}},draft.revision,()=>"recovery");
    assert.equal(draft.dirty,false);
});
test("layer reordering is undoable and updates layout and settings references together", () => {
    const {draft,snapshot} = fixture();
    draft.replace(reorderLayers(draft.document,[0,2,1,3,4,5,6,7],["Base","Symbols","Numbers","Navigation","Pointer","Extra 1","Extra 2","Extra 3"]),draft.revision);
    assert.equal(draft.current.summary.names[1],"Symbols");
    assert.equal(draft.document.layers[0][0],0x5222);
    draft.undo(draft.revision); assert.equal(draft.current.fingerprint,snapshot.fingerprint);
});

// ── discarding from the review ──────────────────────────────────────────

const key = (layoutIndex, keycode, layer = "Layer 0") => ({type:"updateLayoutKeys",layers:[{layer,changes:[{layoutIndex,keycode}]}]});
const groupOf = (draft, unit) => draft.changes().find(row => row.unit === unit)?.group;
const unitsOf = (draft, group) => draft.changes().filter(row => row.group === group).map(row => row.unit).sort();
test("rows are grouped by the edit that made them, and by any later edit that joins them", () => {
    const {draft} = fixture();
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"KC_B"}]}]});
    stage(draft,key(2,"KC_C","Layer 1"));
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"alone"});
    assert.deepEqual(unitsOf(draft,groupOf(draft,"layout:1:0")),["layout:1:0","layout:1:1"],"one message is one group");
    assert.notEqual(groupOf(draft,"layout:1:2"),groupOf(draft,"layout:1:0"),"a separate edit stays apart");
    assert.notEqual(groupOf(draft,"macro:0"),groupOf(draft,"layout:1:0"));
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:1,keycode:"KC_D"},{layoutIndex:2,keycode:"KC_E"}]}]});
    assert.deepEqual(unitsOf(draft,groupOf(draft,"layout:1:0")),["layout:1:0","layout:1:1","layout:1:2"],"a later edit touching both joins them");
});
test("a unit edited back to the keyboard's value no longer ties anything together", () => {
    const {draft,snapshot} = fixture();
    const original = snapshot.document.layers[1][1];
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"KC_B"}]}]});
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:1,keycode:"KC_Z"},{layoutIndex:2,keycode:"KC_C"}]}]});
    assert.equal(unitsOf(draft,groupOf(draft,"layout:1:0")).length,3);
    draft.replace({...draft.document,layers:draft.document.layers.map((keys,l)=>l===1?keys.map((code,p)=>p===1?original:code):keys)},draft.revision);
    assert.equal(groupOf(draft,"layout:1:1"),undefined,"the key is back");
    assert.notEqual(groupOf(draft,"layout:1:0"),groupOf(draft,"layout:1:2"),"and the keys it linked are apart again");
});
test("discarding a group puts every row of it back, as one undoable step, and leaves the rest", () => {
    const {draft,snapshot} = fixture();
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"KC_B"}]}]});
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"kept"});
    const kept = draft.document.macros[0];
    draft.discard(draft.revision,groupOf(draft,"layout:1:1"));
    assert.deepEqual(draft.changes().map(row => row.unit),["macro:0"]);
    assert.deepEqual(draft.document.layers,snapshot.document.layers);
    assert.equal(draft.document.macros[0],kept);
    draft.undo(draft.revision);
    assert.deepEqual(draft.changes().map(row => row.unit).sort(),["layout:1:0","layout:1:1","macro:0"],"undo brings the group back");
    assert.throws(()=>draft.discard(draft.revision,99),/no longer in the draft/);
    assert.throws(()=>draft.discard(draft.revision - 1,0),/draft changed/);
});
test("a discard from a current review keeps it current, and discarding the last change leaves a clean draft", () => {
    const {draft} = fixture();
    stage(draft,key(0,"KC_A")); stage(draft,key(1,"KC_B"));
    draft.review(draft.revision);
    draft.discard(draft.revision,groupOf(draft,"layout:0:0"));
    assert.equal(draft.view({}).reviewed,true,"what is left was part of what was reviewed");
    draft.discard(draft.revision,groupOf(draft,"layout:0:1"));
    assert.equal(draft.dirty,false);
    stage(draft,key(0,"KC_A"));
    draft.discard(draft.revision,groupOf(draft,"layout:0:0"));
    assert.equal(draft.view({}).reviewed,false,"an unreviewed draft is not reviewed by a discard");
});
test("a macro name discarded back leaves no indescribable settings difference", () => {
    const value = require("../fixtures/pd-profile").document();
    const draft = new ProfileDraftSession({document:value,fingerprint:fingerprint(value),summary:summary(value),limits:{brightnessMax:200}}, "board", {compiledLayerCount:8,supportedDomainMask:31,actionAbiDigest:value.actionAbiDigest});
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_3",name:"Hello",expectedFingerprint:draft.current.fingerprint});
    draft.discard(draft.revision,groupOf(draft,"macro:3"));
    assert.equal(draft.dirty,false,"the settings format the name upgraded goes back too");
});
test("a rebase does not tie together everything it carried over", () => {
    const {draft,snapshot,caps} = fixture();
    stage(draft,key(0,"KC_A")); stage(draft,key(1,"KC_B"));
    const external = new ProfileDraftSession(snapshot,"board",caps);
    stage(external,{type:"updateViaMacro",keycode:"VIA_MACRO_9",payload:"elsewhere"});
    draft.observe(external.current,"board"); draft.rebase(draft.revision);
    assert.notEqual(groupOf(draft,"layout:0:0"),groupOf(draft,"layout:0:1"));
});
test("discarding the whole draft is a step undo takes back, and undo says what it undoes", () => {
    const {draft,snapshot} = fixture();
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 1",changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"KC_B"}]}]});
    stage(draft,{type:"updateViaMacro",keycode:"VIA_MACRO_0",payload:"kept"});
    assert.equal(draft.view({}).undoLabel,"Edited a macro");
    const target = draft.current.fingerprint;
    draft.discardAll(draft.revision);
    assert.equal(draft.dirty,false); assert.equal(draft.current.fingerprint,snapshot.fingerprint);
    assert.equal(draft.view({}).undoLabel,"Discarded the draft (3 changes)");
    draft.undo(draft.revision);
    assert.equal(draft.current.fingerprint,target,"undo brings every change back");
    assert.equal(draft.view({}).redoLabel,"Discarded the draft (3 changes)");
    draft.discard(draft.revision,groupOf(draft,"macro:0"));
    assert.match(draft.view({}).undoLabel,/^Discarded Macro 0/);
});
test("a revision is decoded once and its history entry cannot be edited in place", () => {
    const {draft} = fixture();
    stage(draft,{type:"updateLayoutKeys",layers:[{layer:"Layer 0",changes:[{layoutIndex:0,keycode:"KC_A"}]}]});
    assert.equal(draft.current.decoded, draft.current.decoded, "the same decode serves every read of this revision");
    assert.equal(draft.current.document, draft.current.decoded.document, "and belongs to the document it is handed with");
    assert.throws(() => { "use strict"; draft.current.document.layers[0][0] = 5; }, TypeError, "history is frozen");
    const copyOf = {...draft.current, document: structuredClone(draft.current.document)};
    const {decodedOf} = require("../../core/model/portable-profile");
    assert.notEqual(decodedOf(copyOf), draft.current.decoded, "a snapshot with another document is decoded afresh, never with a stale decode");
});

// ── layers compared by identity ─────────────────────────────────────────

const SWAP = [0,1,3,2,4,5,6,7];
// An Edit layers save: the step it rearranges the draft by, every layer
// keeping its name unless `names` (by new slot) says otherwise.
function layers(draft, step = SWAP, {names, keysFollow = true} = {}) {
    const current = draft.current.summary.names;
    draft.editLayers(reorderLayers(draft.document,step,names || step.map(slot => current[slot]),{keysFollow}),draft.revision,step);
}
const units = draft => draft.changes().map(row => row.unit).sort();
test("a layer swap is one item, and nothing it moved is listed on its own", () => {
    const {draft} = fixture();
    layers(draft);
    const rows = draft.changes();
    assert.deepEqual(rows.map(row => row.unit),["layerOrder"]);
    assert.equal(rows[0].title,"Layer priority");
    assert.deepEqual(rows[0].fields.map(field => [field.label,field.before,field.after]),[["Symbols","2","3 · higher"],["Navigation","3","2 · lower"]],
        "each layer that moved, highest first, by where it sat and sits");
    assert.deepEqual(rows[0].fields.map(field => field.labelMark),[{kind:"layer",layer:3},{kind:"layer",layer:2}],"in its light");
    assert.deepEqual(rows[0].place,{kind:"layers",layers:[3,2]});
    assert.equal(draft.view({}).undoLabel,"Swapped Symbols and Navigation");
});
test("a key edited on a swapped layer is its own item, and each discard keeps the other", () => {
    const {draft,snapshot} = fixture();
    layers(draft);
    stage(draft,key(0,"KC_Q","Layer 2"));
    assert.deepEqual(units(draft),["layerOrder","layout:2:0"]);
    assert.equal(draft.changes().find(row => row.unit === "layout:2:0").title,"Navigation · Left · row 1, column 1");
    assert.notEqual(groupOf(draft,"layerOrder"),groupOf(draft,"layout:2:0"));
    draft.discard(draft.revision,groupOf(draft,"layout:2:0"));
    assert.deepEqual(units(draft),["layerOrder"],"the key goes back, the swap stays");
    draft.undo(draft.revision);
    draft.discard(draft.revision,groupOf(draft,"layerOrder"));
    assert.deepEqual(units(draft),["layout:3:0"],"the order goes back, the key stays on Navigation");
    assert.deepEqual(draft.current.summary.names,snapshot.summary.names);
    assert.equal(draft.view({}).undoLabel,"Put the layer order back");
});
test("a swap and a rename are two items, and discarding one keeps the other", () => {
    const {draft,snapshot} = fixture();
    layers(draft,SWAP,{names:["Base","Numbers","Nav","Symbols","Pointer","Extra 1","Extra 2","Extra 3"]});
    assert.deepEqual(units(draft),["layerName:2","layerOrder"]);
    assert.equal(draft.view({}).undoLabel,"Swapped Symbols and Navigation, renamed Navigation to Nav");
    assert.notEqual(groupOf(draft,"layerOrder"),groupOf(draft,"layerName:2"),"one save, two decisions");
    assert.deepEqual(draft.changes().find(row => row.unit === "layerOrder").fields.map(field => field.label),["Symbols","Nav"],"the order names a layer as it is called now");
    const rename = draft.changes().find(row => row.unit === "layerName:2");
    assert.deepEqual([rename.fields[0].before,rename.fields[0].after],["Navigation","Nav"],"named against the layer it renames");
    draft.discard(draft.revision,groupOf(draft,"layerName:2"));
    assert.deepEqual(draft.current.summary.names.slice(2,4),["Navigation","Symbols"],"the name goes back where the layer now sits");
    draft.undo(draft.revision);
    draft.discard(draft.revision,groupOf(draft,"layerOrder"));
    assert.deepEqual(draft.current.summary.names.slice(2,4),["Symbols","Nav"],"the rename moves back with its layer");
    assert.deepEqual(units(draft),["layerName:3"]);
    assert.notDeepEqual(draft.current.summary.names,snapshot.summary.names);
});
test("several swaps are one priority change: the net move of every layer they touched", () => {
    const {draft} = fixture();
    layers(draft); layers(draft,[0,1,2,4,3,5,6,7]);
    const chained = draft.changes().find(row => row.unit === "layerOrder");
    assert.deepEqual(chained.fields.map(field => [field.label,field.before,field.after]),
        [["Symbols","2","4 · higher"],["Pointer","4","3 · lower"],["Navigation","3","2 · lower"]],"two swaps sharing a layer move three");
    layers(draft,[0,1,2,3,4,6,5,7]);
    assert.deepEqual(draft.changes().filter(row => row.unit === "layerOrder").length,1,"a separate swap joins the same item");
    assert.deepEqual(draft.changes()[0].fields.map(field => field.label),["Extra 1","Extra 2","Symbols","Pointer","Navigation"]);
});
test("swapping back leaves no layer order, and names alone never make one", () => {
    const {draft} = fixture();
    layers(draft); layers(draft);
    assert.equal(draft.dirty,false);
    assert.deepEqual(draft.changes(),[]);
    // The contents move, the names are swapped over: on names alone nothing changed.
    layers(draft,SWAP,{names:["Base","Numbers","Symbols","Navigation","Pointer","Extra 1","Extra 2","Extra 3"]});
    assert.deepEqual(units(draft),["layerName:2","layerName:3","layerOrder"]);
});
test("keys that keep their numbers through a reorder are listed as the changes they are", () => {
    const {draft,snapshot} = fixture();
    layers(draft,[0,2,1,3,4,5,6,7],{keysFollow:false});
    assert.equal(draft.document.layers[0][0],snapshot.document.layers[0][0],"the key kept its number");
    assert.ok(units(draft).includes("layerOrder"));
    assert.ok(units(draft).includes("layout:0:0"),"and now reaches another layer, which the review says");
});
test("undo and redo carry the order; a rebase keeps it; an import and a whole discard start from the keyboard's", () => {
    const {draft,snapshot,caps} = fixture();
    layers(draft); stage(draft,key(0,"KC_Q","Layer 2"));
    draft.undo(draft.revision); assert.deepEqual(units(draft),["layerOrder"]);
    draft.undo(draft.revision); assert.deepEqual(units(draft),[]);
    draft.redo(draft.revision); draft.redo(draft.revision); assert.deepEqual(units(draft),["layerOrder","layout:2:0"]);
    const external = new ProfileDraftSession(snapshot,"board",caps);
    stage(external,{type:"updateViaMacro",keycode:"VIA_MACRO_9",payload:"elsewhere"});
    draft.observe(external.current,"board"); draft.rebase(draft.revision);
    assert.deepEqual(units(draft),["layerOrder","layout:2:0","macro:9"]);
    const imported = reorderLayers(snapshot.document,SWAP,SWAP.map(slot => snapshot.summary.names[slot]));
    draft.replace(imported,draft.revision,"edit","Imported a profile",[0,1,2,3,4,5,6,7]);
    assert.deepEqual(units(draft).filter(unit => unit.startsWith("layer")),["layerName:2","layerName:3"],"an import is compared slot by slot");
    draft.undo(draft.revision); draft.discardAll(draft.revision);
    assert.equal(draft.dirty,false); assert.deepEqual(draft.order,[0,1,2,3,4,5,6,7]);
});
test("edits link by the layer they touched, so a reorder between them ties nothing new", () => {
    const {draft} = fixture();
    const both = layer => ({type:"updateLayoutKeys",layers:[{layer,changes:[{layoutIndex:0,keycode:"KC_A"},{layoutIndex:1,keycode:"KC_B"}]}]});
    stage(draft,both("Layer 2"));
    layers(draft);
    stage(draft,both("Layer 2"));
    assert.equal(groupOf(draft,"layout:3:0"),groupOf(draft,"layout:3:1"),"Symbols' two keys, made together, moved together");
    assert.equal(groupOf(draft,"layout:2:0"),groupOf(draft,"layout:2:1"));
    assert.notEqual(groupOf(draft,"layout:3:0"),groupOf(draft,"layout:2:0"),"Navigation's keys are another edit");
});
