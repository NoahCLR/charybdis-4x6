// The physical keyboard: where each of the 56 positions sits, which LED it
// carries, and which half it belongs to. Lifted from the app's own board so a
// second interface cannot teach a different keyboard.

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
    trackball: {x: 690, y: 500, r: 26},
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
