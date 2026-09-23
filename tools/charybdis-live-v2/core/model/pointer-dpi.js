"use strict";

// Every pointer speed the app offers, in one list. The Charybdis sensor
// (PMW3360) takes CPI in steps of 100 and rounds anything else down, and the
// list stops at the top of the normal pointer range. A field the keyboard
// limits further offers the part of this list the keyboard accepts, never a
// list of its own.
const DPI_STEP = 100;
const DPI_MAX_CHOICE = 3400;
const POINTER_DPI = Object.freeze(Array.from({length: DPI_MAX_CHOICE / DPI_STEP}, (_, i) => (i + 1) * DPI_STEP));
// 0 is not a speed: a pointing mode stored with 0 keeps the normal pointer DPI.
const NORMAL_SPEED = Object.freeze({value: 0, label: "Normal pointer speed"});

function dpiChoices({accepts = () => true, normalSpeed = false} = {}) {
    return [...(normalSpeed ? [NORMAL_SPEED] : []), ...POINTER_DPI.filter(accepts).map(value => ({value, label: `${value} DPI`}))];
}

module.exports = {DPI_STEP, DPI_MAX_CHOICE, POINTER_DPI, dpiChoices};
