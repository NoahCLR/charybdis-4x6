import assert from "node:assert/strict";
import test from "node:test";

import {captureContentScroll, restoreContentScroll} from "../webview/lib/scroll.mjs";

const rootWith = (content) => ({querySelector: (selector) => selector === ".content" ? content : null});

test("rerendering a screen preserves its content scroll position", () => {
    const before = {scrollTop: 840, scrollLeft: 36};
    const scroll = captureContentScroll(rootWith(before), "lighting", "lighting");
    assert.deepEqual(scroll, {top: 840, left: 36});

    const after = {scrollTop: 0, scrollLeft: 0};
    restoreContentScroll(rootWith(after), scroll);
    assert.deepEqual(after, {scrollTop: 840, scrollLeft: 36});
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
