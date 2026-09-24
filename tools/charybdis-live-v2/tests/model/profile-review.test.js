"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {profileReview} = require("../../core/model/profile-review");
const {document} = require("../fixtures/portable-profile");
const {fingerprint,reorderLayers} = require("../../core/model/portable-profile");
const snapshot = document => ({document,fingerprint:fingerprint(document)});
test("unchanged snapshots have no review entries and layer renaming does not invent policy changes", () => {
    const before=snapshot(document()); assert.deepEqual(profileReview(before,before),[]);
    const names=["Base","Numbers","Symbols","Navigation","Mouse","Extra 1","Extra 2","Extra 3"];
    const after=snapshot(reorderLayers(before.document,[0,1,2,3,4,5,6,7],names));
    assert.deepEqual(profileReview(before,after),[{area:"Layers",unit:"layerName:4",title:"Layer 4",status:"changed",
        fields:[{label:"Name",status:"changed",before:"Pointer",after:"Mouse"}],place:{kind:"layers"},titleMark:{kind:"layer",layer:4}}]);
});
test("review preserves untrusted names as text data", () => {
    const before=snapshot(document());
    const names=["<img src=x>","Numbers","Symbols","Navigation","Pointer","Extra 1","Extra 2","Extra 3"];
    const after=snapshot(reorderLayers(before.document,[0,1,2,3,4,5,6,7],names));
    assert.equal(profileReview(before,after)[0].fields[0].after,names[0]);
});
test("a key changed for another with the same label is still a visible change", () => {
    const before=snapshot(document()), changed=structuredClone(before.document);
    changed.layers[1][7]=0x1e; const base=snapshot(changed);
    const after=structuredClone(changed); after.layers[1][7]=0x59; // KC_1 → KC_KP_1, both labelled "1"
    const rows=profileReview(base,snapshot(after));
    assert.equal(rows.length,1);
    assert.equal(rows[0].area,"Layout");
    const [field]=rows[0].fields;
    assert.notEqual(field.before,field.after);
    assert.match(field.before,/KC_1\b/); assert.match(field.after,/KC_KP_1/);
});

// ── items ───────────────────────────────────────────────────────────────

