// The webview keeps a few wire numbers of its own — pointing kinds, axes and
// button kinds, modifier bits — because its pure modules need them at import
// time and it may not import core/. They are the keyboard's numbers, so they
// are pinned here to the schema that defines them: a change there fails here,
// not on a keyboard.

import assert from "node:assert/strict";
import test from "node:test";
import {createRequire} from "node:module";
import {AXIS, BUTTON, KIND} from "../webview/view/pointing-config.mjs";
import {MODIFIER_BITS} from "../webview/view/keyvalues.mjs";
import {TIER_FIELDS} from "../webview/view/edits.mjs";

const require = createRequire(import.meta.url);
const pd = require("../core/schema/pd-mode-domain-v1");
const {VOCABULARY} = require("../core/model/vocabulary");

test("the webview's pointing numbers are the schema's", () => {
    assert.deepEqual(KIND, {DIRECTIONAL: pd.PD_KIND.DIRECTIONAL, SCROLLING: pd.PD_KIND.SCROLLING});
    assert.deepEqual(AXIS, {VERTICAL: pd.PD_AXIS.VERTICAL, HORIZONTAL: pd.PD_AXIS.HORIZONTAL, DOMINANT: pd.PD_AXIS.DOMINANT, EIGHT: pd.PD_AXIS.EIGHT});
    assert.deepEqual(BUTTON, {PASS_THROUGH: pd.PD_BUTTON.PASS_THROUGH, CONSUME: pd.PD_BUTTON.CONSUME, TAP: pd.PD_BUTTON.TAP, HOLD_MODIFIERS: pd.PD_BUTTON.HOLD_MODIFIERS});
});

test("the webview's modifier bits and names are the vocabulary's", () => {
    assert.deepEqual(MODIFIER_BITS, VOCABULARY.modifiers);
});

test("the webview's tiers are the vocabulary's", () => {
    assert.deepEqual(Object.keys(TIER_FIELDS).sort(), Object.keys(VOCABULARY.tiers).sort());
});
