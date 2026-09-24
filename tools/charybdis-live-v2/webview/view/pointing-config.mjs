// A pointing slot as a record the keyboard stores, built from its form.
//
// Pure, so the posted record is testable: tests/edits.test.mjs stages what
// readConfig() returns against a real draft.

export const KIND = {DIRECTIONAL: 1, SCROLLING: 2};
export const BUTTON = {PASS_THROUGH: 0, CONSUME: 1, TAP: 2, HOLD_MODIFIERS: 3};
export const DIRECTIONS = [["up", "Up"], ["left", "Left"], ["right", "Right"], ["down", "Down"]];
export const AXIS = {VERTICAL: 0, HORIZONTAL: 1, DOMINANT: 2, EIGHT: 3};
// Eight directions add the diagonals, stored beside the straight directions.
export const DIAGONALS = [["upLeft", "Up-left"], ["upRight", "Up-right"], ["downLeft", "Down-left"], ["downRight", "Down-right"]];
// What moving toward a direction with no shortcut does, in every directional
// mode. "Both" sends the two compass neighbours, 45 degrees either side, the
// one the movement leans toward first: a diagonal's straight directions, a
// straight direction's diagonals. Only eight
// directions has those, so elsewhere it acts as "its neighbours take over".
export const EMPTY_DIRECTION = [[0, "Its neighbours take over"], [1, "Send both neighbours"], [2, "Nothing"]];
// The directions each axis setting reads. The keyboard refuses a mode that
// keeps a shortcut or a threshold on an axis it does not read, so the form
// draws only these and the record carries the others as zero.
export const AXIS_DIRECTIONS = {
    [AXIS.VERTICAL]: ["up", "down"],
    [AXIS.HORIZONTAL]: ["left", "right"],
    [AXIS.DOMINANT]: ["up", "left", "right", "down"],
    [AXIS.EIGHT]: ["up", "left", "right", "down"],
};
export const axisReads = (axis) => AXIS_DIRECTIONS[axis] || AXIS_DIRECTIONS[AXIS.DOMINANT];
export const readsHorizontal = (axis) => axisReads(axis).includes("left");
export const readsVertical = (axis) => axisReads(axis).includes("up");
// Every scroll field the record holds.
export const SCROLL_FIELDS = [
    ["thresholdH", "Horizontal activation threshold"], ["thresholdV", "Vertical activation threshold"],
    ["divisorH", "Movement per wheel step ↔"], ["divisorV", "Movement per wheel step ↕"],
    ["intervalMs", "Minimum interval (ms)"], ["expireMs", "Gesture expiry (ms)"], ["lockMs", "Axis lock timeout (ms)"],
    ["startNumerator", "Axis selection ratio · numerator"], ["startDenominator", "Axis selection ratio · denominator"],
    ["sustainNumerator", "Axis retention ratio · numerator"], ["sustainDenominator", "Axis retention ratio · denominator"],
    ["decayDivisor", "Cross-axis decay divisor"],
];

// A pointing mode's pointer speed, as select options. The list is the app's
// one DPI list, which arrives with the model (core/model/pointer-dpi.js); a
// stored value outside it is shown in its place as stored, never replaced,
// because the firmware accepts it.
export function dpiOptions(choices, current) {
    const options = (choices || []).map(({value, label}) => [value, label]);
    const value = Number(current ?? 0);
    if (Number.isInteger(value) && !options.some(([choice]) => choice === value)) {
        const at = options.findIndex(([choice]) => choice > value);
        options.splice(at < 0 ? options.length : at, 0, [value, `${value} DPI · as stored`]);
    }
    return options;
}

// A slot switched to another kind of movement starts from the firmware's own
// shipped tuning for that kind — Dragscroll's scroll record and Volume's
// vertical threshold — wherever the stored record is still empty, so the first
// field changed posts a valid mode instead of an all-zero one.
export const SCROLL_STARTER = {thresholdH: 2, thresholdV: 3, divisorH: 6, divisorV: 8, intervalMs: 8, expireMs: 80, lockMs: 55,
    startNumerator: 7, startDenominator: 4, sustainNumerator: 5, sustainDenominator: 4, decayDivisor: 4, invert: 0};
