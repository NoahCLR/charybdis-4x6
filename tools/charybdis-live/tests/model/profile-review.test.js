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
    assert.deepEqual(profileReview(before,after),[{area:"Layers",label:"Layer 4 name",before:"Pointer",after:"Mouse"}]);
});
test("review preserves untrusted names as text data", () => {
    const before=snapshot(document());
    const names=["<img src=x>","Numbers","Symbols","Navigation","Pointer","Extra 1","Extra 2","Extra 3"];
    const after=snapshot(reorderLayers(before.document,[0,1,2,3,4,5,6,7],names));
    assert.equal(profileReview(before,after)[0].after,names[0]);
});
