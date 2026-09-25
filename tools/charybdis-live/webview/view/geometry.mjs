// The physical keyboard: where each of the 56 positions sits, which LED it
// carries, and which half it belongs to. Lifted from the app's own board so a
// second interface cannot teach a different keyboard.

// The trackball is a 34mm sphere among 19.05mm key pitches, so it is nearly
// two keys across — drawn at its own size rather than as a marker, and sitting
// where the keyboard puts it: outboard of the thumb cluster, under the inner
// columns of the right half.
export const GEO = {
    keyW: 58, keyH: 58, radius: 7, yOffset: 54, rowStep: 64,
    viewBox: "20 96 1082 478",
    leftX: [36, 102, 168, 234, 300, 366], leftTopY: [118, 118, 78, 54, 78, 78],
    rightX: [698, 764, 830, 896, 962, 1028], rightTopY: [78, 78, 54, 78, 118, 118],
    thumbs: {
        48: {x: 328, y: 354, angle: 0}, 49: {x: 396, y: 350, angle: 10}, 50: {x: 468, y: 358, angle: 17},
        51: {x: 576, y: 358, angle: -17}, 52: {x: 648, y: 350, angle: -10}, 53: {x: 398, y: 432, angle: 9},
        54: {x: 468, y: 446, angle: 15}, 55: {x: 578, y: 432, angle: -15},
    },
    trackball: {x: 776, y: 480, r: 58},
};

export const LED_INDEX = {
    0: 0, 1: 7, 2: 8, 3: 15, 4: 16, 5: 20,
    12: 1, 13: 6, 14: 9, 15: 14, 16: 17, 17: 21,
    24: 2, 25: 5, 26: 10, 27: 13, 28: 18, 29: 22,
    36: 3, 37: 4, 38: 11, 39: 12, 40: 19, 41: 23,
    6: 49, 7: 45, 8: 44, 9: 37, 10: 36, 11: 29,
    18: 50, 19: 46, 20: 43, 21: 38, 22: 35, 23: 30,
    30: 51, 31: 47, 32: 42, 33: 39, 34: 34, 35: 31,
    42: 52, 43: 48, 44: 41, 45: 40, 46: 33, 47: 32,
    48: 26, 49: 27, 50: 28, 51: 53, 52: 54, 53: 25, 54: 24, 55: 55,
};

export const TRACKBALL_LED = 56;

const RIGHT_THUMBS = new Set([51, 52, 55]);

export const isRightHalf = (layoutIndex) =>
    layoutIndex < 48 ? layoutIndex % 12 >= 6 : RIGHT_THUMBS.has(layoutIndex);

// Where an overlay paints. RGB_KEY_HALF and RGB_KEYS_ONLY depend on the key
// that triggered the mode, so they are answered against that trigger.
export function inLocality(layoutIndex, locality, triggerIndex) {
    switch (locality) {
        case "RGB_LEFT_HALF": return !isRightHalf(layoutIndex);
        case "RGB_RIGHT_HALF": return isRightHalf(layoutIndex);
        case "RGB_KEY_HALF":
            return triggerIndex === undefined ? true : isRightHalf(layoutIndex) === isRightHalf(triggerIndex);
        case "RGB_KEYS_ONLY": return triggerIndex === undefined ? false : layoutIndex === triggerIndex;
        default: return true;
    }
}

export function keyVisual(layoutIndex) {
    const thumb = GEO.thumbs[layoutIndex];
    if (thumb) return {x: thumb.x, y: thumb.y + GEO.yOffset, angle: thumb.angle};
    const row = Math.floor(layoutIndex / 12), column = layoutIndex % 12;
    return column < 6
        ? {x: GEO.leftX[column], y: GEO.leftTopY[column] + row * GEO.rowStep + GEO.yOffset, angle: 0}
        : {x: GEO.rightX[column - 6], y: GEO.rightTopY[column - 6] + row * GEO.rowStep + GEO.yOffset, angle: 0};
}

// The trackball LED answers localities on its own terms: it is soldered on the
// right half, and no key maps to it, so a locality that names the trigger key
// never reaches it.
export function trackballInLocality(locality, triggerIndex) {
    switch (locality) {
        case "RGB_LEFT_HALF": return false;
        case "RGB_RIGHT_HALF": return true;
        case "RGB_KEY_HALF": return triggerIndex === undefined ? true : isRightHalf(triggerIndex);
        case "RGB_KEYS_ONLY": return false;
        default: return true;
    }
}

// ── the key face ────────────────────────────────────────────────────────
//
// Where the marks and legends sit on one cap. The rows are a designed set, not
// a running offset: how far the legend drops depends on how many rows sit above
// it, and each case was tuned against the others. Lifted from the app's own
// board, so the two cannot drift.
export function keyFaceRows(y, {tiers = false, combos = false, sub = false} = {}) {
    const rows = {comboHeight: 8.2};
    if (tiers && combos) { rows.tierY = y + 6.3; rows.comboY = y + 14.2; }
    else if (tiers) { rows.tierY = y + 7.5; }
    else if (combos) { rows.comboY = y + 7.2; }

    const above = (tiers ? 1 : 0) + (combos ? 1 : 0);
    if (sub) {
        rows.mainY = [y + 24.5, y + 29.3, y + 32.2][above];
        rows.separatorY = [y + 37.4, y + 40.8, y + 43.4][above];
        rows.subY = [y + 48.8, y + 51.8, y + 53.5][above];
    } else {
        rows.mainY = [y + GEO.keyH / 2, y + 36, y + 40.4][above];
    }
    return rows;
}

// Roughly how wide a legend runs, so a label can be sized to fit its row rather
// than bucketed by how many characters it happens to have.
const charWidth = (char) => {
    if (char === " ") return 0.32;
    if (/[.,:;!'|]/.test(char)) return 0.26;
    if (/[/\\()[\]{}]/.test(char)) return 0.34;
    if (/[MW@#%&]/.test(char)) return 0.82;
    if (/[A-Z0-9_]/.test(char)) return 0.62;
    if (/[a-z]/.test(char)) return 0.54;
    return 0.58;
};

/**
 * The size a legend takes to sit inside `width`, never above `max` and never
 * below `min`. `squeeze` is set when even the smallest size overruns, and is
 * the width to compress the glyphs into rather than let them spill off the cap.
 */
export function fitText(text, {max, min, width}) {
    const unit = [...String(text || "")].reduce((total, char) => total + charWidth(char), 0);
    const preferred = unit > 0 ? Math.min(max, width / unit) : max;
    return {size: Math.round(Math.max(min, preferred) * 100) / 100, squeeze: unit * max > width ? width : 0};
}