export function startingRecord(slot, kind) {
    if (kind === KIND.SCROLLING && !SCROLL_FIELDS.some(([key]) => Number(slot.scroll?.[key]))) return {...slot, scroll: {...SCROLL_STARTER}};
    if (kind === KIND.DIRECTIONAL && !Number(slot.thresholdX) && !Number(slot.thresholdY)) return {...slot, thresholdX: 0, thresholdY: DIRECTIONAL_STARTER_THRESHOLD};
    return slot;
}
// A new mode for an empty slot, from the firmware's shipped tuning: a
// directional mode reads the dominant axis at Volume's threshold with no
// shortcuts yet, a scrolling mode starts from Dragscroll's record. It is named
// after its slot and posted like any edit, so the keyboard stores a valid mode
// straight away and every field can then be changed.
export function newMode(slot, kind) {
    const starter = kind === KIND.SCROLLING
        ? {...slot, kind, axis: 0, thresholdX: 0, thresholdY: 0, heldModifiers: 0, scroll: {...SCROLL_STARTER}}
        : {...slot, kind, axis: AXIS.DOMINANT, thresholdX: DIRECTIONAL_STARTER_THRESHOLD, thresholdY: DIRECTIONAL_STARTER_THRESHOLD, emptyDirection: 0};
    return readConfig(starter, {kind: () => kind, name: () => `Mode ${slot.id + 1}`, dpi: () => "0"});
}
// Volume's vertical threshold, the firmware's shipped tuning for a directional
// axis. An axis the stored record did not read has a zero threshold, which the
// keyboard refuses once the axis is read, so switching it on starts here.
export const DIRECTIONAL_STARTER_THRESHOLD = 60;
// The record posted for a slot. The slot is posted whole, with the fields the
// form owns replaced — the firmware stores one record, so half a record is
// never sent. `form` maps each drawn field to a reader; a field that is not
// drawn keeps the value the slot was read with.
export function readConfig(slot, form) {
    const number = (value, fallback) => {
        const text = String(value ?? "").trim();
        return /^\d+$/.test(text) ? Number(text) : fallback;
    };
    const kind = form.kind();
    const config = {
        id: slot.id,
        kind,
        name: form.name(),
        dpi: number(form.dpi(), slot.dpi),
        pointerLayer: form.pointerLayer ? form.pointerLayer() : slot.pointerLayer,
        buttons: (slot.buttons || []).map((button, index) => buttonRecord({
            kind: form[`button:${index}:kind`] ? form[`button:${index}:kind`]() : button.kind,
            modifiers: form[`button:${index}:modifiers`] ? form[`button:${index}:modifiers`]() : button.modifiers,
            tap: {...button.tap, keycode: form[`button:${index}:tap`] ? form[`button:${index}:tap`]() : button.tap?.keycode},
        })),
    };
    if (kind === KIND.DIRECTIONAL) {
        const axis = form.axis ? form.axis() : slot.axis;
        // A threshold is zero on an axis that is not read, and starts from the
        // shipped tuning on one that has just been switched on. A zero typed
        // into an axis that was already read is posted as typed.
        const threshold = (reads, wasRead, value) => {
            if (!reads) return 0;
            return !value && !wasRead ? DIRECTIONAL_STARTER_THRESHOLD : value;
        };
        config.axis = axis;
        config.thresholdX = threshold(readsHorizontal(axis), readsHorizontal(slot.axis), number(form.thresholdX?.(), slot.thresholdX));
        config.thresholdY = threshold(readsVertical(axis), readsVertical(slot.axis), number(form.thresholdY?.(), slot.thresholdY));
        const reads = axisReads(axis);
        config.directions = Object.fromEntries(DIRECTIONS.map(([direction]) => [direction, reads.includes(direction) ? {
            keycode: form[`dir:${direction}`] ? form[`dir:${direction}`]() || "0" : String(slot.directions?.[direction]?.keycode ?? 0),
            modifierPolicy: form[`dirPolicy:${direction}`] ? form[`dirPolicy:${direction}`]() : slot.directions?.[direction]?.modifierPolicy ?? 0,
            mask: form[`dirMask:${direction}`] ? form[`dirMask:${direction}`]() : slot.directions?.[direction]?.mask ?? 0,
        } : {keycode: "0", modifierPolicy: 0, mask: 0}]));
        // Diagonals exist only in eight-direction mode; any other axis
        // carries them as zero, which the keyboard requires.
        const eight = axis === AXIS.EIGHT;
        config.diagonals = Object.fromEntries(DIAGONALS.map(([diagonal]) => [diagonal, eight ? {
            keycode: form[`diag:${diagonal}`] ? form[`diag:${diagonal}`]() || "0" : String(slot.diagonals?.[diagonal]?.keycode ?? 0),
            modifierPolicy: slot.diagonals?.[diagonal]?.modifierPolicy ?? 0,
            mask: slot.diagonals?.[diagonal]?.mask ?? 0,
        } : {keycode: "0", modifierPolicy: 0, mask: 0}]));
        config.emptyDirection = form.emptyDirection ? form.emptyDirection() : slot.emptyDirection ?? 0;
    } else {
        config.heldModifiers = form.heldModifiers ? form.heldModifiers() : slot.heldModifiers;
        config.scroll = Object.fromEntries(SCROLL_FIELDS.map(([key]) => [key,
            number(form[`scroll:${key}`]?.(), slot.scroll?.[key] ?? 0)]));
        config.scroll.invert = form.invert ? form.invert() : slot.scroll?.invert ?? 0;
    }
    return config;
}

// One mouse-button override as the keyboard stores it: a shortcut only when
// it taps one, modifiers only when it holds them, and the rest zero, which
// the keyboard requires. A shortcut left behind by an earlier kind is dropped
// rather than refused.
export function buttonRecord({kind, modifiers, tap}) {
    return {
        kind,
        modifiers: kind === BUTTON.HOLD_MODIFIERS ? Number(modifiers) || 0 : 0,
        tap: kind === BUTTON.TAP
            ? {keycode: String(tap?.keycode ?? "").trim() || "0", modifierPolicy: tap?.modifierPolicy ?? 0, mask: tap?.mask ?? 0}
            : {keycode: "0", modifierPolicy: 0, mask: 0},
    };
}
// Whether the keyboard can store the override yet: tapping a shortcut needs
// the shortcut, holding modifiers needs at least one.
export function buttonReady(button) {
    if (button.kind === BUTTON.TAP) return String(button.tap?.keycode ?? "0") !== "0";
    if (button.kind === BUTTON.HOLD_MODIFIERS) return Number(button.modifiers) !== 0;
    return true;
}
// A record read from the form, split into what can be posted now and the
// button overrides still being set up. The kind and its field are chosen one
// after the other, and the keyboard refuses either alone, so an unfinished
// override posts as the slot stored it and is held by the editor until it is
// complete; the rest of the record still reaches the draft.
export function settleButtons(slot, config) {
    const pending = {};
    const buttons = config.buttons.map((button, index) => {
        if (buttonReady(button)) return button;
        pending[index] = button;
        return buttonRecord(slot.buttons[index]);
    });
    return {config: {...config, buttons}, pending};
}
