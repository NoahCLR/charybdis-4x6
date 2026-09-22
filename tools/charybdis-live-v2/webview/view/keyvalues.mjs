// Numeric keycodes, named.
//
// Pointing-mode slots store their shortcuts as bare numbers, and the profile
// writer takes them back as names. This turns one into the other using the
// catalogue the keyboard itself reported, and falls back to the hex value
// rather than inventing a name it cannot prove.

const MOD_WRAPPERS_LEFT = [[1, "C"], [2, "S"], [4, "A"], [8, "G"]];
const MOD_WRAPPERS_RIGHT = [[1, "RCTL"], [2, "RSFT"], [4, "RALT"], [8, "RGUI"]];

export function keyName(model, code) {
    if (!code) return "";
    const direct = (value) => (model?.qmkKeycodes || []).find((entry) => entry.keycode === value)?.value;
    const exact = direct(code);
    if (exact) return exact;

    // Modifier-wrapped basic keycodes: 0x0100–0x1fff carries the held
    // modifiers in bits 8–12, and bit 12 means they are the right-hand ones.
    if (code >= 0x0100 && code <= 0x1fff) {
        const base = direct(code & 0xff);
        const bits = (code >> 8) & 0x1f;
        if (base && bits) {
            const wrappers = bits & 0x10 ? MOD_WRAPPERS_RIGHT : MOD_WRAPPERS_LEFT;
            return wrappers.filter(([bit]) => bits & bit).reduceRight((value, [, name]) => `${name}(${value})`, base);
        }
    }
    return `0x${code.toString(16).toUpperCase()}`;
}

export const MODIFIER_BITS = [
    [1, "Left Ctrl"], [2, "Left Shift"], [4, "Left Alt"], [8, "Left GUI"],
    [16, "Right Ctrl"], [32, "Right Shift"], [64, "Right Alt"], [128, "Right GUI"],
];

export const modifierNames = (mask) => MODIFIER_BITS.filter(([bit]) => mask & bit).map(([, name]) => name);
