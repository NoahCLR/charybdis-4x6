import assert from "node:assert/strict";
import test from "node:test";

import {captureContentScroll, captureKeysBenchHeight, revealSelectedContentRow, restoreContentScroll, restoreKeysBenchHeight, scrollContentTo} from "../webview/lib/scroll.mjs";

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
    assert.equal(captureKeysBenchHeight(previous, "keys", "keys", true), 0,
        "an explicit section toggle lets a collapsed workbench shrink");
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

test("a newly selected behaviour is revealed in the outer content scroller", () => {
    const selected = {getBoundingClientRect: () => ({top: 330, bottom: 380})};
    const content = {scrollTop: 170, querySelector: () => selected,
        getBoundingClientRect: () => ({top: 100, bottom: 350})};
    revealSelectedContentRow(rootWith(content));
    assert.equal(content.scrollTop, 212);
    selected.getBoundingClientRect = () => ({top: 90, bottom: 120});
    revealSelectedContentRow(rootWith(content));
    assert.equal(content.scrollTop, 190, "a selection above the viewport also uses the same scroller");
    assert.doesNotThrow(() => revealSelectedContentRow(rootWith(null)));
});
