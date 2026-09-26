import assert from "node:assert/strict";
import test from "node:test";

import {captureContentScroll, captureKeysBenchHeight, captureNestedScroll, limitBehaviourGroups, restoreContentScroll, restoreKeysBenchHeight, restoreNestedScroll, scrollContentTo} from "../webview/lib/scroll.mjs";

const rootWith = (content) => ({querySelector: (selector) => selector === ".content" ? content : null});

test("rerendering a screen preserves its content scroll position", () => {
    const before = {scrollTop: 840, scrollLeft: 36};
    const scroll = captureContentScroll(rootWith(before), "lighting", "lighting");
    assert.deepEqual(scroll, {top: 840, left: 36});

    const after = {scrollTop: 0, scrollLeft: 0};
    restoreContentScroll(rootWith(after), scroll);
    assert.deepEqual(after, {scrollTop: 840, scrollLeft: 36});
});

test("a taller Keys tab keeps enough page height when another tab is shown", () => {
    const previous = {querySelector: (selector) => selector === ".keys-pad .bench"
        ? {getBoundingClientRect: () => ({height: 820})} : null};
    const height = captureKeysBenchHeight(previous, "keys", "keys");
    const nextBench = {style: {minHeight: ""}};
    restoreKeysBenchHeight({querySelector: () => nextBench}, height);
    assert.equal(nextBench.style.minHeight, "820px");
    assert.equal(captureKeysBenchHeight(previous, "keys", "lighting"), 0,
        "leaving Keys starts the other screen at its own height");
});

test("navigating to another screen starts at the top", () => {
    const content = {scrollTop: 840, scrollLeft: 36};
    assert.equal(captureContentScroll(rootWith(content), "lighting", "keys"), null);
});

test("scroll preservation tolerates screens without a content container", () => {
    const root = rootWith(null);
    assert.equal(captureContentScroll(root, "keys", "keys"), null);
    assert.doesNotThrow(() => restoreContentScroll(root, {top: 10, left: 2}));
});

test("jumping from the combo builder places the board below the content edge", () => {
    const calls = [];
    const board = {getBoundingClientRect: () => ({top: -260})};
    const content = {scrollTop: 900, scrollLeft: 12, getBoundingClientRect: () => ({top: 80}),
        querySelector: (selector) => selector === ".board-card" ? board : null,
        scrollTo: (options) => calls.push(options)};
    scrollContentTo(rootWith(content), ".board-card");
    assert.deepEqual(calls, [{top: 540, left: 12, behavior: "smooth"}]);
});

test("a long behaviour group shows five complete rows, including a taller note", () => {
    const list = {children: [51, 53, 70, 54, 52, 58].map((height) => ({getBoundingClientRect: () => ({height})})), style: {}};
    limitBehaviourGroups({querySelectorAll: () => [list]});
    assert.equal(list.style.maxHeight, "280px");
});

test("behaviour group scroll survives a redraw and reveals a newly selected row", () => {
    const before = {dataset: {scrollKey: "behaviours:0:here"}, scrollTop: 170};
    const root = (list) => ({querySelectorAll: () => [list]});
    const positions = captureNestedScroll(root(before), "keys", "keys");
    const selected = {getBoundingClientRect: () => ({top: 330, bottom: 380})};
    const after = {dataset: before.dataset, scrollTop: 0, hasAttribute: () => true,
        querySelector: () => selected, getBoundingClientRect: () => ({top: 100, bottom: 350})};
    restoreNestedScroll(root(after), positions);
    assert.equal(after.scrollTop, 200, "the new row is brought into the five-row viewport");
    assert.equal(captureNestedScroll(root(before), "keys", "lighting").size, 0);
});
