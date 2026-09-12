"use strict";
const test = require("node:test"), assert = require("node:assert/strict"), vm = require("node:vm");
const {renderProfileDraft} = require("../../webview/profile-draft-ui");
const {renderDeviceProfileDetails} = require("../../webview/device-profile-ui");
const {renderDeviceCombos} = require("../../webview/combo-ui");
const {getStudioHtml} = require("../../webview/studio-ui");
class Node {
    constructor(tag) {this.tag=tag;this.children=[];this.style={};this.attributes={};this.textContent="";}
    set innerHTML(value) {throw Error("Profile data must remain text");}
    append(...children) {this.children.push(...children);}
    replaceChildren(...children) {this.children=children;}
    setAttribute(key,value) {this.attributes[key]=value;}
    all(tag) {return this.children.flatMap(child=>[...(child.tag===tag?[child]:[]),...child.all(tag)]);}
}
function fixture(overrides={}) {
    const host=new Node("main"), messages=[];
    const document={createElement:tag=>new Node(tag)};
    const model={portable:{available:true},draft:{revision:4,connected:true,dirty:true,canUndo:true,reviewed:true,changes:[{area:"Layers",label:"Name",before:"Base",after:"<img src=x>"}],...overrides}};
    renderProfileDraft(document,host,model,message=>messages.push(message));
    return {host,messages,button:label=>host.all("button").find(n=>n.textContent===label)};
}
test("review uses text, includes both values and submits the reviewed revision",()=>{
    const f=fixture(); assert.equal(f.host.all("img").length,0);
    assert.deepEqual(f.host.all("td").map(n=>n.textContent),["Name","Base","<img src=x>"]);
    assert.equal(f.host.all("strong")[0].textContent,"1 profile change");
    f.button("Apply to keyboard").onclick(); assert.deepEqual(f.messages,[{type:"applyProfileDraft",draftRevision:4}]);
});
test("stale and disconnected drafts cannot apply; a different connected board can replace the draft",()=>{
    const stale=fixture({stale:true}); assert.equal(stale.button("Apply to keyboard").disabled,true);
    assert.equal(stale.button("Review against keyboard").disabled,false);
    const disconnected=fixture({connected:false}); assert.equal(disconnected.button("Apply to keyboard").disabled,true);
    assert.equal(disconnected.button("Discard draft").disabled,false);
    assert.ok(fixture({busy:true}).host.all("button").every(n=>n.disabled));
});
test("RGB stage and combo timing forms participate in shared dirty tracking",()=>{
    const nodes={deviceRgbStages:new Node("section"),deviceCombos:new Node("section")};
    const document={createElement:tag=>new Node(tag),getElementById:id=>nodes[id]};
    const model={draft:{},rgb:{stages:[{label:"Layers",bit:1,enabled:true}]},comboReadback:{state:"read",writable:true,enabled:true},combos:[{badge:"Combo 1",id:0,holdTermMs:200,inputDisplays:[],inputs:[]}]};
    renderDeviceProfileDetails(document,model);renderDeviceCombos(document,model);
    for (const node of Object.values(nodes)) {
        assert.ok(node.all("div").some(n=>"data-dirty-section" in n.attributes));
        assert.ok(node.all("button").some(n=>"data-dirty-button" in n.attributes));
    }
});
test("pass-through colour previews are resolved before the clean form baseline",()=>{
    const html=getStudioHtml(), script=html.match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const section={dataset:{}}, picker={value:"#000000"}, context=vm.createContext({document:{querySelectorAll:selector=>selector==="[data-color-control]"?[picker]:[section]},updateColorControl:control=>{control.value="#ff0000";},dirtySnapshot:()=>picker.value,updateDirtySection:()=>{},refreshDirtyTabIndicators:()=>{}});
    vm.runInContext(script.slice(script.indexOf("    function initializeDirtyTracking("),script.indexOf("    function updateDirtyFromEvent(")),context);
    context.initializeDirtyTracking(); assert.equal(section.dataset.dirtyBaseline,"#ff0000");
});
