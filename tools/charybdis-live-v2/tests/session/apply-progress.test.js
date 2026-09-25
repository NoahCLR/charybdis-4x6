"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {APPLY_STEPS, ApplyProgress, failureReason} = require("../../core/session/apply-progress");

test("an Apply moves forward through its steps and never back", () => {
    const views = [];
    const progress = new ApplyProgress(view => views.push(view));
    progress.report("check");
    progress.report("upload", {completed: 20, total: 120});
    progress.report("upload", {completed: 60, total: 120});
    progress.report("validate");
    progress.report("upload", {completed: 120, total: 120}); // a late poll of an earlier phase
    const view = views.at(-1);
    assert.equal(view.current, "validate", "an earlier step reported late does not move it back");
    assert.equal(view.state, "applying");
    assert.deepEqual(view.steps.map(step => step.state).slice(0, 5), ["done", "done", "done", "active", "pending"]);
    assert.equal(view.bytes, null, "a new step starts without the previous step's byte count");
    progress.report("peer", {detail: "Sending", completed: 30, total: 900});
    assert.deepEqual(views.at(-1).bytes, {completed: 30, total: 900});
    assert.equal(views.at(-1).detail, "Sending");
    assert.throws(() => progress.report("nonsense"), /Unknown Apply step/);
});

test("a failure is pinned to the running step, and nothing moves after it", () => {
    const progress = new ApplyProgress();
    progress.report("peer");
    assert.equal(progress.beforeDecision, true, "the peer copy comes before the commit decision");
    const failure = progress.fail({reason: "The other half kept answering that it was busy.", saved: "none"});
    assert.deepEqual(failure, {step: "peer", label: "Copy the profile to the other half", reason: "The other half kept answering that it was busy.", saved: "none"});
    progress.report("commit");
    const view = progress.view();
    assert.equal(view.state, "failed");
    assert.equal(view.steps.find(step => step.id === "peer").state, "failed");
    assert.equal(view.steps.find(step => step.id === "stage").state, "done");
    assert.equal(view.steps.find(step => step.id === "commit").state, "pending");
    progress.report("commit");
    assert.equal(progress.beforeDecision, true);
});

test("a finished Apply shows every step done", () => {
    const progress = new ApplyProgress();
    progress.report("check");
    progress.finish();
    assert.equal(progress.view().state, "done");
    assert.ok(progress.view().steps.every(step => step.state === "done"));
    assert.equal(APPLY_STEPS.length, 10);
    assert.notEqual(new ApplyProgress().view().id, progress.view().id, "each Apply is told apart from the next");
});

test("the reason is the keyboard's own when it gave one, else the other half's last answer", () => {
    assert.equal(failureReason({deviceError: {name: "VALIDATION_REJECTED", domainId: 0x20, rowIndex: 4}}),
        "The keyboard rejected the profile in key behaviours, row 5.");
    assert.equal(failureReason({status: {error: {name: "STORAGE_FAILURE"}}}), "The keyboard could not write its storage.");
    assert.equal(failureReason({deviceError: {name: "SOMETHING_NEW"}}), "The keyboard reported SOMETHING_NEW.");
    assert.equal(failureReason({message: "made no observable progress"}, {lastStatusName: "BUSY"}), "The other half kept answering that it was busy.");
    assert.equal(failureReason({message: "made no observable progress"}, {lastStatusName: "OK", transportFailureCount: 3}), "The link between the halves stopped answering.");
    assert.equal(failureReason({message: "disconnected"}), "disconnected");
});

test("a copy that stopped on the other half names its answer", () => {
    assert.equal(failureReason({deviceError: {name: "PEER_TRANSFER_FAILED"}}, {lastStatusName: "STORAGE_ERROR"}), "The other half could not store the profile, even after trying again.");
    assert.equal(failureReason({deviceError: {name: "PEER_TRANSFER_FAILED"}}, null), "The other half could not take the profile.");
});

test("a copy the other half keeps refusing says what it is waiting on", () => {
    const stalled = {message: "made no observable progress"};
    assert.equal(failureReason(stalled, {lastStatusName: "OK", busyStreak: 40, busyReason: "OTHER_COPY"}), "The other half was still holding an earlier, unfinished copy.");
    assert.equal(failureReason(stalled, {lastStatusName: "OK", busyStreak: 40, busyReason: "MAILBOX_FULL"}), "The other half stopped handling requests from this half.");
    assert.equal(failureReason(stalled, {lastStatusName: "BUSY", busyStreak: 1, busyReason: "OTHER_COPY"}), "The other half kept answering that it was busy.", "one BUSY is routine");
    const {peerReport} = require("../../core/session/portable-profile-session");
    assert.equal(peerReport({phaseName: "BEGINNING", transferOffset: 0, transferLength: 2520, busyStreak: 12, busyReason: "OTHER_COPY"}).detail, "Starting the copy · the other half still holds an earlier copy");
    assert.equal(peerReport({phaseName: "SENDING", transferOffset: 28, transferLength: 2520, busyStreak: 1, busyReason: "ADMITTED"}).detail, "Sending");
    assert.match(peerReport({phaseName: "PREPARED", waitingSafeBoundary: true}).detail, /Release held keys/);
    assert.match(failureReason({deviceError: {name: "TIMEOUT"}}, {waitingSafeBoundary: true}), /stayed locked.*Nothing changed/);
    assert.equal(failureReason({deviceError: {name: "TIMEOUT"}}, {waitingSafeBoundary: false}), "The keyboard gave up waiting for this save.");
});
