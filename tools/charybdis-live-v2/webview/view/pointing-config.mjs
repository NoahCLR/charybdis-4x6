// A pointing slot as a record the keyboard stores, built from its form.
//
// Pure, so the posted record is testable: tests/edits.test.mjs stages what
// readConfig() returns against a real draft.

export const KIND = {DIRECTIONAL: 1, SCROLLING: 2};
export const DIRECTIONS = [["up", "Up"], ["left", "Left"], ["right", "Right"], ["down", "Down"]];
// Every scroll field the record holds.
export const SCROLL_FIELDS = [
    ["thresholdH", "Horizontal activation threshold"], ["thresholdV", "Vertical activation threshold"],
    ["divisorH", "Movement per wheel step ↔"], ["divisorV", "Movement per wheel step ↕"],
    ["intervalMs", "Minimum interval (ms)"], ["expireMs", "Gesture expiry (ms)"], ["lockMs", "Axis lock timeout (ms)"],
    ["startNumerator", "Axis selection ratio · numerator"], ["startDenominator", "Axis selection ratio · denominator"],
    ["sustainNumerator", "Axis retention ratio · numerator"], ["sustainDenominator", "Axis retention ratio · denominator"],
    ["decayDivisor", "Cross-axis decay divisor"],
];

// A slot switched to another kind of movement starts from the firmware's own
// shipped tuning for that kind — Dragscroll's scroll record and Volume's
// vertical threshold — wherever the stored record is still empty, so the first
// field changed posts a valid mode instead of an all-zero one.
export const SCROLL_STARTER = {thresholdH: 2, thresholdV: 3, divisorH: 6, divisorV: 8, intervalMs: 8, expireMs: 80, lockMs: 55,
    startNumerator: 7, startDenominator: 4, sustainNumerator: 5, sustainDenominator: 4, decayDivisor: 4, invert: 0};
export function startingRecord(slot, kind) {
    if (kind === KIND.SCROLLING && !SCROLL_FIELDS.some(([key]) => Number(slot.scroll?.[key]))) return {...slot, scroll: {...SCROLL_STARTER}};
    if (kind === KIND.DIRECTIONAL && !Number(slot.thresholdX) && !Number(slot.thresholdY)) return {...slot, thresholdX: 0, thresholdY: 60};
    return slot;
}
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
        buttons: (slot.buttons || []).map((button, index) => ({
            kind: form[`button:${index}:kind`] ? form[`button:${index}:kind`]() : button.kind,
            modifiers: form[`button:${index}:modifiers`] ? form[`button:${index}:modifiers`]() : button.modifiers,
            tap: {
                keycode: form[`button:${index}:tap`] ? form[`button:${index}:tap`]() || "0" : String(button.tap?.keycode ?? 0),
                modifierPolicy: button.tap?.modifierPolicy ?? 0,
                mask: button.tap?.mask ?? 0,
            },
        })),
    };
    if (kind === KIND.DIRECTIONAL) {
        config.axis = form.axis ? form.axis() : slot.axis;
        config.thresholdX = number(form.thresholdX?.(), slot.thresholdX);
        config.thresholdY = number(form.thresholdY?.(), slot.thresholdY);
        config.directions = Object.fromEntries(DIRECTIONS.map(([direction]) => [direction, {
            keycode: form[`dir:${direction}`] ? form[`dir:${direction}`]() || "0" : String(slot.directions?.[direction]?.keycode ?? 0),
            modifierPolicy: form[`dirPolicy:${direction}`] ? form[`dirPolicy:${direction}`]() : slot.directions?.[direction]?.modifierPolicy ?? 0,
            mask: form[`dirMask:${direction}`] ? form[`dirMask:${direction}`]() : slot.directions?.[direction]?.mask ?? 0,
        }]));
    } else {
        config.heldModifiers = form.heldModifiers ? form.heldModifiers() : slot.heldModifiers;
        config.scroll = Object.fromEntries(SCROLL_FIELDS.map(([key]) => [key,
            number(form[`scroll:${key}`]?.(), slot.scroll?.[key] ?? 0)]));
        config.scroll.invert = form.invert ? form.invert() : slot.scroll?.invert ?? 0;
    }
    return config;
}