const {document:pdDocument} = require("../fixtures/pd-profile");
const {validateSnapshot} = require("../../core/model/portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {decodePdDomain, encodePdDomain} = require("../../core/schema/pd-mode-domain-v1");
// A copy of a document with one profile domain rewritten.
function withDomain(value, id, decode, encode, change) {
    const blob=decodeProfileBlob(Buffer.from(value.profile,"base64")), domain=blob.domains.find(row=>row.id===id);
    domain.payload=encode(change(decode(domain.payload)));
    return {...value,profile:encodeProfileBlob(blob).toString("base64")};
}
const behaviours = (value, change) => withDomain(value,32,payload=>decodeKeyBehaviorDomain(payload,{actionLimits:{maxPdModes:8}}).rows,rows=>encodeKeyBehaviorDomain({rows},{actionLimits:{maxPdModes:8}}),change);
test("a behaviour lists only the fields that changed, in the editor's words", () => {
    const base=pdDocument(), before=snapshot(base);
    const target=validateSnapshot(base).behaviors.rows[0].target;
    const after=snapshot(behaviours(base,rows=>rows.map(row=>JSON.stringify(row.target)===JSON.stringify(target)?{...row,tapHoldTerm:180}:row)));
    const [item]=profileReview(before,after);
    assert.equal(item.status,"changed");
    assert.deepEqual(item.fields,[{label:"Tap / hold",status:"changed",before:`default · ${validateSnapshot(base).settings.values[1]} ms`,after:"180 ms",
        labelMark:{kind:"tier",tier:"hold"}}], "the tiers that did not change are not repeated, and the timing is marked with the tier it decides");
});
test("a removed behaviour is marked removed and lists what it held, without its defaults", () => {
    const base=pdDocument(), before=snapshot(base), row=validateSnapshot(base).behaviors.rows[0];
    const [item]=profileReview(before,snapshot(behaviours(base,rows=>rows.slice(1))));
    assert.equal(item.status,"removed");
    assert.ok(item.fields.length>0 && item.fields.every(field=>field.after===null));
    assert.ok(item.fields.every(field=>!/^default/.test(field.before)),"defaults restate nothing");
    assert.ok(item.fields.some(field=>/^\d× (tap|hold|long hold)$/.test(field.label)),"tiers are named as the grid names them");
    void row;
});
test("a new pointing mode is one added item, however many fields it sets", () => {
    const base=pdDocument(), before=snapshot(base), slots=validateSnapshot(base).pdModes;
    const after=snapshot(withDomain(base,80,decodePdDomain,encodePdDomain,modes=>modes.map((mode,id)=>id===6?{...slots[0],id:6,name:"Mode 7"}:mode)));
    const items=profileReview(before,after);
    assert.equal(items.length,1);
    assert.equal(items[0].status,"added");
    assert.equal(items[0].title,"Slot 7 · Mode 7");
    assert.ok(items[0].fields.length>5,"it lists what it sets");
    assert.deepEqual(items[0].place,{kind:"pointing",slot:6});
});
test("changing a default in Settings is one settings item, not a change to every behaviour that uses it", () => {
    const base=pdDocument(), before=snapshot(base);
    const after=snapshot(withDomain(base,64,payload=>require("../../core/schema/settings-domain-v1").decodeSettings(payload),
        value=>require("../../core/schema/settings-domain-v1").encodeSettings(value),settings=>({...settings,values:settings.values.map((v,i)=>i===1?175:v)})));
    const items=profileReview(before,after);
    assert.deepEqual(items.map(entry=>entry.unit),["settings:keyTiming"]);
    assert.deepEqual(items[0].fields.map(field=>[field.before,field.after]),[[String(validateSnapshot(base).settings.values[1]),"175"]]);
});
test("renaming a layer does not read as a change to the settings that name it", () => {
    const before=snapshot(pdDocument());
    const names=validateSnapshot(before.document).settings.names.map((name,i)=>i===4?"Mouse":name);
    const units=profileReview(before,snapshot(reorderLayers(before.document,[0,1,2,3,4,5,6,7],names))).map(entry=>entry.unit);
    assert.deepEqual(units,["layerName:4"]);
});
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../../core/schema/rgb-domain-v1");
const lighting = (value, change) => withDomain(value,16,payload=>decodeRgbDomainV1(payload),rgb=>encodeRgbDomainV1(rgb),change);
test("a colour travels as a colour, and an off one says what off means there", () => {
    const base=pdDocument(), before=snapshot(base);
    const after=snapshot(lighting(base,rgb=>({...rgb,
        layerColors:rgb.layerColors.map(row=>row.layerId===2?{...row,color:{h:20,s:200,v:100}}:row),
        groups:[...rgb.groups,{id:rgb.groups.length,leds:[1,2,3]}],
        layerGroupRows:[...rgb.layerGroupRows,{selector:3,color:{h:0,s:0,v:0},groupId:rgb.groups.length}]})));
    const items=profileReview(before,after), names=validateSnapshot(base).settings.names;
    const layer=items.find(entry=>entry.unit==="rgb:layer:2");
    assert.equal(layer.title,`${names[2]} colour`,"a layer's colour is named by the layer");
    assert.deepEqual(layer.fields[0].afterColour,{h:20,s:200,v:100});
    assert.ok(layer.fields[0].beforeColour,"both sides carry their colour");
    const override=items.find(entry=>entry.unit==="rgb:groups").fields.find(field=>field.label===`Override · ${names[3]}`);
    assert.match(override.after,/Group \d+ · 3 LEDs · inherits the stage colour/);
    assert.deepEqual(override.afterColour,{h:0,s:0,v:0},"drawn as off, as Lighting draws it");
});
test("switching one lighting stage off names that stage, in the Lighting screen's words", () => {
    const base=pdDocument(), before=snapshot(base);
    const [item]=profileReview(before,snapshot(lighting(base,rgb=>({...rgb,stageEnableMask:rgb.stageEnableMask & ~4}))));
    assert.deepEqual(item.fields,[{label:"Pointing modes",status:"changed",before:"on",after:"off",beforeMark:{kind:"stage",on:true},afterMark:{kind:"stage",on:false}}]);
});
test("a behaviour tier says which branch and tier it is, so it is coloured as the grid colours it", () => {
    const base=pdDocument(), before=snapshot(base);
    const row=validateSnapshot(base).behaviors.rows.find(entry=>entry.steps.some(step=>step.hold));
    const [item]=profileReview(before,snapshot(behaviours(base,rows=>rows.filter(entry=>entry!==row && JSON.stringify(entry.target)!==JSON.stringify(row.target)))));
    const hold=item.fields.find(field=>/× hold$/.test(field.label));
    assert.deepEqual(hold.labelMark,{kind:"tier",tier:"hold",branch:Number(hold.label[0])});
});
test("a pointing-mode action reads by its slot's name, and carries the slot for its colour", () => {
    const base=pdDocument(), before=snapshot(base), slots=validateSnapshot(base).pdModes;
    const target=validateSnapshot(base).behaviors.rows[0].target;
    const after=snapshot(behaviours(base,rows=>rows.map(entry=>JSON.stringify(entry.target)===JSON.stringify(target)
        ?{...entry,steps:[{tapIndex:0,tap:{kind:4,flags:0,operand:0}}]}:entry)));
    const field=profileReview(before,after)[0].fields.find(entry=>entry.label==="1× tap");
    assert.equal(field.after,slots[0].name,"named as the Pointing modes screen names it");
    assert.deepEqual(field.afterMark,{kind:"pointing",slot:0});
});
test("a setting is marked with what it governs, as the Settings screen marks it", () => {
    const base=pdDocument(), before=snapshot(base);
    const settings=require("../../core/schema/settings-domain-v1");
    const after=snapshot(withDomain(base,64,payload=>settings.decodeSettings(payload),value=>settings.encodeSettings(value),
        value=>({...value,values:value.values.map((v,i)=>i===1?175:i===2?450:i===5?3:v)})));
    const items=profileReview(before,after);
    const timing=items.find(entry=>entry.unit==="settings:keyTiming").fields;
    assert.deepEqual(timing.map(field=>field.labelMark),[{kind:"tier",tier:"hold"},{kind:"tier",tier:"long"}]);
    const layer=items.find(entry=>entry.unit==="settings:autoMouse").fields.find(field=>field.label==="Auto-mouse layer");
    assert.deepEqual(layer.afterMark,{kind:"layer",layer:3},"a layer value carries the layer's colour");
});
test("clearing a pointing mode is a change to the slot, not to every behaviour that reaches it", () => {
    const base=pdDocument(), slots=validateSnapshot(base).pdModes;
    const target=validateSnapshot(base).behaviors.rows[0].target;
    const reaching=snapshot(behaviours(base,rows=>rows.map(entry=>JSON.stringify(entry.target)===JSON.stringify(target)
        ?{...entry,steps:[{tapIndex:0,tap:{kind:5,flags:0,operand:4}}]}:entry)));
    const cleared=snapshot(withDomain(reaching.document,80,decodePdDomain,encodePdDomain,modes=>modes.map((mode,id)=>id===4?{...slots[6],id:4}:mode)));
    assert.deepEqual(profileReview(reaching,cleared).map(entry=>entry.unit),["pd:4"]);
});
test("a tier removed from a behaviour that stays is a removed field of a changed item, not a value called none", () => {
    const base=pdDocument(), before=snapshot(base);
    const row=validateSnapshot(base).behaviors.rows.find(entry=>entry.steps.some(step=>step.hold && step.tap));
    const after=snapshot(behaviours(base,rows=>rows.map(entry=>JSON.stringify(entry.target)===JSON.stringify(row.target)
        ?{...entry,steps:entry.steps.map(step=>step.hold && step.tap?{tapIndex:step.tapIndex,tap:step.tap}:step)}:entry)));
    const [item]=profileReview(before,after);
    assert.equal(item.status,"changed","the behaviour is still there");
    const hold=item.fields.find(field=>/× hold$/.test(field.label));
    assert.equal(hold.status,"removed");
    assert.equal(hold.after,null,"the draft side is absent, not the word none");
    assert.ok(hold.before);
});
test("an eight-direction mode reads as eight directions, and lists its diagonals", () => {
    const base=pdDocument(), before=snapshot(base), slots=validateSnapshot(base).pdModes;
    const directional=slots.findIndex(slot=>slot.kind===1);
    const after=snapshot(withDomain(base,80,decodePdDomain,encodePdDomain,modes=>modes.map((mode,id)=>id===directional?{...mode,axis:3,thresholdX:mode.thresholdX||60,thresholdY:mode.thresholdY||60}:mode)));
    const [item]=profileReview(before,after);
    const axis=item.fields.find(field=>field.label==="Axes");
    assert.equal(axis.after,"Eight directions","not undefined");
    assert.ok(item.fields.some(field=>field.label==="Up-left"),"the diagonals it now reads are listed");
});
